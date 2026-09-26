#include "kit_audio.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>

namespace pocket {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr size_t maxWavBytes = 64u << 20;

int fail(const std::string& error) {
    fprintf(stderr, "pocket kit: %s\n", error.c_str());
    return 1;
}

bool number(const std::string& s, double& value) {
    if (s.empty()) return false;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), value);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size() && std::isfinite(value);
}

bool bounded(double v, double lo, double hi) {
    return std::isfinite(v) && v >= lo && v <= hi;
}

void putLe(std::string& s, uint32_t v, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) s += static_cast<char>((v >> (8 * i)) & 255);
}

uint32_t le(std::string_view s, size_t p, unsigned bytes) {
    uint32_t n = 0;
    for (unsigned i = 0; i < bytes; ++i) n |= uint32_t(static_cast<unsigned char>(s[p + i])) << (8 * i);
    return n;
}

std::string wavHeader(size_t samples, int rate) {
    std::string out;
    out.reserve(44 + samples * 2);
    out = "RIFF";
    putLe(out, static_cast<uint32_t>(36 + samples * 2), 4);
    out += "WAVEfmt ";
    putLe(out, 16, 4); putLe(out, 1, 2); putLe(out, 1, 2);
    putLe(out, rate, 4); putLe(out, rate * 2, 4); putLe(out, 2, 2); putLe(out, 16, 2);
    out += "data";
    putLe(out, static_cast<uint32_t>(samples * 2), 4);
    return out;
}

void sample(std::string& out, double v) {
    const int n = static_cast<int>(std::lround(std::clamp(v, -1.0, 1.0) * 32767));
    putLe(out, static_cast<uint16_t>(n), 2);
}

// Attack/release are proportional when they overlap. Endpoints are exactly zero.
double envelope(size_t i, size_t count, double attack, double release, int rate) {
    const double a = std::max(1.0, attack * rate), r = std::max(1.0, release * rate);
    return std::min({1.0, i / a, (count - 1 - i) / r});
}

// PolyBLEP removes the step discontinuity of saw/square oscillators.
double blep(double phase, double step) {
    if (phase < step) { const double x = phase / step; return 2 * x - x * x - 1; }
    if (phase > 1 - step) { const double x = (phase - 1) / step; return x * x + 2 * x + 1; }
    return 0;
}

double oscillator(const std::string& wave, double phase, double step) {
    if (wave == "saw") return 2 * phase - 1 - blep(phase, step);
    if (wave == "square") return (phase < .5 ? 1 : -1) + blep(phase, step) - blep(std::fmod(phase + .5, 1.0), step);
    if (wave == "tri") return 1 - 4 * std::fabs(phase - .5);
    return std::sin(2 * pi * phase);
}

int writeWav(const std::string& path, const Result<std::string>& wav, int rate) {
    if (!wav.ok) return fail(wav.error);
    const auto saved = atomicWriteFile(path, wav.value);
    if (!saved.ok) return fail(saved.error);
    printf("%s: %.3fs mono PCM16 %d Hz\n", path.c_str(), (wav.value.size() - 44) / (2.0 * rate), rate);
    return 0;
}

double db(double x) { return x > 0 ? 20 * std::log10(x) : -120; }
}  // namespace

double noteFreq(const std::string& note) {
    double hz = 0;
    if (number(note, hz)) return hz > 0 && hz < 30000 ? hz : -1;
    if (note.size() < 2 || note.size() > 5) return -1;
    const char n = note[0] >= 'a' && note[0] <= 'g' ? note[0] - 'a' + 'A' : note[0];
    if (n < 'A' || n > 'G') return -1;
    static constexpr int semis[] = {9, 11, 0, 2, 4, 5, 7};
    int semi = semis[n - 'A'];
    size_t p = 1;
    if (note[p] == '#') ++semi, ++p;
    else if (note[p] == 'b') --semi, ++p;
    int octave = 0;
    const auto r = std::from_chars(note.data() + p, note.data() + note.size(), octave);
    if (r.ec != std::errc{} || r.ptr != note.data() + note.size() || octave < -1 || octave > 9) return -1;
    return 440 * std::exp2(((octave + 1) * 12 + semi - 69) / 12.0);
}

Result<std::string> synthNotes(const std::string& notes, const std::string& wave, double bpm, double gain, int rate) {
    using R = Result<std::string>;
    if (rate < 8000 || rate > 96000 || !bounded(gain, 0, 1) || !bounded(bpm, 0, 1000))
        return R::Err("rate must be 8000..96000, gain 0..1, bpm 0..1000");
    if (wave != "sine" && wave != "square" && wave != "saw" && wave != "tri")
        return R::Err("wave must be sine, square, saw or tri");
    if (notes.empty() || notes.size() > 65536) return R::Err("note sequence must contain 1..65536 bytes");
    struct Event { double frequency; size_t count; };
    std::vector<Event> events;
    size_t total = 0;
    for (size_t p = 0; p < notes.size();) {
        p = notes.find_first_not_of(" ,\t\r\n", p);
        if (p == std::string::npos) break;
        size_t end = notes.find_first_of(" ,\t\r\n", p);
        if (end == std::string::npos) end = notes.size();
        const std::string event = notes.substr(p, end - p);
        p = end;
        const size_t colon = event.find(':');
        const std::string note = event.substr(0, colon);
        double duration = .25;
        if (colon != std::string::npos && !number(event.substr(colon + 1), duration))
            return R::Err("invalid duration in event: " + event);
        if (bpm > 0) duration *= 60 / bpm;
        const double freq = note == "R" || note == "r" ? 0 : noteFreq(note);
        if (!bounded(duration, 1.0 / rate, 60) || freq < 0 || freq >= rate * .5)
            return R::Err("invalid event (duration >=1 sample, <=60s; frequency below Nyquist): " + event);
        const size_t count = static_cast<size_t>(std::llround(duration * rate));
        if (events.size() >= 4096 || count > size_t(rate) * 60 - total)
            return R::Err("note sequence exceeds 4096 events or 60 seconds");
        events.push_back({freq, count});
        total += count;
    }
    if (events.empty()) return R::Err("note sequence is empty");
    std::string out = wavHeader(total, rate);
    double phase = 0;
    for (const Event& event : events) {
        const double step = event.frequency / rate;
        for (size_t i = 0; i < event.count; ++i) {
            const double v = step > 0 ? oscillator(wave, phase, step) : 0;
            sample(out, v * envelope(i, event.count, .005, .03, rate) * gain);
            phase += step;
            phase -= std::floor(phase);
        }
    }
    return R::Ok(std::move(out));
}

Result<SoundSpec> soundPreset(const std::string& name) {
    SoundSpec s;
    if (name == "click") { s.duration = .045; s.frequency = 1400; s.endFrequency = 600; s.noise = .35; s.attack = .001; s.release = .04; }
    else if (name == "chime") { s.duration = .8; s.frequency = s.endFrequency = 880; s.harmonics = .35; s.release = .75; }
    else if (name == "laser") { s.duration = .35; s.frequency = 1800; s.endFrequency = 120; s.release = .32; }
    else if (name == "whoosh") { s.duration = .6; s.noise = 1; s.attack = .3; s.release = .3; s.lowpass = 3500; }
    else if (name == "impact") { s.duration = .45; s.frequency = 150; s.endFrequency = 35; s.noise = .35; s.attack = .001; s.release = .43; s.lowpass = 1800; }
    else if (name == "tone") {}
    else if (name == "noise") { s.noise = 1; }
    else return Result<SoundSpec>::Err("preset must be click, chime, laser, whoosh, impact, tone or noise");
    return Result<SoundSpec>::Ok(s);
}

Result<std::string> synthSound(const SoundSpec& s) {
    using R = Result<std::string>;
    if (s.rate < 8000 || s.rate > 96000 || !bounded(s.duration, .001, 60) || !bounded(s.gain, 0, 1) ||
        !bounded(s.frequency, 1, s.rate * .49) || !bounded(s.endFrequency, 1, s.rate * .49) ||
        !bounded(s.noise, 0, 1) || !bounded(s.harmonics, 0, 1) || !bounded(s.attack, 0, 60) ||
        !bounded(s.release, 0, 60) || !bounded(s.lowpass, 0, s.rate * .49))
        return R::Err("invalid sound: duration .001..60s, rate 8000..96000, gain/noise 0..1, frequencies 1..<Nyquist, attack/release 0..60s");
    const size_t count = static_cast<size_t>(std::llround(s.duration * s.rate));
    std::string out = wavHeader(count, s.rate);
    // Exponential chirp, with fixed-cost recurrence per sample.
    const double sweep = std::pow(s.endFrequency / s.frequency, 1.0 / std::max<size_t>(1, count - 1));
    const double alpha = s.lowpass > 0 ? 1 - std::exp(-2 * pi * s.lowpass / s.rate) : 1;
    double frequency = s.frequency, phase = 0, filtered = 0;
    uint32_t rng = s.seed;  // full 32-bit seed range, including zero
    for (size_t i = 0; i < count; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const double noise = double(rng >> 8) / 8388607.5 - 1;
        double tone = std::sin(2 * pi * phase);
        if (s.harmonics > 0) {
            const double decay = std::exp(-4.0 * i / count);
            // Omit above-Nyquist partials when callers raise the pitch.
            double bell = 0, weight = 0;
            if (frequency * 2 < s.rate * .49) bell += std::sin(4 * pi * phase), weight += 1;
            if (frequency * 3 < s.rate * .49) bell += .5 * std::sin(6 * pi * phase), weight += .5;
            tone = (1 - s.harmonics) * tone + s.harmonics * (weight ? bell / weight : 0) * decay;
        }
        const double mixed = (1 - s.noise) * tone + s.noise * noise;
        filtered += alpha * (mixed - filtered);
        sample(out, filtered * s.gain * envelope(i, count, s.attack, s.release, s.rate));
        phase += frequency / s.rate;
        phase -= std::floor(phase);
        frequency *= sweep;
    }
    return R::Ok(std::move(out));
}

Result<AudioStats> analyzeWav(std::string_view d) {
    using R = Result<AudioStats>;
    if (d.size() > maxWavBytes) return R::Err("WAV exceeds 64 MiB analysis limit");
    if (d.size() < 12 || d.substr(0, 4) != "RIFF" || d.substr(8, 4) != "WAVE")
        return R::Err("need RIFF/WAVE (convert other formats with ffmpeg)");
    const uint32_t riffSize = le(d, 4, 4);
    if (riffSize < 4 || riffSize > d.size() - 8) return R::Err("truncated RIFF/WAVE");
    const size_t end = size_t(riffSize) + 8;
    size_t data = 0, length = 0;
    unsigned format = 0, align = 0, byteRate = 0;
    bool haveFmt = false, haveData = false;
    AudioStats a;
    for (size_t p = 12; p < end;) {
        if (end - p < 8) return R::Err("truncated WAV chunk header");
        const size_t size = le(d, p + 4, 4), start = p + 8;
        if (size > end - start || (size & 1u) > end - start - size) return R::Err("truncated WAV chunk or padding");
        const auto tag = d.substr(p, 4);
        if (tag == "fmt ") {
            if (haveFmt || size < 16) return R::Err("invalid or duplicate WAV format chunk");
            haveFmt = true;
            format = le(d, start, 2); a.channels = le(d, start + 2, 2); a.rate = le(d, start + 4, 4);
            byteRate = le(d, start + 8, 4); align = le(d, start + 12, 2); a.bits = le(d, start + 14, 2);
            if (format == 0xfffe) {
                static constexpr char guidTail[] = "\0\0\0\0\x10\0\x80\0\0\xaa\0\x38\x9b\x71";
                if (size < 40 || le(d, start + 16, 2) < 22 || size_t(le(d, start + 16, 2)) > size - 18 ||
                    le(d, start + 18, 2) != unsigned(a.bits) || d.substr(start + 26, 14) != std::string_view(guidTail, 14))
                    return R::Err("unsupported extensible WAV format");
                format = le(d, start + 24, 2);
            }
        } else if (tag == "data") {
            if (haveData) return R::Err("multiple WAV data chunks are unsupported");
            haveData = true; data = start; length = size;
        }
        p = start + size + (size & 1u);
    }
    if (!haveFmt || !haveData || a.channels < 1 || a.channels > 32 || a.rate < 8000 || a.rate > 192000 ||
        !((format == 1 && (a.bits == 8 || a.bits == 16 || a.bits == 24 || a.bits == 32)) || (format == 3 && a.bits == 32)))
        return R::Err("need PCM8/16/24/32 or float32 WAV, 1..32 channels, 8000..192000 Hz");
    const size_t bytes = a.bits / 8;
    if (align != bytes * a.channels || byteRate != uint32_t(a.rate) * align || length % align)
        return R::Err("invalid WAV frame alignment or byte rate");
    a.frames = length / align;
    auto readSample = [&](size_t frame, int channel) {
        const uint32_t raw = le(d, data + frame * align + channel * bytes, bytes);
        if (format == 3) return double(std::bit_cast<float>(raw));
        if (a.bits == 8) return (double(raw) - 128) / 128;
        const int64_t signedValue = raw & (uint32_t(1) << (a.bits - 1)) ? int64_t(raw) - (int64_t(1) << a.bits) : raw;
        return double(signedValue) / double(int64_t(1) << (a.bits - 1));
    };
    double sum = 0, silentEnergy = 0, bestEnergy = 0;
    const size_t silentBlock = a.rate / 10, pitchStep = std::max(1, a.rate / 24000);
    const size_t pitchBlock = 4096 * pitchStep;
    size_t silentCount = 0, bestStart = 0, blockStart = 0;
    int bestChannel = 0;
    std::array<double, 32> channelEnergy{};
    for (size_t i = 0; i < a.frames; ++i) {
        double frameEnergy = 0;
        for (int c = 0; c < a.channels; ++c) {
            const double v = readSample(i, c), abs = std::fabs(v);
            if (!std::isfinite(v) || abs > 1e6) return R::Err("WAV contains non-finite or excessive float samples");
            a.peak = std::max(a.peak, abs);
            if (format == 3 ? abs >= 1 : v <= -1 || v >= 1 - std::ldexp(1.0, 1 - a.bits)) ++a.clippedSamples;
            frameEnergy += v * v;
            channelEnergy[c] += v * v;
        }
        sum += frameEnergy;
        silentEnergy += frameEnergy / a.channels;
        ++silentCount;
        if (silentCount == silentBlock || i + 1 == a.frames) {
            if (silentEnergy / silentCount < 1e-5) a.silenceSeconds += double(silentCount) / a.rate;
            silentCount = 0; silentEnergy = 0;
        }
        if (i + 1 - blockStart == pitchBlock || i + 1 == a.frames) {
            // Only complete windows compete, except for files shorter than one window.
            if (i + 1 - blockStart == pitchBlock || a.frames < pitchBlock) {
                for (int c = 0; c < a.channels; ++c) {
                    if (channelEnergy[c] > bestEnergy) bestEnergy = channelEnergy[c], bestStart = blockStart, bestChannel = c;
                }
            }
            channelEnergy.fill(0); blockStart = i + 1;
        }
    }
    a.rms = a.frames ? std::sqrt(sum / (a.frames * a.channels)) : 0;
    const size_t window = std::min<size_t>(4096, (a.frames - bestStart) / pitchStep);
    if (window >= 32 && bestEnergy > 1e-8) {
        std::array<double, 4096> x{};
        double mean = 0;
        for (size_t i = 0; i < window; ++i) mean += x[i] = readSample(bestStart + i * pitchStep, bestChannel);
        mean /= window;
        for (size_t i = 0; i < window; ++i) x[i] -= mean;
        const double effectiveRate = double(a.rate) / pitchStep;
        const size_t lo = std::max<size_t>(1, size_t(effectiveRate / 2000) - 1);
        const size_t hi = std::min<size_t>(window / 2, effectiveRate / 50) + 1;
        double prev2 = -1, prev = -1;
        for (size_t lag = lo; lag <= hi; ++lag) {
            double cross = 0, aa = 0, bb = 0;
            for (size_t i = 0; i + lag < window; ++i) {
                cross += x[i] * x[i + lag]; aa += x[i] * x[i]; bb += x[i + lag] * x[i + lag];
            }
            const double corr = aa * bb > 0 ? cross / std::sqrt(aa * bb) : 0;
            if (lag > lo + 1 && prev > .75 && prev > prev2 && prev >= corr) {
                const double denominator = prev2 - 2 * prev + corr;
                const double offset = denominator ? .5 * (prev2 - corr) / denominator : 0;
                a.pitchHz = effectiveRate / (double(lag - 1) + std::clamp(offset, -.5, .5));
                break;
            }
            prev2 = prev; prev = corr;
        }
    }
    return R::Ok(a);
}

int kitWav(const std::vector<std::string>& a) {
    if (a.size() < 2 || (a.size() - 2) % 2)
        return fail("usage: kit wav OUT.wav \"C4:.25 E4:.25 R:.25 440:.5\" [--wave sine|square|saw|tri] [--bpm 1..1000] [--gain 0..1]");
    std::string wave = "sine";
    double bpm = 0, gain = .3;
    for (size_t i = 2; i < a.size(); i += 2) {
        if (a[i] == "--wave") wave = a[i + 1];
        else if (a[i] == "--bpm") { if (!number(a[i + 1], bpm) || bpm < 1 || bpm > 1000) return fail("bpm must be 1..1000"); }
        else if (a[i] == "--gain") { if (!number(a[i + 1], gain)) return fail("invalid gain"); }
        else return fail("unknown wav option: " + a[i]);
    }
    return writeWav(a[0], synthNotes(a[1], wave, bpm, gain), 44100);
}

int kitSfx(const std::vector<std::string>& a) {
    constexpr const char* usage = "usage: kit sfx OUT.wav click|chime|laser|whoosh|impact|tone|noise [--duration SEC] [--freq HZ] [--end-freq HZ] [--gain 0..1] [--noise 0..1] [--attack SEC] [--release SEC] [--lowpass HZ] [--seed UINT32] [--rate 8000..96000]";
    if (a.size() == 1 && a[0] == "--help") { puts(usage); return 0; }
    if (a.size() < 2 || (a.size() - 2) % 2) return fail(usage);
    auto preset = soundPreset(a[1]);
    if (!preset.ok) return fail(preset.error);
    SoundSpec& s = preset.value;
    bool endSet = false;
    for (size_t i = 2; i < a.size(); i += 2) {
        double value = 0;
        if (!number(a[i + 1], value)) return fail("invalid number for " + a[i]);
        if (a[i] == "--duration") s.duration = value;
        else if (a[i] == "--freq") { s.frequency = value; if (!endSet) s.endFrequency = value; }
        else if (a[i] == "--end-freq") s.endFrequency = value, endSet = true;
        else if (a[i] == "--gain") s.gain = value;
        else if (a[i] == "--noise") s.noise = value;
        else if (a[i] == "--attack") s.attack = value;
        else if (a[i] == "--release") s.release = value;
        else if (a[i] == "--lowpass") s.lowpass = value;
        else if (a[i] == "--seed") {
            if (!bounded(value, 0, std::numeric_limits<uint32_t>::max()) || std::floor(value) != value) return fail("seed must be an unsigned 32-bit integer");
            s.seed = static_cast<uint32_t>(value);
        } else if (a[i] == "--rate") {
            if (!bounded(value, 8000, 96000) || std::floor(value) != value) return fail("rate must be an integer 8000..96000");
            s.rate = static_cast<int>(value);
        } else return fail("unknown sfx option: " + a[i]);
    }
    return writeWav(a[0], synthSound(s), s.rate);
}

int kitAudio(const std::vector<std::string>& a) {
    if (a.size() != 1) return fail("usage: kit audio FILE.wav");
    const auto file = readFileBounded(a[0], maxWavBytes);
    if (!file.ok) return fail(file.error);
    const auto stats = analyzeWav(file.value);
    if (!stats.ok) return fail(stats.error);
    const AudioStats& s = stats.value;
    printf("%s: %.3fs, %d ch, %d Hz, %d-bit\npeak %.2f dBFS · rms %.2f dBFS · clipped samples %zu\n"
           "periodic pitch ~%.1f Hz · silence %.3fs\n",
           a[0].c_str(), double(s.frames) / s.rate, s.channels, s.rate, s.bits,
           db(s.peak), db(s.rms), s.clippedSamples, s.pitchHz, s.silenceSeconds);
    return 0;
}
}  // namespace pocket
