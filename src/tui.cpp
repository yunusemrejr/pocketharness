// PocketHarness - TUI part 1: terminal, colors, markdown-ish render, editor.
#include "tui.h"

#include <cctype>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>

#include "skills.h"

namespace pocket {

namespace {

// --- colors (TTY only; callers ensure that) ---
const char* C_RESET = "\033[0m";
const char* C_BOLD = "\033[1m";
const char* C_DIM = "\033[2m";
const char* C_GREEN = "\033[32m";
const char* C_YELLOW = "\033[33m";
const char* C_MAGENTA = "\033[35m";
const char* C_CYAN = "\033[36m";
const char* C_RED = "\033[31m";

bool useColor() { return getenv("NO_COLOR") == nullptr; }
std::string col(const char* c) { return useColor() ? std::string(c) : std::string(); }

void writeAll(int fd, const std::string& s) {
    size_t off = 0;
    while (off < s.size()) {
        ssize_t n = write(fd, s.data() + off, s.size() - off);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            break;
        }
        off += (size_t)n;
    }
}

struct TermGuard {
    struct termios orig {};
    bool active = false;
    void enter() {
        if (active) return;
        if (tcgetattr(STDIN_FILENO, &orig) != 0) return;
        struct termios raw = orig;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= CS8;
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) active = true;
    }
    void leave() {
        if (!active) return;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
        active = false;
    }
    ~TermGuard() { leave(); }
};

TermGuard* g_term = nullptr;
void onFatalSignal(int sig) {
    if (g_term) g_term->leave();
    signal(sig, SIG_DFL);
    raise(sig);
}
volatile sig_atomic_t g_winch = 0;
void onWinch(int) { g_winch = 1; }

}  // namespace

// ---------------------------------------------------------------------------
// Cheap Markdown-ish line rendering.
// ---------------------------------------------------------------------------
std::string renderLine(const std::string& line, bool& inFence) {
    std::string s = sanitizeTerminal(line);
    std::string fence = trim(s);
    if (fence.size() >= 3 && fence.compare(0, 3, "```") == 0) {
        inFence = !inFence;
        return col(C_DIM) + s + col(C_RESET);
    }
    if (inFence) return col(C_GREEN) + s + col(C_RESET);
    if (!s.empty() && s[0] == '#') {
        size_t i = 0;
        while (i < s.size() && s[i] == '#') ++i;
        if (i < s.size() && s[i] == ' ')
            return col(C_BOLD) + s + col(C_RESET);
    }
    // Bullets / numbered lists.
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    bool bullet = (i + 1 < s.size() && (s[i] == '-' || s[i] == '*') && s[i + 1] == ' ');
    size_t j = i;
    while (j < s.size() && isdigit((unsigned char)s[j])) ++j;
    bool numbered = (j > i && j + 1 < s.size() && s[j] == '.' && s[j + 1] == ' ');
    std::string lead = s.substr(0, i);
    std::string rest = s.substr(i);
    // `code` spans.
    std::string body;
    bool inCode = false;
    for (size_t k = 0; k < rest.size(); ++k) {
        if (rest[k] == '`') {
            body += inCode ? col(C_RESET) : col(C_CYAN);
            inCode = !inCode;
            body.push_back('`');
        } else {
            body.push_back(rest[k]);
        }
    }
    if (inCode) body += col(C_RESET);
    if (bullet || numbered)
        return lead + col(C_YELLOW) + body.substr(0, bullet ? 1 : (j - i + 1)) + col(C_RESET) +
               body.substr(bullet ? 1 : (j - i + 1));
    // **bold** spans (cheap: toggle on pairs).
    std::string out;
    for (size_t k = 0; k < body.size();) {
        if (k + 1 < body.size() && body[k] == '*' && body[k + 1] == '*') {
            // find closing pair
            size_t e = body.find("**", k + 2);
            if (e == std::string::npos) {
                out += "**";
                k += 2;
            } else {
                out += col(C_BOLD);
                out.append(body, k + 2, e - k - 2);
                out += col(C_RESET);
                k = e + 2;
            }
        } else {
            out.push_back(body[k]);
            ++k;
        }
    }
    return lead + out;
}

// ---------------------------------------------------------------------------
// Multiline line editor.
// ---------------------------------------------------------------------------
namespace {

int utf8Len(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

size_t stepBack(const std::string& l, size_t col) {
    if (col == 0) return 0;
    size_t c = col - 1;
    while (c > 0 && ((unsigned char)l[c] & 0xC0) == 0x80) --c;
    return c;
}

size_t stepFwd(const std::string& l, size_t col) {
    if (col >= l.size()) return l.size();
    return col + (size_t)utf8Len((unsigned char)l[col]);
}

size_t dispWidth(const std::string& s) {
    // Codepoint count (wide CJK chars count 1; acceptable imprecision).
    size_t w = 0;
    for (size_t i = 0; i < s.size();) {
        i += (size_t)utf8Len((unsigned char)s[i]);
        ++w;
    }
    return w;
}

const char* kCommands[] = {"/model", "/thinking", "/compact", "/skills",
                            "/session", "/security", "/help", "/quit", nullptr};

struct Editor {
    std::vector<std::string> lines{""};
    size_t row = 0, col = 0;
    std::vector<std::string> history;
    long histIdx = -1;
    std::string histSaved;
    std::string prompt;
    int lastRows = 1;

    bool empty() const { return lines.size() == 1 && lines[0].empty(); }

    void redraw() {
        std::string out;
        if (lastRows > 1) {
            out += "\r";
            for (int i = 1; i < lastRows; ++i) out += "\033[A\033[K\r";
        } else {
            out += "\r";
        }
        for (size_t r = 0; r < lines.size(); ++r) {
            if (r == 0) out += prompt;
            else out += "\033[K... ";
            out += sanitizeTerminal(lines[r]);
            out += "\033[K";
            if (r + 1 < lines.size()) out += "\n";
        }
        // Position cursor: up to row, then to column.
        if (lines.size() - 1 > row) {
            out += "\033[" + std::to_string(lines.size() - 1 - row) + "A";
        }
        size_t target = (row == 0 ? dispWidth(prompt) : 4) + dispWidth(lines[row].substr(0, col));
        size_t endcol =
            (row == 0 ? dispWidth(prompt) : 4) + dispWidth(lines[row]);
        out += "\r";
        if (target > 0) out += "\033[" + std::to_string(target) + "C";
        (void)endcol;
        writeAll(STDOUT_FILENO, out);
        lastRows = (int)lines.size();
    }

    void insertBytes(const char* p, size_t n) {
        lines[row].insert(col, p, n);
        col += n;
    }

    void newline() {
        std::string rest = lines[row].substr(col);
        lines[row].erase(col);
        lines.insert(lines.begin() + (long)row + 1, rest);
        ++row;
        col = 0;
    }

    void backspace() {
        if (col > 0) {
            size_t p = stepBack(lines[row], col);
            lines[row].erase(p, col - p);
            col = p;
        } else if (row > 0) {
            col = lines[row - 1].size();
            lines[row - 1] += lines[row];
            lines.erase(lines.begin() + (long)row);
            --row;
        }
    }

    void deleteFwd() {
        if (col < lines[row].size()) {
            size_t n = stepFwd(lines[row], col);
            lines[row].erase(col, n - col);
        } else if (row + 1 < lines.size()) {
            lines[row] += lines[row + 1];
            lines.erase(lines.begin() + (long)row + 1);
        }
    }

    void killWordBack() {
        if (col == 0) {
            backspace();
            return;
        }
        size_t c = col;
        while (c > 0 && (lines[row][c - 1] == ' ' || lines[row][c - 1] == '\t'))
            c = stepBack(lines[row], c);
        while (c > 0 && lines[row][c - 1] != ' ' && lines[row][c - 1] != '\t')
            c = stepBack(lines[row], c);
        lines[row].erase(c, col - c);
        col = c;
    }

    void histShow(long idx) {
        if (histIdx == -1) histSaved = lines.size() == 1 ? lines[0] : "";
        histIdx = idx;
        lines = {history[(size_t)idx]};
        row = 0;
        col = lines[0].size();
    }
    void histPrev() {
        if (history.empty()) return;
        if (histIdx == -1) histShow((long)history.size() - 1);
        else if (histIdx > 0) histShow(histIdx - 1);
    }
    void histNext() {
        if (histIdx == -1) return;
        if ((size_t)histIdx + 1 < history.size()) histShow(histIdx + 1);
        else {
            histIdx = -1;
            lines = {histSaved};
            row = 0;
            col = lines[0].size();
        }
    }

    void completeSlash() {
        if (lines.size() != 1 || lines[0].empty() || lines[0][0] != '/') return;
        if (lines[0].find(' ') != std::string::npos) return;
        std::string prefix = lines[0];
        const char* match = nullptr;
        int n = 0;
        for (const char** c = kCommands; *c; ++c) {
            if (startsWith(*c, prefix)) {
                match = *c;
                ++n;
            }
        }
        if (n == 1 && match) {
            lines[0] = std::string(match) + " ";
            col = lines[0].size();
        }
    }

    std::string text() const {
        std::string out;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i) out += "\n";
            out += lines[i];
        }
        return out;
    }

    void reset() {
        lines = {""};
        row = col = 0;
        histIdx = -1;
        lastRows = 1;
    }
};

// Read one input chunk (blocking). Returns false on EOF/error.
bool readChunk(std::string& chunk) {
    char buf[128];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return false;
    chunk.assign(buf, (size_t)n);
    return true;
}

bool readMore(std::string& extra, int ms) {
    struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
    if (poll(&pfd, 1, ms) <= 0) return false;
    char buf[64];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return false;
    extra.assign(buf, (size_t)n);
    return true;
}

// Returns submitted text, or nullopt when the user asked to exit.
std::optional<std::string> editLine(Editor& ed, bool& exitFlag) {
    ed.lastRows = 1;
    ed.redraw();
    std::string esc;  // pending escape sequence (after ESC)
    bool inEsc = false;
    auto endEsc = [&]() {
        inEsc = false;
        esc.clear();
    };
    for (;;) {
        if (g_winch) {
            g_winch = 0;
            ed.redraw();
        }
        std::string chunk;
        if (inEsc && esc.empty()) {
            // Lone ESC vs sequence: wait briefly for more bytes.
            if (!readMore(chunk, 40)) {
                endEsc();
                if (!ed.empty()) {
                    ed.lines = {""};
                    ed.row = ed.col = 0;  // Esc clears the draft
                    ed.redraw();
                }
                continue;
            }
        } else if (!readChunk(chunk)) {
            exitFlag = true;
            return std::nullopt;
        }
        for (size_t i = 0; i < chunk.size(); ++i) {
            unsigned char c = (unsigned char)chunk[i];
            if (inEsc) {
                // Alt-Enter arrives as ESC CR.
                if (esc.empty() && (c == '\r' || c == '\n')) {
                    ed.newline();
                    endEsc();
                    continue;
                }
                esc.push_back((char)c);
                if (esc == "[A") {  // Up
                    if (ed.lines.size() > 1 && ed.row > 0) {
                        --ed.row;
                        if (ed.col > ed.lines[ed.row].size()) ed.col = ed.lines[ed.row].size();
                    } else if (ed.lines.size() == 1) {
                        ed.histPrev();
                    }
                    endEsc();
                } else if (esc == "[B") {  // Down
                    if (ed.row + 1 < ed.lines.size()) {
                        ++ed.row;
                        if (ed.col > ed.lines[ed.row].size()) ed.col = ed.lines[ed.row].size();
                    } else if (ed.lines.size() == 1) {
                        ed.histNext();
                    }
                    endEsc();
                } else if (esc == "[C") {  // Right
                    ed.col = stepFwd(ed.lines[ed.row], ed.col);
                    endEsc();
                } else if (esc == "[D") {  // Left
                    ed.col = stepBack(ed.lines[ed.row], ed.col);
                    endEsc();
                } else if (esc == "[H" || esc == "[1~") {
                    ed.col = 0;
                    endEsc();
                } else if (esc == "[F" || esc == "[4~") {
                    ed.col = ed.lines[ed.row].size();
                    endEsc();
                } else if (esc == "[3~") {
                    ed.deleteFwd();
                    endEsc();
                } else if (esc.size() >= 6) {
                    endEsc();  // unknown sequence: ignore
                } else if (esc.size() >= 2 && esc[0] == '[' &&
                           (isalpha((unsigned char)esc.back()) || esc.back() == '~')) {
                    endEsc();  // terminated unknown CSI: ignore
                }
                // else: need more bytes; if chunk exhausted, next read continues
                continue;
            }
            if (c == 0x1b) {
                inEsc = true;
                esc.clear();
                continue;
            }
            switch (c) {
                case '\r': {  // Enter: submit
                    std::string t = ed.text();
                    if (trim(t).empty()) break;  // empty submit: ignore
                    if (ed.history.empty() || ed.history.back() != t) ed.history.push_back(t);
                    if (ed.history.size() > 200) ed.history.erase(ed.history.begin());
                    writeAll(STDOUT_FILENO, "\n");
                    return t;
                }
                case '\n':  // Ctrl-J: newline
                    ed.newline();
                    break;
                case 0x03:  // Ctrl-C
                    if (ed.empty()) {
                        exitFlag = true;
                        return std::nullopt;
                    }
                    ed.lines = {""};
                    ed.row = ed.col = 0;
                    break;
                case 0x04:  // Ctrl-D
                    if (ed.empty()) {
                        exitFlag = true;
                        return std::nullopt;
                    }
                    ed.deleteFwd();
                    break;
                case 0x7f:
                case 0x08:
                    ed.backspace();
                    break;
                case '\t':
                    ed.completeSlash();
                    break;
                case 0x01:
                    ed.col = 0;
                    break;  // Ctrl-A
                case 0x05:
                    ed.col = ed.lines[ed.row].size();
                    break;  // Ctrl-E
                case 0x15:
                    ed.lines[ed.row].clear();
                    ed.col = 0;
                    break;  // Ctrl-U
                case 0x17:
                    ed.killWordBack();
                    break;  // Ctrl-W
                case 0x0b:
                    ed.lines[ed.row].erase(ed.col);
                    break;  // Ctrl-K
                case 0x10:
                    ed.histPrev();
                    break;  // Ctrl-P
                case 0x0e:
                    ed.histNext();
                    break;  // Ctrl-N
                case 0x0c:
                    writeAll(STDOUT_FILENO, "\033[H\033[2J");
                    break;  // Ctrl-L
                default:
                    if (c >= 0x20 || c >= 0x80) ed.insertBytes(chunk.data() + i, 1);
                    break;
            }
        }
        ed.redraw();
    }
}

// ---------------------------------------------------------------------------
// Input watcher: during a turn, the main thread is busy streaming; this helper
// thread watches stdin for Ctrl-C (cancel) and feeds approval keypresses.
// ---------------------------------------------------------------------------
struct SharedInput {
    std::mutex mu;
    std::condition_variable cv;
    std::deque<char> q;
    std::atomic<bool> stop{false};
    std::atomic<bool> approvalMode{false};
    std::atomic<bool>* cancel = nullptr;
};

void watcherMain(SharedInput* in) {
    while (!in->stop.load()) {
        struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
        if (poll(&pfd, 1, 60) <= 0) continue;
        char buf[64];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) continue;
        for (ssize_t i = 0; i < n; ++i) {
            if (in->approvalMode.load()) {
                std::lock_guard<std::mutex> lk(in->mu);
                in->q.push_back(buf[i]);
                in->cv.notify_one();
            } else if (buf[i] == 0x03) {
                if (in->cancel) in->cancel->store(true);
            }
            // Anything else typed mid-generation is discarded (no echo anyway).
        }
    }
}

struct ApprovalCtx {
    SharedInput* in = nullptr;  // null in lineRun (cooked-mode prompt instead)
};
ApprovalCtx* g_approval = nullptr;

}  // namespace

bool askApprovalCli(const std::string& cmd, const std::string& reason) {
    std::string box = std::string(col(C_YELLOW)) + col(C_BOLD) +
                      "\nPocketHarness blocked a potentially destructive command.\n" + col(C_RESET) +
                      col(C_DIM) + reason + col(C_RESET) + "\n\n  " + sanitizeTerminal(cmd) +
                      "\n\nAllow once? [y/N] ";
    writeAll(STDOUT_FILENO, box);
    if (g_approval && g_approval->in) {
        SharedInput* in = g_approval->in;
        in->approvalMode.store(true);
        {
            std::lock_guard<std::mutex> lk(in->mu);
            in->q.clear();
        }
        bool allow = false;
        for (;;) {
            std::unique_lock<std::mutex> lk(in->mu);
            in->cv.wait(lk, [&] { return !in->q.empty() || in->stop.load(); });
            if (in->stop.load()) break;
            char c = in->q.front();
            in->q.pop_front();
            lk.unlock();
            if (c == 'y' || c == 'Y') {
                allow = true;
                break;
            }
            if (c == 'n' || c == 'N' || c == '\r' || c == '\n' || c == 0x1b || c == 0x03 || c == 0x04)
                break;  // default = reject
        }
        in->approvalMode.store(false);
        writeAll(STDOUT_FILENO, allow ? "allowed once\n" : "rejected\n");
        return allow;
    }
    // Cooked-mode fallback (lineRun): read a line.
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    line = toLower(trim(line));
    return line == "y" || line == "yes";
}

// ---------------------------------------------------------------------------
// Streaming renderer + turn runner
// ---------------------------------------------------------------------------
namespace {

struct StreamRenderer {
    std::string carry;
    bool fence = false;
    void feed(std::string_view tok) {
        carry.append(tok.data(), tok.size());
        size_t start = 0;
        for (;;) {
            size_t nl = carry.find('\n', start);
            if (nl == std::string::npos) break;
            writeAll(STDOUT_FILENO, renderLine(carry.substr(start, nl - start), fence) + "\n");
            start = nl + 1;
        }
        carry.erase(0, start);
    }
    void flush() {
        if (!carry.empty()) {
            writeAll(STDOUT_FILENO, renderLine(carry, fence) + "\n");
            carry.clear();
        }
    }
};

std::string shortModel(const std::string& spec) {
    size_t c = spec.find(':');
    return c == std::string::npos ? spec : spec.substr(c + 1);
}

std::string promptFor(TuiOpts& opts, Agent& agent) {
    long max = agent.contextMax();
    long pct = max > 0 ? agent.contextUsed() * 100 / max : 0;
    std::string p = std::string(col(C_BOLD)) + "pocket" + col(C_RESET) + col(C_DIM) + " [" +
                    shortModel(opts.model.spec) + "·" + opts.thinking + "·" +
                    std::to_string(pct) + "%" + (opts.unsafe ? "·UNSAFE" : "") +
                    (opts.allowNet ? "·NET" : "") + "] " + baseName(opts.workspace) + "> " +
                    col(C_RESET);
    return p;
}

void printBanner(TuiOpts& opts) {
    const SandboxCaps& caps = sandboxCaps();
    std::string b = std::string(col(C_BOLD)) + "pocket " + kVersion + col(C_RESET) + "  " +
                    col(C_DIM) + opts.workspace + col(C_RESET) + "\n" + col(C_DIM) + "session " +
                    opts.sessionId + " · " + opts.model.spec + " · thinking " + opts.thinking +
                    col(C_RESET) + "\n";
    if (!opts.systemSource.empty())
        b += col(C_DIM) + std::string("system prompt: ") + opts.systemSource + col(C_RESET) + "\n";
    if (opts.unsafe)
        b += std::string(col(C_RED)) + col(C_BOLD) +
             "UNSAFE MODE — model tools have broader user-account authority." + col(C_RESET) + "\n";
    if (!caps.landlock)
        b += col(C_YELLOW) + std::string("note: Landlock unavailable — model commands run without kernel filesystem confinement") + col(C_RESET) + "\n";
    if (!caps.seccompNet && !opts.allowNet)
        b += col(C_YELLOW) + std::string("note: seccomp unavailable — model network isolation is OFF (run with --network only if intended)") + col(C_RESET) + "\n";
    if (!caps.openat2)
        b += col(C_YELLOW) + std::string("note: openat2 unavailable — native file tools refuse symlinks entirely (strict fallback)") + col(C_RESET) + "\n";
    b += col(C_DIM) + "Enter submits · Ctrl-J newline · /help commands · Ctrl-C cancels · Ctrl-D quits" + col(C_RESET) + "\n";
    writeAll(STDOUT_FILENO, sanitizeTerminal(b));
}

int runTurnInteractive(TuiOpts& opts, Agent& agent, const std::string& input, SharedInput& shared,
                       std::atomic<bool>& cancel) {
    StreamRenderer rend;
    agent.setCallbacks(
        [&](std::string_view tok) {
            if (!cancel.load()) rend.feed(tok);
        },
        [&](const std::string& note) {
            writeAll(STDOUT_FILENO, col(C_DIM) + "\n(" + sanitizeTerminal(note) + ")" + col(C_RESET) + "\n");
        });
    opts.tools->onEvent = [&](const std::string& line) {
        writeAll(STDOUT_FILENO, col(C_DIM) + "\n  ⚙ " + sanitizeTerminal(line) + col(C_RESET) + "\n");
    };
    opts.tools->onToolDone = [&](const std::string& name, bool ok, const std::string& summary) {
        std::string mark = ok ? (col(C_GREEN) + std::string("  ✓ ")) : (col(C_RED) + std::string("  ✗ "));
        writeAll(STDOUT_FILENO, mark + sanitizeTerminal(name) + col(C_RESET) + "\n");
        int shown = 0;
        for (const std::string& ln : splitLines(summary)) {
            if (shown >= 3) {
                writeAll(STDOUT_FILENO, col(C_DIM) + "      [...]" + col(C_RESET) + "\n");
                break;
            }
            writeAll(STDOUT_FILENO,
                     col(C_DIM) + "      " + sanitizeTerminal(ln).substr(0, 160) + col(C_RESET) + "\n");
            ++shown;
        }
    };
    cancel.store(false);
    {
        std::lock_guard<std::mutex> lk(shared.mu);
        shared.q.clear();
    }
    writeAll(STDOUT_FILENO, col(C_MAGENTA) + std::string("assistant:") + col(C_RESET) + "\n");
    std::string err = agent.runTurn(input);
    rend.flush();
    if (err == "cancelled") {
        writeAll(STDOUT_FILENO, col(C_YELLOW) + std::string("\n(cancelled)") + col(C_RESET) + "\n");
        return 0;
    }
    if (!err.empty()) {
        writeAll(STDOUT_FILENO, col(C_RED) + std::string("\nerror: ") + sanitizeTerminal(err) + col(C_RESET) + "\n");
        return 0;
    }
    writeAll(STDOUT_FILENO, "\n");
    return 0;
}

// ---------------------------------------------------------------------------
// Slash commands. Returns false when the session should end.
// ---------------------------------------------------------------------------
bool runCommand(TuiOpts& opts, Agent& agent, const std::string& input) {
    std::string rest = trim(input.substr(1));
    size_t sp = rest.find(' ');
    std::string cmd = sp == std::string::npos ? rest : rest.substr(0, sp);
    std::string args = sp == std::string::npos ? "" : trim(rest.substr(sp + 1));
    cmd = toLower(cmd);

    auto say = [&](const std::string& s) { writeAll(STDOUT_FILENO, sanitizeTerminal(s)); };

    if (cmd == "quit" || cmd == "exit" || cmd == "q") return false;
    if (cmd == "help") {
        say("commands:\n"
            "  /model [spec]     show or switch model (provider:model[@routing] or alias)\n"
            "  /thinking [level] show or set thinking (off/low/medium/high/max)\n"
            "  /compact          summarize older context now\n"
            "  /skills [query]   list or search Markdown skills\n"
            "  /session          show session info and token usage\n"
            "  /security         show sandbox + authority state\n"
            "  /help             this text\n"
            "  /quit             exit\n"
            "keys: Enter submit · Ctrl-J/Alt-Enter newline · Up/Down history · Ctrl-C cancel/quit · Ctrl-D quit\n");
        return true;
    }
    if (cmd == "model") {
        if (args.empty()) {
            std::string s = "current: " + opts.model.spec + " (" + opts.model.provider.protocol +
                            " " + opts.model.provider.baseUrl + ")\naliases:\n";
            for (const auto& m : opts.cfg->models)
                s += "  " + m.alias + " = " + m.provider + ":" + m.model +
                     (m.routing.empty() ? "" : "@" + m.routing) + "\n";
            s += "providers:\n";
            for (const auto& p : opts.cfg->providers) s += "  " + p.name + " (" + p.protocol + ")\n";
            say(s);
            return true;
        }
        auto rm = resolveModel(*opts.cfg, args);
        if (!rm.ok) {
            say(col(C_RED) + std::string("error: ") + rm.error + col(C_RESET) + "\n");
            return true;
        }
        opts.model = rm.value;
        agent.setModel(rm.value, opts.thinking);
        saveUiState(UiState{rm.value.spec, opts.thinking});
        say("model: " + rm.value.spec + "\n");
        return true;
    }
    if (cmd == "thinking") {
        if (args.empty()) {
            say("thinking: " + opts.thinking + " (off/low/medium/high/max)\n");
            return true;
        }
        std::string t = toLower(args);
        if (t != "off" && t != "low" && t != "medium" && t != "high" && t != "max") {
            say("usage: /thinking off|low|medium|high|max\n");
            return true;
        }
        opts.thinking = t;
        agent.setModel(opts.model, t);
        saveUiState(UiState{opts.model.spec, t});
        say("thinking: " + t + "\n");
        return true;
    }
    if (cmd == "compact") {
        std::string err = agent.compactNow();
        say(err.empty() ? "compacted.\n" : err + "\n");
        return true;
    }
    if (cmd == "skills") {
        auto all = skillDiscover(opts.workspace);
        auto hits = skillSearch(all, args);
        if (hits.empty()) {
            say("no skills" + (args.empty() ? "" : " matching \"" + args + "\"") + ".\n");
            return true;
        }
        std::string s;
        for (const auto& m : hits) s += skillOneLine(m) + "\n";
        say(s);
        return true;
    }
    if (cmd == "session") {
        const AgentStats& st = agent.stats();
        say("session: " + opts.sessionId + "\nsystem: " +
            (opts.systemSource.empty() ? "built-in" : opts.systemSource) + "\nworkspace: " +
            opts.workspace + "\n" + "messages: " +
            std::to_string(agent.messageCount()) + " · turns: " + std::to_string(st.turns) +
            " · tool calls: " + std::to_string(st.toolCalls) +
            " · compactions: " + std::to_string(st.compactions) + "\n" + "context: ~" +
            std::to_string(agent.contextUsed()) + " / " + std::to_string(agent.contextMax()) +
            " tokens" + " · usage in/out: " + std::to_string(st.inTokens) + "/" +
            std::to_string(st.outTokens) + "\n");
        if (st.cacheSeen) {
            long denom = st.cacheHit + st.cacheMiss;
            if (denom <= 0) denom = st.inTokens;  // hit-only reporting
            long pct = denom > 0 ? st.cacheHit * 100 / denom : 0;
            say("cache: " + std::to_string(st.cacheHit) + " hit / " +
                std::to_string(st.cacheMiss) + " miss (" + std::to_string(pct) +
                "% reported reuse)\n");
        } else {
            say("cache: unreported by provider\n");
        }
        if (st.costSeen) {
            char buf[64];
            snprintf(buf, sizeof(buf), "cost: $%.4f (reported)\n", st.cost);
            say(buf);
        }
        return true;
    }
    if (cmd == "security") {
        const SandboxCaps& caps = sandboxCaps();
        std::string s = std::string("workspace: ") + opts.workspace + (opts.unsafe ? "  [UNSAFE: containment off]" : "") + "\n";
        if (!opts.unsafe && opts.tools && opts.tools->auth) {
            s += "read roots: " + std::to_string(opts.tools->auth->readRoots.size()) +
                 " · write roots: " + std::to_string(opts.tools->auth->writeRoots.size()) + "\n";
        }
        s += std::string("kernel: openat2 ") + (caps.openat2 ? "yes" : "NO") + " · landlock " +
             (caps.landlock ? "yes" : "NO") + " · seccomp-net " +
             (caps.seccompNet ? "yes" : "NO") + "\n";
        s += "tool network: " + std::string(opts.allowNet ? "ON (explicit)" : "OFF") +
             " · env: sanitized allowlist · no_new_privs: yes · root: refused by default\n";
        say(s);
        return true;
    }
    say("unknown command /" + cmd + " (try /help)\n");
    return true;
}

// ---------------------------------------------------------------------------
// Main loops
// ---------------------------------------------------------------------------
}  // namespace

int tuiRun(TuiOpts& opts) {
    TermGuard term;
    g_term = &term;
    signal(SIGINT, onFatalSignal);
    signal(SIGTERM, onFatalSignal);
    signal(SIGHUP, onFatalSignal);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGWINCH, onWinch);
    term.enter();
    if (!term.active) {
        g_term = nullptr;
        return lineRun(opts);  // termios failed: fall back
    }

    printBanner(opts);
    Agent& agent = *opts.agent;
    Editor ed;
    std::atomic<bool> cancel{false};
    opts.tools->cancel = &cancel;
    agent.setCancel(&cancel);
    SharedInput shared;
    shared.cancel = &cancel;
    std::thread watcher(watcherMain, &shared);
    ApprovalCtx actx{&shared};
    g_approval = &actx;
    opts.tools->askApproval = [&](const std::string& cmd, const std::string& reason) {
        return askApprovalCli(cmd, reason);
    };

    bool exitFlag = false;
    int rc = 0;
    while (!exitFlag) {
        ed.prompt = promptFor(opts, agent);
        auto input = editLine(ed, exitFlag);
        ed.reset();
        if (exitFlag || !input) break;
        std::string text = *input;
        if (trim(text).empty()) continue;
        if (text[0] == '/') {
            if (!runCommand(opts, agent, text)) break;
            continue;
        }
        runTurnInteractive(opts, agent, text, shared, cancel);
    }

    shared.stop.store(true);
    shared.cv.notify_all();
    if (watcher.joinable()) watcher.join();
    g_approval = nullptr;
    g_term = nullptr;
    term.leave();
    writeAll(STDOUT_FILENO, "\n");
    return rc;
}

namespace {
bool g_plain = false;
}  // namespace

int lineRun(TuiOpts& opts) {
    if (!isatty(STDOUT_FILENO)) g_plain = true;
    bool ttyIn = isatty(STDIN_FILENO);
    Agent& agent = *opts.agent;
    std::atomic<bool> cancel{false};
    opts.tools->cancel = &cancel;
    agent.setCancel(&cancel);
    opts.tools->askApproval = [&](const std::string& cmd, const std::string& reason) {
        return askApprovalCli(cmd, reason);
    };
    if (ttyIn) {
        std::string b = "pocket " + std::string(kVersion) + " (" + opts.workspace + ") model " +
                        opts.model.spec + "\n";
        writeAll(STDOUT_FILENO, b);
    }
    StreamRenderer rend;
    agent.setCallbacks(
        [&](std::string_view tok) {
            if (g_plain) {
                writeAll(STDOUT_FILENO, sanitizeTerminal(std::string(tok)));
            } else {
                rend.feed(tok);
            }
        },
        [&](const std::string& note) { writeAll(STDOUT_FILENO, "(" + sanitizeTerminal(note) + ")\n"); });
    opts.tools->onEvent = [&](const std::string& line) {
        writeAll(STDOUT_FILENO, "⚙ " + sanitizeTerminal(line) + "\n");
    };
    opts.tools->onToolDone = [&](const std::string& name, bool ok, const std::string&) {
        writeAll(STDOUT_FILENO, (ok ? std::string("✓ ") : std::string("✗ ")) + sanitizeTerminal(name) + "\n");
    };
    std::string line;
    for (;;) {
        if (ttyIn) writeAll(STDOUT_FILENO, promptFor(opts, agent));
        if (!std::getline(std::cin, line)) break;
        if (trim(line).empty()) continue;
        if (line[0] == '/') {
            if (!runCommand(opts, agent, line)) break;
            continue;
        }
        rend = StreamRenderer{};
        if (!g_plain) writeAll(STDOUT_FILENO, "assistant:\n");
        std::string err = agent.runTurn(line);
        rend.flush();
        if (!err.empty() && err != "cancelled")
            writeAll(STDOUT_FILENO, "error: " + sanitizeTerminal(err) + "\n");
        if (g_plain) writeAll(STDOUT_FILENO, "\n");
    }
    return 0;
}

}  // namespace pocket
