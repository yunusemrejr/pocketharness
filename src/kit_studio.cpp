#include "kit_studio.h"

#include <unistd.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>

#include "process.h"

namespace pocket {
namespace {

constexpr double pi = 3.14159265358979323846;
constexpr int R = kStudioRate;
constexpr size_t maxSeconds = 1200;

int fail(const std::string& error) {
    fprintf(stderr, "pocket kit: %s\n", error.c_str());
    return 1;
}
bool number(const std::string& s, double& v) {
    if (s.empty()) return false;
    auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size() && std::isfinite(v);
}
double dbToGain(double db) { return std::pow(10.0, db / 20); }
double gainToDb(double g) { return g > 1e-9 ? 20 * std::log10(g) : -180; }
uint32_t le(std::string_view s, size_t p, unsigned bytes) {
    uint32_t n = 0;
    for (unsigned i = 0; i < bytes && p + i < s.size(); ++i) n |= uint32_t((unsigned char)s[p + i]) << (8 * i);
    return n;
}

// ---- decoding --------------------------------------------------------------
Result<Stereo> decodeWav(std::string_view d) {
    using Res = Result<Stereo>;
    if (d.size() < 12 || d.substr(0, 4) != "RIFF" || d.substr(8, 4) != "WAVE") return Res::Err("not a RIFF/WAVE file");
    const size_t riffSize = le(d, 4, 4);
    if (riffSize < 4 || riffSize > d.size() - 8) return Res::Err("truncated RIFF/WAVE");
    const size_t end = riffSize + 8;
    unsigned format = 0, channels = 0, rate = 0, bits = 0, align = 0, byteRate = 0;
    size_t data = 0, length = 0;
    bool haveFmt = false, haveData = false;
    for (size_t p = 12; p < end;) {
        if (end - p < 8) return Res::Err("truncated WAV chunk header");
        size_t size = le(d, p + 4, 4), start = p + 8;
        if (size > end - start || (size & 1) > end - start - size) return Res::Err("truncated WAV chunk or padding");
        std::string_view tag = d.substr(p, 4);
        if (tag == "fmt ") {
            if (haveFmt || size < 16) return Res::Err("invalid or duplicate WAV format chunk");
            haveFmt = true;
            format = le(d, start, 2); channels = le(d, start + 2, 2); rate = le(d, start + 4, 4); bits = le(d, start + 14, 2);
            byteRate = le(d, start + 8, 4); align = le(d, start + 12, 2);
            if (format == 0xfffe) {
                static constexpr char guidTail[] = "\0\0\0\0\x10\0\x80\0\0\xaa\0\x38\x9b\x71";
                if (size < 40 || le(d, start + 16, 2) < 22 || size_t(le(d, start + 16, 2)) > size - 18 ||
                    le(d, start + 18, 2) != bits || d.substr(start + 26, 14) != std::string_view(guidTail, 14))
                    return Res::Err("invalid extensible WAV format");
                format = le(d, start + 24, 2);
            }
        } else if (tag == "data") {
            if (haveData) return Res::Err("multiple WAV data chunks are unsupported");
            haveData = true; data = start; length = size;
        }
        p = start + size + (size & 1);
    }
    if (!haveFmt || !haveData || channels < 1 || channels > 32 || rate < 8000 || rate > 192000)
        return Res::Err("invalid WAV (need 1..32 channels, 8000..192000 Hz)");
    if (!((format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32)))
        return Res::Err("unsupported WAV (need PCM 8/16/24/32 or float32)");
    const size_t bytes = bits / 8;
    if (align != bytes * channels || byteRate != rate * align || length % align)
        return Res::Err("invalid WAV frame alignment or byte rate");
    const size_t frames = length / align;
    if (!frames || frames > maxSeconds * size_t(rate)) return Res::Err("audio length must be greater than zero and at most 1200 seconds");
    // A surround mix needs its channel layout to downmix correctly; do not silently discard centre/dialogue channels.
    if (channels > 2) return Res::Err("unsupported WAV (multichannel audio requires FFmpeg for a stereo downmix)");
    std::vector<float> a(frames), b(frames);
    for (size_t i = 0; i < frames; ++i)
        for (unsigned c = 0; c < std::min(2u, channels); ++c) {
            uint32_t raw = le(d, data + (i * channels + c) * bytes, bytes);
            double v;
            if (format == 3) { float f; memcpy(&f, &raw, 4); v = f; }
            else if (bits == 8) v = (double(raw) - 128) / 128;
            else v = double(int32_t(raw << (32 - bits))) / 2147483648.0;
            if (!std::isfinite(v)) return Res::Err("WAV contains a non-finite sample");
            (c ? b : a)[i] = float(v);
        }
    if (channels == 1) b = a;
    Stereo out;
    if (rate == (unsigned)R) { out.l = std::move(a); out.r = std::move(b); return Res::Ok(std::move(out)); }
    const double step = double(rate) / R;
    const size_t count = size_t(double(frames) / step);
    out.l.resize(count); out.r.resize(count);
    if (rate > (unsigned)R) {
        // Windowed-sinc low-pass before decimation. Cubic interpolation alone aliases
        // frequencies above 24 kHz into the audible band. Cache fractional phases;
        // no transcendental functions run per output sample.
        constexpr size_t phases = 1024;
        const int radius = int(std::ceil(16 * step)), taps = radius * 2;
        const double cutoff = .95 / step;
        std::vector<std::vector<double>> kernels(phases, std::vector<double>(taps));
        for (size_t ph = 0; ph < phases; ++ph) {
            double sum = 0;
            for (int j = 0; j < taps; ++j) {
                const double x = j - radius + 1 - double(ph) / phases;
                const double sinc = std::fabs(x) < 1e-12 ? cutoff : std::sin(pi * cutoff * x) / (pi * x);
                kernels[ph][j] = sinc * (.5 + .5 * std::cos(pi * x / radius));
                sum += kernels[ph][j];
            }
            for (double& v : kernels[ph]) v /= sum;
        }
        for (size_t i = 0; i < count; ++i) {
            const double pos = i * step;
            const long centre = long(pos);
            const auto& k = kernels[std::min(phases - 1, size_t((pos - centre) * phases))];
            double l = 0, r = 0;
            for (int j = 0; j < taps; ++j) {
                const size_t at = size_t(std::clamp<long>(centre + j - radius + 1, 0, long(frames) - 1));
                l += a[at] * k[j]; r += b[at] * k[j];
            }
            out.l[i] = float(l); out.r[i] = float(r);
        }
        return Res::Ok(std::move(out));
    }
    // Catmull-Rom interpolation for upsampling, where there is no decimation alias.
    auto cubic = [&](const std::vector<float>& x, double pos) {
        long i = long(pos);
        double t = pos - i;
        auto at = [&](long k) { return double(x[std::clamp<long>(k, 0, long(frames) - 1)]); };
        double p0 = at(i - 1), p1 = at(i), p2 = at(i + 1), p3 = at(i + 2);
        return float(p1 + .5 * t * (p2 - p0 + t * (2 * p0 - 5 * p1 + 4 * p2 - p3 + t * (3 * (p1 - p2) + p3 - p0))));
    };
    for (size_t i = 0; i < count; ++i) out.l[i] = cubic(a, i * step), out.r[i] = cubic(b, i * step);
    return Res::Ok(std::move(out));
}

struct Biquad {
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    double p(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

}  // namespace

Result<Stereo> loadAudio(const std::string& path) {
    auto file = readFileBounded(path, 256u << 20);
    if (!file.ok) return Result<Stereo>::Err(path + ": " + file.error);
    if (file.value.size() > 12 && file.value.compare(0, 4, "RIFF") == 0) {
        auto w = decodeWav(file.value);
        if (w.ok) return w;
        if (!startsWith(w.error, "unsupported WAV")) return Result<Stereo>::Err(path + ": " + w.error);
    }
    std::string ffmpeg = whichExe("ffmpeg");
    if (ffmpeg.empty()) return Result<Stereo>::Err(path + ": not a plain WAV and FFmpeg is not installed to decode it");
    std::string tmp = "/tmp/pocket-audio-XXXXXX";
    int fd = mkstemp(tmp.data());
    if (fd < 0) return Result<Stereo>::Err("cannot create a temporary file");
    close(fd);
    SpawnOpts o;
    o.exe = ffmpeg;
    o.argv = {ffmpeg, "-hide_banner", "-loglevel", "error", "-nostdin", "-y", "-i", path, "-vn", "-t", std::to_string(maxSeconds + 1), "-ac", "2", "-ar",
              std::to_string(R), "-c:a", "pcm_s16le", "-f", "wav", tmp};
    o.timeoutMs = 300000;
    SpawnResult sr = spawn(o);
    auto decoded = sr.ok && sr.exitCode == 0 ? readFileBounded(tmp, 512u << 20) : Result<std::string>::Err(sr.err.substr(0, 300));
    unlink(tmp.c_str());
    if (!decoded.ok) return Result<Stereo>::Err(path + ": cannot decode audio: " + decoded.error);
    return decodeWav(decoded.value);
}

Result<void> saveStereoWav(const std::string& path, const Stereo& s) {
    if (s.l.size() != s.r.size() || s.size() > maxSeconds * size_t(R))
        return Result<void>::Err("stereo channels must have equal lengths, at most 1200 seconds");
    auto put = [](std::string& o, uint32_t v, int n) { for (int i = 0; i < n; ++i) o += char((v >> (8 * i)) & 255); };
    std::string o = "RIFF";
    put(o, uint32_t(36 + s.size() * 4), 4);
    o += "WAVEfmt ";
    put(o, 16, 4); put(o, 1, 2); put(o, 2, 2); put(o, R, 4); put(o, R * 4, 4); put(o, 4, 2); put(o, 16, 2);
    o += "data";
    put(o, uint32_t(s.size() * 4), 4);
    o.reserve(44 + s.size() * 4);
    for (size_t i = 0; i < s.size(); ++i)
        for (float v : {s.l[i], s.r[i]}) {
            if (!std::isfinite(v)) return Result<void>::Err("audio contains a non-finite sample");
            put(o, uint16_t(int16_t(std::lround(std::clamp(v, -1.f, 1.f) * 32767))), 2);
        }
    auto saved = writeOutputFile(path, o);
    return saved.ok ? Result<void>::Ok() : Result<void>::Err(saved.error);
}

double integratedLufs(const Stereo& s) {
    // BS.1770-4 K-weighting for 48 kHz: high-shelf then RLB high-pass.
    Biquad sh[2] = {{1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585},
                    {1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585}};
    Biquad hp[2] = {{1, -2, 1, -1.99004745483398, 0.99007225036621}, {1, -2, 1, -1.99004745483398, 0.99007225036621}};
    const size_t hop = R / 10;
    std::vector<double> sub;
    double acc = 0;
    size_t in = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        double a = hp[0].p(sh[0].p(s.l[i])), b = hp[1].p(sh[1].p(s.r[i]));
        acc += a * a + b * b;
        if (++in == hop) { sub.push_back(acc / hop); acc = 0; in = 0; }
    }
    std::vector<double> blocks;
    for (size_t i = 0; i + 4 <= sub.size(); ++i) blocks.push_back((sub[i] + sub[i + 1] + sub[i + 2] + sub[i + 3]) / 4);
    // A short cue cannot fill the standard 400 ms window. Estimate its loudness
    // from the available K-weighted samples instead of treating audible sound as silence.
    if (s.size() && s.size() < 4 * hop) {
        double energy = acc;
        for (double z : sub) energy += z * hop;
        blocks.push_back(energy / s.size());
    }
    auto lufs = [](double z) { return -0.691 + 10 * std::log10(z); };
    double sum = 0;
    size_t n = 0;
    for (double z : blocks) if (z > 0 && lufs(z) > -70) sum += z, ++n;
    if (!n) return -1000;
    const double gate = lufs(sum / n) - 10;
    sum = 0; n = 0;
    for (double z : blocks) if (z > 0 && lufs(z) > -70 && lufs(z) > gate) sum += z, ++n;
    return n ? lufs(sum / n) : -1000;
}

double peakDb(const Stereo& s) {
    float p = 0;
    for (size_t i = 0; i < s.size(); ++i) p = std::max({p, std::fabs(s.l[i]), std::fabs(s.r[i])});
    return gainToDb(p);
}

void limitPeak(Stereo& s, double ceilingDb) {
    const double ceil = dbToGain(ceilingDb);
    const size_t n = s.size(), half = R / 200;  // 5 ms look-ahead each side
    if (!n) return;
    std::vector<float> need(n, 1.f);
    bool any = false;
    for (size_t i = 0; i < n; ++i) {
        double p = std::max(std::fabs(s.l[i]), std::fabs(s.r[i]));
        if (p > ceil) need[i] = float(ceil / p), any = true;
    }
    if (!any) return;
    // Sliding minimum over [i-half, i+half], then a box smoother of the same reach: gain <= need everywhere.
    std::deque<size_t> q;
    std::vector<float> low(n);
    for (size_t i = 0, j = 0; i < n; ++i) {
        for (; j < std::min(n, i + half + 1); ++j) {
            while (!q.empty() && need[q.back()] >= need[j]) q.pop_back();
            q.push_back(j);
        }
        while (q.front() + half < i) q.pop_front();
        low[i] = need[q.front()];
    }
    const size_t w = half + 1;
    // Keep a running window rather than two extra full-programme buffers.
    size_t left = 0, right = 0;
    double sum = 0;
    for (size_t i = 0; i < n; ++i) {
        size_t a = i >= w / 2 ? i - w / 2 : 0, b = std::min(n, i + w / 2 + 1);
        for (; right < b; ++right) sum += low[right];
        for (; left < a; ++left) sum -= low[left];
        float g = std::min(float(sum / (b - a)), need[i]);
        s.l[i] *= g; s.r[i] *= g;
    }
}

namespace {

// ---- music -----------------------------------------------------------------
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t x) : s(x ? x : 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    double f() { return next() / 4294967296.0; }
    int pick(int n) { return int(next() % uint32_t(n)); }
};

double saw(double ph, double dt) {  // PolyBLEP saw
    double v = 2 * ph - 1;
    if (ph < dt) { double x = ph / dt; v -= 2 * x - x * x - 1; }
    else if (ph > 1 - dt) { double x = (ph - 1) / dt; v -= x * x + 2 * x + 1; }
    return v;
}
double mtof(double m) { return 440 * std::exp2((m - 69) / 12); }

struct Comb {
    std::vector<float> b; size_t i = 0; float lp = 0;
    float p(float x, float fb, float damp) { float o = b[i]; lp = o * (1 - damp) + lp * damp; b[i] = x + lp * fb; if (++i == b.size()) i = 0; return o; }
};
struct Allpass {
    std::vector<float> b; size_t i = 0;
    float p(float x) { float bo = b[i]; float o = -x + bo; b[i] = x + bo * .5f; if (++i == b.size()) i = 0; return o; }
};

// Freeverb topology: 8 damped combs into 4 allpasses per side.
void reverb(const std::vector<float>& inL, const std::vector<float>& inR, std::vector<float>& outL, std::vector<float>& outR,
            float room, float damp) {
    static const int comb[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617}, ap[4] = {556, 441, 341, 225};
    const double k = double(R) / 44100;
    Comb cl[8], cr[8];
    Allpass al[4], ar[4];
    for (int i = 0; i < 8; ++i) { cl[i].b.assign(size_t(comb[i] * k), 0); cr[i].b.assign(size_t((comb[i] + 23) * k), 0); }
    for (int i = 0; i < 4; ++i) { al[i].b.assign(size_t(ap[i] * k), 0); ar[i].b.assign(size_t((ap[i] + 23) * k), 0); }
    outL.assign(inL.size(), 0); outR.assign(inL.size(), 0);
    for (size_t n = 0; n < inL.size(); ++n) {
        float x = (inL[n] + inR[n]) * .015f, a = 0, b = 0;
        for (int i = 0; i < 8; ++i) a += cl[i].p(x, room, damp), b += cr[i].p(x, room, damp);
        for (int i = 0; i < 4; ++i) a = al[i].p(a), b = ar[i].p(b);
        outL[n] = a; outR[n] = b;
    }
}

struct Style {
    const char* name;
    int bpm; bool minor, sevenths; int barsPerChord;
    double padGain, bassGain, arpGain, leadGain, swing, cutoff, room, revMix, duck;
    int drums;  // 0 none, 1 lofi, 2 corporate, 3 four-on-floor, 4 cinematic, 5 upbeat
    int arp;    // 0 none, 1 bell 8ths, 2 pluck 8ths, 3 pluck 16ths, 4 epiano chords
    const char* bass;
};
const Style kStyles[] = {
    {"ambient",   70, true,  false, 2, .55, .30, .20, .12, 0,   1100, .90, .55, 0,   0, 1, "x..............."},
    {"lofi",      78, true,  true,  1, .28, .42, .00, .10, .20, 1500, .80, .30, .25, 1, 4, "x.....x...x....."},
    {"corporate", 108, false, false, 1, .38, .40, .26, .16, 0,   2200, .82, .28, .30, 2, 2, "x...x...x..xx..."},
    {"cinematic", 84, true,  false, 2, .60, .40, .28, .14, 0,   1400, .92, .50, .15, 4, 3, "x..............."},
    {"tech",      118, true,  false, 1, .30, .42, .30, .10, 0,   2600, .78, .22, .55, 3, 3, ".x.x.x.x.x.x.x.x"},
    {"upbeat",    124, false, false, 1, .34, .38, .28, .18, 0,   3000, .80, .25, .45, 5, 2, "x.x.x.x.x.x.x.xo"},
};
const int kMinorLoops[4][4] = {{0, 5, 2, 6}, {0, 3, 5, 4}, {0, 5, 3, 6}, {0, 2, 5, 6}};
const int kMajorLoops[4][4] = {{0, 4, 5, 3}, {0, 5, 3, 4}, {5, 3, 0, 4}, {0, 3, 5, 4}};
const char* kKick[6] = {"", "x.....x.x.......", "x.......x.......", "x...x...x...x...", "x...............", "x...x...x...x..."};
const char* kSnare[6] = {"", "....x.......x...", "....x.......x...", "....x.......x...", "", "....x.......x..."};
const char* kHat[6] = {"", "x.x.x.x.x.x.x.x.", ".x.x.x.x.x.x.x.x", "..x...x...x...x.", "", "x.x.x.x.x.x.x.xx"};

struct Song {
    size_t n;
    std::vector<float> bedL, bedR, topL, topR, rvL, rvR, dlL;
    std::vector<size_t> kicks;
    explicit Song(size_t len) : n(len), bedL(len), bedR(len), topL(len), topR(len), rvL(len), rvR(len), dlL(len) {}
    void put(bool bed, size_t i, double l, double r, double rev, double dly) {
        if (i >= n) return;
        (bed ? bedL : topL)[i] += float(l); (bed ? bedR : topR)[i] += float(r);
        rvL[i] += float(l * rev); rvR[i] += float(r * rev);
        dlL[i] += float((l + r) * .5 * dly);
    }
    void mono(bool bed, size_t i, double v, double pan, double rev, double dly) {
        double a = (pan + 1) * pi / 4;
        put(bed, i, v * std::cos(a), v * std::sin(a), rev, dly);
    }
    void pad(const int (&m)[4], size_t at, size_t len, double cutoff, double gain, double phase, double rev) {
        if (at >= n) return;
        const size_t rel = size_t(.9 * R), att = std::min<size_t>(size_t(.5 * R), len / 2 + 1);
        double ph[4][3] = {}, lpL[2] = {}, lpR[2] = {};
        const double dt[3] = {std::exp2(-7 / 1200.0), 1, std::exp2(7 / 1200.0)};
        double freq[4];
        for (int v = 0; v < 4; ++v) freq[v] = mtof(m[v]) / R;
        for (size_t i = 0; i < len + rel && at + i < n; ++i) {
            double env = std::min(1.0, double(i) / att) * (i < len ? 1.0 : 1.0 - double(i - len) / rel), a = 0, b = 0;
            for (int v = 0; v < 4; ++v) {
                const double f = freq[v];
                for (int d = 0; d < 3; ++d) {
                    double s = saw(ph[v][d], f * dt[d]);
                    ph[v][d] += f * dt[d]; ph[v][d] -= std::floor(ph[v][d]);
                    if (d == 0) a += s; else if (d == 2) b += s; else a += s * .6, b += s * .6;
                }
            }
            double cut = cutoff * (1 + .35 * std::sin(phase + 2 * pi * double(i) / (R * 9.0)));
            double k = 1 - std::exp(-2 * pi * cut / R);
            lpL[0] += k * (a - lpL[0]); lpL[1] += k * (lpL[0] - lpL[1]);
            lpR[0] += k * (b - lpR[0]); lpR[1] += k * (lpR[0] - lpR[1]);
            put(true, at + i, lpL[1] * env * gain * .2, lpR[1] * env * gain * .2, rev, 0);
        }
    }
    void pluck(double f, size_t at, double dur, double pan, double gain, double bright, bool tri, bool bed, double rev, double dly) {
        if (at >= n) return;
        double ph = 0, lp = 0;
        const size_t len = std::min(size_t((dur + .2) * R), n - at);
        const double angle = (pan + 1) * pi / 4, left = std::cos(angle), right = std::sin(angle);
        for (size_t i = 0; i < len; ++i) {
            double t = double(i) / R, amp = std::exp(-t / (dur * .5 + .03)) * std::min(1.0, i / (.002 * R));
            double x = tri ? 1 - 4 * std::fabs(ph - .5) : saw(ph, f / R);
            ph += f / R; ph -= std::floor(ph);
            double k = 1 - std::exp(-2 * pi * (f * bright * std::exp(-t * 7) + f * 1.2) / R);
            lp += k * (x - lp);
            const double v = lp * amp * gain;
            put(bed, at + i, v * left, v * right, rev, dly);
        }
    }
    void epiano(double f, size_t at, double dur, double pan, double gain) {
        if (at >= n) return;
        const size_t len = std::min(size_t((dur * 1.2 + .3) * R), n - at);
        for (int c = 0; c < 2; ++c) {
            double fc = f * (c ? 1.0015 : .9985), ph = 0, pm = 0;
            const double angle = (pan + (c ? .35 : -.35) + 1) * pi / 4, left = std::cos(angle), right = std::sin(angle);
            for (size_t i = 0; i < len; ++i) {
                double t = double(i) / R;
                double y = std::sin(2 * pi * ph + 1.4 * std::exp(-t * 5) * std::sin(2 * pi * pm)) * std::exp(-t / (dur * .9 + .1));
                y *= std::min(1.0, i / (.004 * R));
                ph += fc / R; pm += fc * 14 / R;
                const double v = y * gain * .5;
                put(true, at + i, v * left, v * right, .3, 0);
            }
        }
    }
    void bass(double f, size_t at, double dur, double gain) {
        if (at >= n) return;
        double ph = 0, lp = 0;
        const size_t len = size_t(dur * R) + R / 20;
        const double k = 1 - std::exp(-2 * pi * 420 / R);
        for (size_t i = 0; i < std::min(len, n - at); ++i) {
            double t = double(i) / R;
            double env = std::min({1.0, i / (.004 * R), (len - 1.0 - i) / (.04 * R)}) * (.7 + .3 * std::exp(-t * 4));
            lp += k * (saw(ph, f / R) - lp);
            double v = (std::sin(2 * pi * ph) * .9 + lp * .45) * env * gain * .5;
            ph += f / R; ph -= std::floor(ph);
            put(true, at + i, v, v, 0, 0);
        }
    }
    void kick(size_t at, double gain, bool boom) {
        if (at >= n) return;
        double ph = 0;
        const double decay = boom ? .45 : .11, f0 = boom ? 60 : 46, f1 = boom ? 42 : 150;
        for (size_t i = 0; i < std::min(size_t(decay * 6 * R), n - at); ++i) {
            double t = double(i) / R;
            ph += (f0 + (f1 - f0) * std::exp(-t * (boom ? 12 : 32))) / R;
            double v = std::sin(2 * pi * ph) * std::exp(-t / decay) * std::min(1.0, i / (.0008 * R));
            put(false, at + i, v * gain, v * gain, boom ? .25 : 0, 0);
        }
        if (!boom) kicks.push_back(at);
    }
    void noiseHit(size_t at, double decay, double gain, double tone, Rng& rng, double rev, bool bursts) {
        double prev = 0, ph = 0;
        double left[3], right[3];
        for (int j = 0; j < 3; ++j) {
            const double angle = ((j - 1) * .12 + 1) * pi / 4;
            left[j] = std::cos(angle); right[j] = std::sin(angle);
        }
        for (size_t i = 0; i < size_t(decay * 5 * R); ++i) {
            double t = double(i) / R, x = rng.f() * 2 - 1;
            double d = x - prev; prev = x;  // first difference: bright noise
            double env = std::exp(-t / decay);
            if (bursts && t < .03) env *= std::fmod(t, .01) < .006 ? 1.0 : .45;  // clap: three quick bursts
            ph += tone / R;
            double v = (d * .6 + (tone > 0 ? std::sin(2 * pi * ph) * .5 : 0)) * env * gain;
            const int pan = int(rng.next() % 3);
            put(false, at + i, v * left[pan], v * right[pan], rev, 0);
        }
    }
    void bell(double f, size_t at, double dur, double pan, double gain) {
        if (at >= n) return;
        const double part[3] = {1, 2.0, 2.76}, amp[3] = {1, .35, .22}, tau[3] = {1.0, .6, .35};
        const double angle = (pan + 1) * pi / 4, left = std::cos(angle), right = std::sin(angle);
        for (int p = 0; p < 3; ++p) {
            double ph = 0;
            for (size_t i = 0; i < std::min(size_t((dur * tau[p] * 5 + .05) * R), n - at); ++i) {
                double t = double(i) / R;
                double v = std::sin(2 * pi * ph) * amp[p] * std::exp(-t / (dur * tau[p] * .8 + .05)) * std::min(1.0, i / (.002 * R));
                ph += f * part[p] / R;
                const double value = v * gain * .5;
                put(false, at + i, value * left, value * right, .55, .5);
            }
        }
    }
    void riser(size_t at, size_t len, double gain, Rng& rng) {
        double lp = 0, lp2 = 0;
        for (size_t i = 0; i < len; ++i) {
            double p = double(i) / len, x = rng.f() * 2 - 1;
            double k = 1 - std::exp(-2 * pi * (300 * std::pow(30, p)) / R);
            lp += k * (x - lp); lp2 += k * (lp - lp2);
            double v = lp2 * p * p * gain * 1.6;
            mono(false, at + i, v, (rng.f() - .5) * .6, .4, 0);
        }
    }
};

int parseKey(std::string k) {
    if (k.empty() || k.size() > 2 || (k.size() == 2 && k[1] != '#' && k[1] != 'b')) return -1;
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7};
    char c = (char)toupper((unsigned char)k[0]);
    if (c < 'A' || c > 'G') return -1;
    int s = base[c - 'A'];
    if (k.size() > 1 && (k[1] == '#')) ++s;
    else if (k.size() > 1 && k[1] == 'b') --s;
    return (s + 12) % 12;
}

Stereo composeMusic(const Style& st, int key, bool minor, double bpm, double seconds, uint32_t seed, double* lufs) {
    Rng rng(seed);
    const size_t n = size_t(seconds * R);
    Song song(n);
    const double beat = 60.0 / bpm, step = beat / 4, barSec = beat * 4;
    const int bars = std::max(4, int(std::ceil(seconds / barSec)));
    const int cycle = 4 * st.barsPerChord;
    static const int scaleMajor[7] = {0, 2, 4, 5, 7, 9, 11}, scaleMinor[7] = {0, 2, 3, 5, 7, 8, 10};
    const int* scale = minor ? scaleMinor : scaleMajor;
    const int tonic = 48 + key;
    auto note = [&](int deg) { int o = deg >= 0 ? deg / 7 : -((6 - deg) / 7); return tonic + 12 * o + scale[deg - 7 * o]; };
    const int* loop = (minor ? kMinorLoops : kMajorLoops)[rng.pick(4)];
    int lofiLoop[4] = {1, 4, 0, 5};  // ii-V-I-vi in major for a jazzier lofi
    if (!strcmp(st.name, "lofi") && !minor) loop = lofiLoop;
    // Motif: one bar of rhythm plus contour, relative to the sounding chord.
    static const double rhythms[5][5] = {{1, 1, 1, 1, 0}, {1.5, .5, 1, 1, 0}, {.5, .5, 1, 2, 0}, {2, 1, 1, 0, 0}, {1, .5, .5, 2, 0}};
    const double* rhythm = rhythms[rng.pick(5)];
    int contour[5] = {0, 0, 0, 0, 0};
    for (int e = 0, cur = 0; e < 5 && rhythm[e] > 0; ++e) {
        static const int chordTone[3] = {0, 2, 4};
        cur = e == 0 || rng.pick(3) == 0 ? chordTone[rng.pick(3)] : cur + (rng.pick(2) ? 1 : -1);
        contour[e] = cur;
    }
    // Bar plan.
    const int intro = std::max(st.barsPerChord, (bars / 10) / st.barsPerChord * st.barsPerChord);
    const int cycles = bars / cycle;
    const int breakCycle = cycles >= 5 ? int(std::lround(cycles * .6)) : -1;
    const int drumsFrom = intro + (st.drums == 4 ? cycle : std::max(st.barsPerChord, intro / 2));
    const int outro = std::max(st.barsPerChord, 2);
    int voicing[4] = {0, 0, 0, 0};
    bool first = true;
    for (int b = 0; b < bars; ++b) {
        const size_t t0 = size_t(b * barSec * R);
        if (t0 >= n) break;
        const int slot = (b / st.barsPerChord) % 4, cyc = b / cycle;
        const bool inBreak = cyc == breakCycle, inOutro = b >= bars - outro;
        const bool drumsOn = st.drums && b >= drumsFrom && !inBreak && !inOutro;
        const bool bassOn = b >= std::max(1, intro / 2) && !inBreak;
        const bool arpOn = st.arp && b >= intro && !inOutro;
        const bool leadOn = st.leadGain > 0 && b >= intro + cycle && (cyc % 2 == 1 || inBreak) && !inOutro;
        const int root = loop[slot];
        int tones[4] = {note(root), note(root + 2), note(root + 4), st.sevenths ? note(root + 6) : note(root) + 12};
        if (!st.sevenths && !strcmp(st.name, "ambient")) tones[3] = note(root + 8);  // add9 shimmer
        if (b % st.barsPerChord == 0) {
            // Voice-leading: keep each voice near where it was.
            int best[4] = {0, 0, 0, 0}, bestCost = 1 << 30, perm[4] = {0, 1, 2, 3};
            do {
                int cand[4], cost = 0;
                for (int v = 0; v < 4; ++v) {
                    int pc = ((tones[perm[v]] % 12) + 12) % 12, target = first ? 62 + (v - 1) * 4 : voicing[v];
                    int m = pc + 12 * int(std::lround((target - pc) / 12.0));
                    cand[v] = m; cost += std::abs(m - target);
                }
                if (cost < bestCost) { bestCost = cost; memcpy(best, cand, sizeof best); }
            } while (std::next_permutation(perm, perm + 4));
            memcpy(voicing, best, sizeof voicing);
            std::sort(voicing, voicing + 4);
            first = false;
            if (strcmp(st.name, "lofi")) song.pad(voicing, t0, size_t(st.barsPerChord * barSec * R), st.cutoff * (inBreak ? .8 : 1),
                                                  st.padGain * (b < intro ? .8 : 1), b * .7, st.revMix);
        }
        // Lofi: strummed electric-piano chords instead of a pad.
        if (st.arp == 4 && b >= 0 && !inOutro)
            for (int v = 0; v < 4; ++v) {
                size_t off = size_t((v * .012 + (drumsOn ? 0 : .02)) * R);
                song.epiano(mtof(voicing[v]), t0 + off, barSec * .9, -.3 + v * .2, st.padGain * (b < intro ? .7 : 1));
                if (drumsOn && (b % 2 == 1))
                    song.epiano(mtof(voicing[v]), t0 + size_t((2.5 * beat + v * .01) * R), beat * 1.2, -.3 + v * .2, st.padGain * .6);
            }
        // Bass line.
        if (bassOn) {
            const int rootPc = ((tones[0] % 12) + 12) % 12;
            const int bm = 28 + ((rootPc - 28) % 12 + 12) % 12;
            for (int s = 0; s < 16; ++s) {
                char c = st.bass[s];
                if (c == '.') continue;
                int len = 1;
                while (s + len < 16 && st.bass[s + len] == '.') ++len;
                const size_t at = t0 + size_t(s * step * R);
                if (c == 'o') song.bass(mtof(bm + 12), at, step * .9, st.bassGain);
                else song.bass(mtof(bm), at, step * std::min(len, 6) * .92, st.bassGain);
            }
        }
        // Arpeggio.
        if (arpOn && st.arp != 4) {
            static const int pat[3][8] = {{0, 2, 1, 3, 2, 1, 3, 2}, {0, 1, 2, 3, 2, 1, 2, 3}, {0, 2, 3, 1, 2, 3, 1, 2}};
            const int* p = pat[(b / cycle + st.arp) % 3];
            const bool sixteenth = st.arp == 3;
            const int steps = sixteenth ? 16 : 8;
            const double sd = sixteenth ? step : step * 2;
            for (int s = 0; s < steps; ++s) {
                if (st.arp == 1 && (s % 2 == 1 || (s % 4 == 2 && rng.pick(3) == 0))) continue;  // bells: sparse
                if (inBreak && s % 2) continue;
                const int m = voicing[p[s % 8]] + (st.arp == 1 ? 24 : 12);
                const size_t at = t0 + size_t((s * sd + ((s % 2) ? st.swing * sd * .5 : 0)) * R);
                const double vel = (s % 4 == 0 ? 1.0 : .7) * (.85 + rng.f() * .3);
                if (st.arp == 1) song.bell(mtof(m), at, 1.4, (s % 4 < 2 ? -.4 : .4), st.arpGain * vel);
                else song.pluck(mtof(m), at, sd * 1.6, (s % 2 ? .35 : -.35), st.arpGain * vel * .7, sixteenth ? 6 : 4, false, true, .22, .32);
            }
        }
        // Lead motif.
        if (leadOn) {
            double pos = 0;
            for (int e = 0; e < 5 && rhythm[e] > 0; ++e) {
                int deg = root + contour[e] + 14 + (slot == 3 && e == 0 ? -1 : 0);
                if (slot == 3 && rhythm[e + 1] == 0 && e + 1 < 5) deg = root + 14;
                song.bell(mtof(note(deg)), t0 + size_t(pos * beat * R), rhythm[e] * beat * 1.4, -.1 + .2 * (e % 2), st.leadGain * (e == 0 ? 1.1 : .9));
                pos += rhythm[e];
            }
        }
        // Drums.
        if (drumsOn) {
            const bool boom = st.drums == 4;
            for (int s = 0; s < 16; ++s) {
                const double sw = st.swing > 0 ? ((s % 4 == 2) ? st.swing * 2 * step : (s % 2 ? st.swing * step : 0)) : 0;
                const size_t at = t0 + size_t((s * step + sw) * R);
                if (kKick[st.drums][s] == 'x' && (!boom || b % st.barsPerChord == 0)) song.kick(at, boom ? .8 : .55, boom);
                if (kSnare[st.drums][s] == 'x') song.noiseHit(at, st.drums == 1 ? .13 : .11, st.drums == 1 ? .20 : .26, st.drums == 1 ? 190 : 0, rng, .18, st.drums != 1);
                if (kHat[st.drums][s] == 'x') song.noiseHit(at, s % 4 == 2 ? .05 : .025, (s % 4 == 0 ? .09 : .06) * (.8 + rng.f() * .4), 0, rng, .05, false);
            }
            if (b == drumsFrom || (breakCycle >= 0 && cyc == breakCycle + 1 && b % cycle == 0)) song.noiseHit(t0, 1.1, .32, 0, rng, .5, false);
        }
        if (strcmp(st.name, "ambient") && strcmp(st.name, "lofi") && ((st.drums && b == drumsFrom - 1) || (breakCycle >= 0 && cyc == breakCycle && b % cycle == cycle - 1)))
            song.riser(t0, size_t(barSec * R), .13, rng);
    }
    // Sidechain-style pump on the bed, then buses.
    if (st.duck > 0) {
        std::vector<float> g(n, 1.f);
        double d = 0;
        size_t k = 0;
        std::sort(song.kicks.begin(), song.kicks.end());
        const double rel = std::exp(-1.0 / (.16 * R));
        for (size_t i = 0; i < n; ++i) {
            while (k < song.kicks.size() && song.kicks[k] <= i) d = 1, ++k;
            g[i] = float(1 - st.duck * d);
            d *= rel;
        }
        for (size_t i = 0; i < n; ++i) song.bedL[i] *= g[i], song.bedR[i] *= g[i];
    }
    std::vector<float> rl, rr;
    reverb(song.rvL, song.rvR, rl, rr, float(st.room), .22f);
    Stereo out;
    out.l.assign(n, 0); out.r.assign(n, 0);
    // Ping-pong delay on the delay send, dotted eighth.
    const size_t dlen = std::max<size_t>(1, size_t(beat * .75 * R));
    std::vector<float> bl(dlen), br(dlen);
    float lpa = 0, lpb = 0;
    for (size_t i = 0, p = 0; i < n; ++i, p = (p + 1) % dlen) {
        float y1 = bl[p], y2 = br[p];
        lpa += .35f * (y2 - lpa); lpb += .35f * (y1 - lpb);
        bl[p] = song.dlL[i] + .4f * lpa; br[p] = .4f * lpb;
        const float wet = .5f;
        out.l[i] = song.bedL[i] + song.topL[i] + rl[i] * float(st.revMix > .4 ? 1.1 : 1.0) * 1.0f + y1 * wet;
        out.r[i] = song.bedR[i] + song.topR[i] + rr[i] * float(st.revMix > .4 ? 1.1 : 1.0) * 1.0f + y2 * wet;
    }
    if (!strcmp(st.name, "lofi")) {  // vinyl hiss and pops
        Rng nz(seed ^ 0xa5a5u);
        float lp = 0;
        for (size_t i = 0; i < n; ++i) {
            float x = float(nz.f() * 2 - 1);
            lp += .3f * (x - lp);
            float pop = nz.f() < .00035 ? float(std::pow(nz.f(), 3) * .05 * (nz.f() < .5 ? -1 : 1)) : 0;
            out.l[i] += lp * .004f + pop; out.r[i] += lp * .004f + pop * .8f;
        }
    }
    // Bus compressor (3:1 above -22 dBFS, 8 ms attack, 140 ms release): glue and a production-like crest factor.
    {
        double env = 0;
        const double atk = 1 - std::exp(-1.0 / (.008 * R)), rel = 1 - std::exp(-1.0 / (.14 * R));
        for (size_t i = 0; i < n; ++i) {
            double x = std::max(std::fabs(out.l[i]), std::fabs(out.r[i]));
            env += (x > env ? atk : rel) * (x - env);
            double over = gainToDb(env) + 22, g = over > 0 ? dbToGain(-over * (1 - 1 / 3.0)) : 1;
            out.l[i] *= float(g); out.r[i] *= float(g);
        }
    }
    // DC/sub cleanup, soft saturation, fades.
    float hl = 0, hr = 0;
    const float hk = float(1 - std::exp(-2 * pi * 28 / R));
    const size_t fadeOut = size_t(std::min(seconds * .2, 3.0) * R), fadeIn = size_t(.03 * R);
    for (size_t i = 0; i < n; ++i) {
        hl += hk * (out.l[i] - hl); hr += hk * (out.r[i] - hr);
        float f = std::min({1.f, float(i) / fadeIn, float(n - 1 - i) / std::max<size_t>(1, fadeOut)});
        f = f * f * (3 - 2 * f);
        out.l[i] = std::tanh((out.l[i] - hl) * 1.1f) * f;
        out.r[i] = std::tanh((out.r[i] - hr) * 1.1f) * f;
    }
    *lufs = integratedLufs(out);
    if (*lufs > -900) {
        const float g = float(dbToGain(-16 - *lufs));
        for (size_t i = 0; i < n; ++i) out.l[i] *= g, out.r[i] *= g;
        limitPeak(out, -1.0);
        *lufs = integratedLufs(out);
    }
    return out;
}

}  // namespace

int kitMusic(const std::vector<std::string>& a) {
    const char* usage = "usage: kit music OUT.wav [--style ambient|lofi|corporate|cinematic|tech|upbeat] [--duration SEC] "
                        "[--bpm N] [--key A|C#|Bb...] [--mode major|minor] [--seed N]";
    if (a.size() == 1 && a[0] == "--help") { puts(usage); return 0; }
    if (a.empty() || a.size() % 2 == 0) return fail(usage);
    std::string style = "corporate", key, mode;
    double duration = 30, bpm = 0, seed = 1;
    for (size_t i = 1; i < a.size(); i += 2) {
        const std::string& k = a[i];
        const std::string& v = a[i + 1];
        if (k == "--style") style = v;
        else if (k == "--key") key = v;
        else if (k == "--mode") mode = v;
        else if (k == "--duration" || k == "--bpm" || k == "--seed") {
            double x = 0;
            if (!number(v, x)) return fail("invalid number for " + k);
            (k == "--duration" ? duration : k == "--bpm" ? bpm : seed) = x;
        } else return fail("unknown option " + k + "\n" + usage);
    }
    const Style* st = nullptr;
    for (const Style& s : kStyles) if (style == s.name) st = &s;
    if (!st) return fail("style must be ambient, lofi, corporate, cinematic, tech or upbeat");
    if (duration < 4 || duration > maxSeconds) return fail("duration must be 4.." + std::to_string(maxSeconds) + " seconds");
    if (bpm != 0 && (bpm < 50 || bpm > 200)) return fail("bpm must be 50..200");
    if (seed < 0 || seed > 4294967295.0 || seed != std::floor(seed)) return fail("seed must be an unsigned integer");
    if (!mode.empty() && mode != "major" && mode != "minor") return fail("mode must be major or minor");
    Rng pickRng(uint32_t(seed) * 2654435761u + 7);
    static const char* keys[] = {"A", "C", "D", "E", "F", "G", "Bb"};
    if (key.empty()) key = keys[pickRng.pick(7)];
    int k = parseKey(key);
    if (k < 0) return fail("invalid key " + key);
    const bool minor = mode.empty() ? st->minor : mode == "minor";
    double lufs = 0;
    Stereo song = composeMusic(*st, k, minor, bpm > 0 ? bpm : st->bpm, duration, uint32_t(seed), &lufs);
    auto saved = saveStereoWav(a[0], song);
    if (!saved.ok) return fail(saved.error);
    printf("%s: %.1fs stereo 48000 Hz, %s in %s %s at %d BPM, %.1f LUFS, peak %.1f dBFS\n", a[0].c_str(), duration, st->name,
           key.c_str(), minor ? "minor" : "major", int(bpm > 0 ? bpm : st->bpm), lufs, peakDb(song));
    return 0;
}

int kitMix(const std::vector<std::string>& a) {
    const char* usage =
        "usage: kit mix OUT.wav [--voice FILE [--voice-at S] [--voice-trim START:END] [--voice-db 0]]\n"
        "                [--music FILE [--music-db -14] [--music-at S] [--music-trim START:END]] [--duck 8]\n"
        "                [--at SEC:FILE[:DB] ...] [--duration S] [--lufs -14] [--ceiling -1.5] [--fade-in 0] [--fade-out 1.5]";
    if (a.size() == 1 && a[0] == "--help") { puts(usage); return 0; }
    if (a.empty() || a.size() % 2 == 0) return fail(usage);
    std::string voicePath, musicPath, voiceTrim, musicTrim;
    std::vector<std::string> cues;
    double voiceAt = 0, musicAt = 0, voiceDb = 0, musicDb = -14, duck = 8, duration = 0, target = -14, ceiling = -1.5,
           fadeIn = 0, fadeOut = 1.5;
    for (size_t i = 1; i < a.size(); i += 2) {
        const std::string& k = a[i];
        const std::string& v = a[i + 1];
        double* num = k == "--voice-at" ? &voiceAt : k == "--music-at" ? &musicAt : k == "--voice-db" ? &voiceDb
                    : k == "--music-db" ? &musicDb : k == "--duck" ? &duck : k == "--duration" ? &duration : k == "--lufs" ? &target
                    : k == "--ceiling" ? &ceiling : k == "--fade-in" ? &fadeIn : k == "--fade-out" ? &fadeOut : nullptr;
        if (k == "--voice") voicePath = v;
        else if (k == "--music") musicPath = v;
        else if (k == "--voice-trim") voiceTrim = v;
        else if (k == "--music-trim") musicTrim = v;
        else if (k == "--at") cues.push_back(v);
        else if (num) { if (!number(v, *num)) return fail("invalid number for " + k); }
        else return fail("unknown option " + k + "\n" + usage);
    }
    if (voicePath.empty() && musicPath.empty() && cues.empty()) return fail("nothing to mix\n" + std::string(usage));
    if (duck < 0 || duck > 30 || target > -6 || target < -40 || ceiling > -0.1 || ceiling < -12 || voiceAt < 0 || voiceAt > maxSeconds ||
        musicAt < 0 || musicAt > maxSeconds || voiceDb < -60 || voiceDb > 12 || musicDb < -60 || musicDb > 12 ||
        fadeIn < 0 || fadeIn > 30 || fadeOut < 0 || fadeOut > 30 || duration < 0 || duration > maxSeconds)
        return fail("value out of range\n" + std::string(usage));
    if ((!voiceTrim.empty() && voicePath.empty()) || (!musicTrim.empty() && musicPath.empty()))
        return fail("trim needs the corresponding --voice or --music input");
    struct Placed { Stereo audio; size_t at; double db; };
    Stereo voice, music;
    std::vector<Placed> sfx;
    if (!voicePath.empty()) { auto r = loadAudio(voicePath); if (!r.ok) return fail(r.error); voice = std::move(r.value); }
    if (!musicPath.empty()) { auto r = loadAudio(musicPath); if (!r.ok) return fail(r.error); music = std::move(r.value); }
    auto trimTrack = [&](Stereo& s, const std::string& cut) -> std::string {
        if (cut.empty()) return "";
        const size_t colon = cut.find(':');
        double first = 0, last = 0;
        if (colon == std::string::npos || !number(cut.substr(0, colon), first) || !number(cut.substr(colon + 1), last) ||
            first < 0 || last <= first || last > double(s.size()) / R)
            return "trim needs START:END within the input's duration: " + cut;
        const size_t begin = size_t(std::llround(first * R)), end = std::min(s.size(), size_t(std::llround(last * R)));
        if (begin >= end) return "trim must contain at least one sample";
        for (auto* channel : {&s.l, &s.r}) {
            std::move(channel->begin() + begin, channel->begin() + end, channel->begin());
            channel->resize(end - begin);
        }
        return "";
    };
    if (auto error = trimTrack(voice, voiceTrim); !error.empty()) return fail(error);
    if (auto error = trimTrack(music, musicTrim); !error.empty()) return fail(error);
    for (const std::string& c : cues) {
        size_t colon = c.find(':');
        double at = 0, db = 0;
        if (colon == std::string::npos || !number(c.substr(0, colon), at) || at < 0 || at > maxSeconds)
            return fail("--at needs SEC:FILE[:DB], with SEC in 0..1200, got " + c);
        std::string rest = c.substr(colon + 1), file = rest;
        if (size_t c2 = rest.rfind(':'); c2 != std::string::npos && number(rest.substr(c2 + 1), db)) file = rest.substr(0, c2);
        if (db < -60 || db > 12) return fail("cue gain must be -60..12 dB: " + c);
        auto r = loadAudio(file);
        if (!r.ok) return fail(r.error);
        sfx.push_back({std::move(r.value), size_t(at * R), db});
    }
    size_t end = 0;
    if (voice.size()) end = std::max(end, size_t(voiceAt * R) + voice.size());
    if (music.size()) end = std::max(end, size_t(musicAt * R) + music.size());
    for (auto& s : sfx) end = std::max(end, s.at + s.audio.size());
    if (duration > 0) end = size_t(duration * R);
    if (end < R / 10 || end > maxSeconds * size_t(R)) return fail("mix length must be 0.1.." + std::to_string(maxSeconds) + " seconds");
    Stereo out;
    out.l.assign(end, 0); out.r.assign(end, 0);
    auto addAt = [&](const Stereo& s, size_t at, double gain) {
        for (size_t i = 0; i < s.size() && at + i < end; ++i) out.l[at + i] += float(s.l[i] * gain), out.r[at + i] += float(s.r[i] * gain);
    };
    double voiceLufs = -1000, voiceGain = 1;
    if (voice.size()) {
        voiceLufs = integratedLufs(voice);
        voiceGain = voiceLufs > -900 ? dbToGain(-16 + voiceDb - voiceLufs) : dbToGain(voiceDb);
        addAt(voice, size_t(voiceAt * R), voiceGain);
    }
    if (music.size()) {
        double ml = integratedLufs(music);
        // With a voice the music sits musicDb below it; alone it is the programme.
        double level = voice.size() ? -16 + voiceDb + musicDb : -16;
        double g = ml > -900 ? dbToGain(level - ml) : 1;
        std::vector<float> duckGain(end, 1.f);
        if (voice.size() && duck > 0) {
            // Speech presence: rectified envelope, fast attack, slow release, opened 80 ms early.
            const size_t vAt = size_t(voiceAt * R), early = R * 8 / 100;
            std::vector<float> active(end, 0.f);
            double env = 0;
            const double atk = 1 - std::exp(-1.0 / (.01 * R)), rel = 1 - std::exp(-1.0 / (.35 * R));
            for (size_t i = 0; i < voice.size() && vAt + i < end; ++i) {
                double x = std::max(std::fabs(voice.l[i]), std::fabs(voice.r[i])) * voiceGain;
                env += (x > env ? atk : rel) * (x - env);
                double db = gainToDb(env);
                active[vAt + i] = float(std::clamp((db + 52) / 14, 0.0, 1.0));  // -52..-38 dBFS opens the duck
            }
            const double depth = 1 - dbToGain(-duck);
            double s = 0;
            const double sm = 1 - std::exp(-1.0 / (.06 * R));
            for (size_t i = 0; i < end; ++i) {
                s += sm * (active[std::min(end - 1, i + early)] - s);
                duckGain[i] = float(1 - depth * s);
            }
        }
        for (size_t i = 0; i < music.size() && size_t(musicAt * R) + i < end; ++i) {
            const size_t o = size_t(musicAt * R) + i;
            out.l[o] += float(music.l[i] * g * duckGain[o]); out.r[o] += float(music.r[i] * g * duckGain[o]);
        }
        if (size_t(musicAt * R) + music.size() + R < end && !voicePath.empty())
            fprintf(stderr, "pocket kit: note: the music ends %.1fs before the mix does; generate it longer with kit music --duration\n",
                    double(end - size_t(musicAt * R) - music.size()) / R);
    }
    for (auto& s : sfx) {
        float pk = 0;
        for (size_t i = 0; i < s.audio.size(); ++i) pk = std::max({pk, std::fabs(s.audio.l[i]), std::fabs(s.audio.r[i])});
        addAt(s.audio, s.at, pk > 1e-6f ? dbToGain(-10 + s.db) / pk : 0);  // accents peak 10 dB under full scale
    }
    // Programme fade-out, loudness to target, peak ceiling.
    const size_t fadeUp = std::min(end, size_t(fadeIn * R));
    for (size_t i = 0; i < fadeUp; ++i) {
        double f = double(i) / std::max<size_t>(1, fadeUp - 1);
        f = f * f * (3 - 2 * f);
        out.l[i] *= float(f); out.r[i] *= float(f);
    }
    const size_t fade = std::min(end, size_t(fadeOut * R));
    for (size_t i = 0; i < fade; ++i) {
        double f = double(fade - 1 - i) / fade;
        f = f * f * (3 - 2 * f);
        out.l[end - fade + i] *= float(f); out.r[end - fade + i] *= float(f);
    }
    double lufs = integratedLufs(out);
    if (lufs < -900) return fail("the mix is silent");
    const double gain = dbToGain(target - lufs);
    for (size_t i = 0; i < end; ++i) out.l[i] *= float(gain), out.r[i] *= float(gain);
    limitPeak(out, ceiling);
    auto saved = saveStereoWav(a[0], out);
    if (!saved.ok) return fail(saved.error);
    printf("%s: %.2fs stereo 48000 Hz, %.1f LUFS, peak %.1f dBFS (target %.0f LUFS, ceiling %.1f dBFS)\n", a[0].c_str(),
           double(end) / R, integratedLufs(out), peakDb(out), target, ceiling);
    return 0;
}

}  // namespace pocket
