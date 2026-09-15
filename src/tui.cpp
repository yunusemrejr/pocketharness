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
#include <chrono>
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
const char* C_REV = "\033[7m";

bool useColor() { return getenv("NO_COLOR") == nullptr && isatty(STDOUT_FILENO); }
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
        // OPOST stays on: "\n" must render as CR+LF or output staircases.
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
    writeAll(STDOUT_FILENO, "\033[r\033[?2004l");  // undo region + paste mode
    if (g_term) g_term->leave();
    signal(sig, SIG_DFL);
    raise(sig);
}
volatile sig_atomic_t g_winch = 0;
void onWinch(int) { g_winch = 1; }
volatile sig_atomic_t g_tstp = 0;  // suspend/resume: BottomBar must re-sync
void onTstp(int) {
    writeAll(STDOUT_FILENO, "\033[r\033[?2004l");
    if (g_term) g_term->leave();
    signal(SIGTSTP, SIG_DFL);
    raise(SIGTSTP);
    signal(SIGTSTP, onTstp);  // back via fg: restore everything
    if (g_term) g_term->enter();
    writeAll(STDOUT_FILENO, "\033[?2004h");
    g_winch = 1;
    g_tstp = 1;
}

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

int termWidth() {
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col >= 20) return ws.ws_col;
    return 80;
}

int termRows() {
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row >= 5) return ws.ws_row;
    return 24;
}

std::string gotoRc(int r, int c = 1) {
    if (r < 1) r = 1;
    if (c < 1) c = 1;
    return "\033[" + std::to_string(r) + ";" + std::to_string(c) + "H";
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
    size_t winStart = 0;  // first visible logical line (bottom-area window)

    bool empty() const { return lines.size() == 1 && lines[0].empty(); }

    // Physical terminal rows per logical line (long lines wrap).
    std::vector<int> physRows(int W) const {
        std::vector<int> out;
        out.reserve(lines.size());
        for (size_t r = 0; r < lines.size(); ++r) {
            size_t pre = (r == 0 ? visibleWidth(prompt) : 4);
            int pr = (int)((pre + visibleWidth(lines[r]) + (size_t)W - 1) / (size_t)W);
            if (pr < 1) pr = 1;
            out.push_back(pr);
        }
        return out;
    }

    void redraw() {
        int W = termWidth();
        std::vector<int> phys = physRows(W);
        if (winStart >= lines.size()) winStart = lines.size() - 1;
        if (row < winStart) winStart = row;  // cursor always visible
        int total = 0;
        for (size_t r = winStart; r < phys.size(); ++r) total += phys[r];
        std::string out;
        out += "\r\033[K";
        for (int i = 1; i < lastRows; ++i) out += "\033[A\r\033[K";
        for (size_t r = winStart; r < lines.size(); ++r) {
            if (r == 0) out += prompt;
            else out += "\033[K... ";
            out += sanitizeTerminal(lines[r]);
            out += "\033[K";
            if (r + 1 < lines.size()) out += "\n";
        }
        // Wrap-aware cursor placement.
        size_t pre = (row == 0 ? visibleWidth(prompt) : 4);
        size_t abs = pre + visibleWidth(lines[row].substr(0, col));
        int above = 0;
        for (size_t r = winStart; r < row; ++r) above += phys[r];
        int off = (int)(abs / (size_t)W);
        int tcol = (int)(abs % (size_t)W);
        // Cursor at end of a width-exact line has auto-wrapped to the next row.
        bool wrappedEnd = (col == lines[row].size() && tcol == 0 && abs > 0);
        int below = total - 1 - (above + off + (wrappedEnd ? 1 : 0));
        if (below > 0) out += "\033[" + std::to_string(below) + "A";
        out += "\r";
        if (!wrappedEnd && tcol > 0) out += "\033[" + std::to_string(tcol) + "C";
        writeAll(STDOUT_FILENO, out);
        lastRows = total;
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

    void setLines(const std::string& t) {
        lines = splitLines(t);
        if (lines.empty()) lines = {""};
        row = lines.size() - 1;
        col = lines.back().size();
    }
    void histShow(long idx) {
        if (histIdx == -1) histSaved = text();
        histIdx = idx;
        setLines(history[(size_t)idx]);
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
            setLines(histSaved);
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
        winStart = 0;
    }
};

// Bytes read but not yet consumed (submit-chunk remainders). All raw-mode
// readers go through readChunk so piped/scripted input is never lost.
std::string g_stdinPend;

// Read one input chunk (blocking). Returns false on EOF/error.
bool readChunk(std::string& chunk) {
    if (!g_stdinPend.empty()) {
        chunk = std::move(g_stdinPend);
        g_stdinPend.clear();
        return true;
    }
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

bool stdinReady() {
    struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
    return poll(&pfd, 1, 0) > 0;
}

// Returns submitted text, or nullopt when the user asked to exit.
std::optional<std::string> editLine(Editor& ed, const std::function<void()>& draw,
                                    bool& exitFlag) {
    ed.lastRows = 1;
    draw();
    std::string esc;  // pending escape sequence (after ESC)
    bool inEsc = false;
    bool paste = false, pasteCR = false;  // bracketed paste state
    auto endEsc = [&]() {
        inEsc = false;
        esc.clear();
    };
    for (;;) {
        if (g_winch) {
            g_winch = 0;
            draw();
        }
        std::string chunk;
        if (inEsc && esc.empty()) {
            // Lone ESC vs sequence: wait briefly for more bytes.
            if (!readMore(chunk, 40)) {
                endEsc();
                if (!ed.empty() && !paste) {
                    ed.lines = {""};
                    ed.row = ed.col = 0;  // Esc clears the draft
                    draw();
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
                if (paste) {  // only the terminator is special inside a paste
                    if (esc == "[201~") {
                        paste = false;
                        endEsc();
                    } else if (std::string("[201~").compare(0, esc.size(), esc) == 0) {
                        // terminator in progress: wait for more bytes
                    } else {  // content ESC, not the terminator: drop ESC, keep bytes
                        for (char b : esc) {
                            unsigned char u = (unsigned char)b;
                            if (u == '\r' || u == '\n') ed.newline();
                            else if (u == '\t' || (u >= 0x20 && u != 0x7f))
                                ed.insertBytes(&b, 1);
                        }
                        endEsc();
                    }
                    continue;
                }
                if (esc == "[200~") {  // bracketed paste start
                    paste = true;
                    pasteCR = false;
                    endEsc();
                    continue;
                }
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
            if (paste) {  // literal insert: CR/LF both newline, rest verbatim
                if (c == '\r') {
                    ed.newline();
                    pasteCR = true;
                } else if (c == '\n') {
                    if (!pasteCR) ed.newline();
                    pasteCR = false;
                } else {
                    pasteCR = false;
                    if (c == '\t' || (c >= 0x20 && c != 0x7f))
                        ed.insertBytes(chunk.data() + i, 1);
                }
                continue;
            }
            switch (c) {
                case '\r': {  // Enter: submit
                    std::string t = ed.text();
                    if (trim(t).empty()) break;  // empty submit: ignore
                    if (ed.history.empty() || ed.history.back() != t) ed.history.push_back(t);
                    if (ed.history.size() > 200) ed.history.erase(ed.history.begin());
                    g_stdinPend = chunk.substr(i + 1);  // keep typeahead for next reader
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
                    ed.lastRows = 1;  // screen cleared: next redraw starts fresh
                    draw();
                    break;  // Ctrl-L
                default:
                    if (c >= 0x20 || c >= 0x80) ed.insertBytes(chunk.data() + i, 1);
                    break;
            }
        }
        if (paste && stdinReady()) continue;  // big paste: one draw at the end
        draw();
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
    // True only while the agent streams a turn. While the editor owns stdin
    // the watcher must not read at all: competing reads swallow keystrokes.
    std::atomic<bool> enabled{false};
    std::atomic<bool>* cancel = nullptr;
};

void watcherMain(SharedInput* in) {
    while (!in->stop.load()) {
        if (!in->enabled.load() && !in->approvalMode.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            continue;
        }
        struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
        if (poll(&pfd, 1, 60) <= 0) continue;
        if (!in->enabled.load() && !in->approvalMode.load()) continue;  // turn ended: leave bytes for the editor
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
// Set while a turn streams: lets the approval prompt commit the live tail
// before drawing (same thread; cleared when the turn ends).
std::function<void()> g_endPartial;

}  // namespace

// Visible terminal columns: ANSI sequences are zero-width, tabs stop at
// multiples of 8, other C0 controls are dropped (matching sanitizeTerminal).
// Wide CJK chars count 1; acceptable imprecision.
size_t visibleWidth(const std::string& s) {
    size_t w = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b) {
            ++i;
            if (i < s.size() && s[i] == '[') {
                ++i;
                while (i < s.size() && !isalpha((unsigned char)s[i]) && s[i] != '~')
                    ++i;
                if (i < s.size()) ++i;
            } else if (i < s.size() && s[i] == ']') {
                ++i;
                while (i < s.size() && s[i] != '\x07') {
                    if (s[i] == 0x1b && i + 1 < s.size() && s[i + 1] == '\\') {
                        i += 2;
                        break;
                    }
                    ++i;
                }
                if (i < s.size() && s[i] == '\x07') ++i;
            } else if (i < s.size()) {
                ++i;
            }
            continue;
        }
        if (c == '\t') {
            w += (size_t)(8 - (w % 8));
            ++i;
            continue;
        }
        if (c < 0x20 || c == 0x7f) {
            ++i;
            continue;
        }
        i += (size_t)utf8Len(c);
        ++w;
    }
    return w;
}

std::string fmtK(long n) {
    if (n < 0) n = 0;
    if (n < 1000) return std::to_string(n);
    if (n < 1000000) {
        if (n % 1000 == 0) return std::to_string(n / 1000) + "k";
        char b[32];
        snprintf(b, sizeof(b), "%.1fk", n / 1000.0);
        return b;
    }
    char b[32];
    snprintf(b, sizeof(b), "%.1fM", n / 1000000.0);
    return b;
}

std::vector<size_t> pickFilter(const std::vector<std::string>& labels,
                               const std::string& filter) {
    std::vector<size_t> out;
    std::string f = toLower(filter);
    for (size_t i = 0; i < labels.size(); ++i)
        if (f.empty() || toLower(labels[i]).find(f) != std::string::npos) out.push_back(i);
    return out;
}

std::string cutBytes(const std::string& s, size_t maxB) {
    if (s.size() <= maxB) return s;
    size_t c = maxB;
    while (c > 0 && ((unsigned char)s[c] & 0xC0) == 0x80) --c;
    return s.substr(0, c);
}

bool askApprovalCli(const std::string& cmd, const std::string& reason) {
    if (g_endPartial) g_endPartial();  // don't draw over the live stream tail
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
    std::string carry;  // live tail: already on screen, no newline yet
    bool fence = false;
    int partRows = 0;  // physical rows of the live tail on screen
    void clearPart() {
        if (partRows <= 0) return;
        std::string out = "\r\033[K";
        for (int i = 1; i < partRows; ++i) out += "\033[A\r\033[K";
        writeAll(STDOUT_FILENO, out);
        partRows = 0;
    }
    bool dim = false;  // inside a thinking span: render everything dim
    std::string shade(const std::string& vis) {
        return dim ? col(C_DIM) + vis + col(C_RESET) : vis;
    }
    void showPart() {
        // ponytail: re-render the whole tail per token; O(line^2) worst case,
        // lines are short and it converges when the line completes.
        bool f = fence;  // display-only: completed lines mutate the real state
        std::string vis = renderLine(carry, f);
        writeAll(STDOUT_FILENO, shade(vis));
        int W = termWidth();
        int r = (int)((visibleWidth(vis) + (size_t)W - 1) / (size_t)W);
        partRows = r < 1 ? 1 : r;
    }
    // think=true streams a thinking chunk (dim); think=false streams answer
    // text. Spans never interleave inside one request: thinking always
    // precedes the answer, so a text chunk commits any open thinking tail.
    void feed(std::string_view tok, bool think = false) {
        if (think && !dim && !carry.empty()) endLine();  // defensive: fresh span
        if (dim && !think) {
            dim = false;
            endLine();
        }  // thinking span ends: commit its tail
        dim = think;
        clearPart();
        carry.append(tok.data(), tok.size());
        // Bound memory + quadratic re-render on newline-free floods: commit
        // the head as its own visual line, keep the tail live.
        while (carry.find('\n') == std::string::npos && carry.size() > 8192) {
            size_t cut = 8192;
            while (cut > 0 && ((unsigned char)carry[cut] & 0xC0) == 0x80) --cut;
            if (cut == 0) cut = 8192;  // degenerate: terminal shows U+FFFD
            writeAll(STDOUT_FILENO, shade(renderLine(carry.substr(0, cut), fence)) + "\n");
            carry.erase(0, cut);
        }
        size_t start = 0;
        for (;;) {
            size_t nl = carry.find('\n', start);
            if (nl == std::string::npos) break;
            writeAll(STDOUT_FILENO, shade(renderLine(carry.substr(start, nl - start), fence)) + "\n");
            start = nl + 1;
        }
        carry.erase(0, start);
        if (!carry.empty()) showPart();
    }
    // Commit the live tail before out-of-band writes (events, approval).
    void endLine() {
        if (carry.empty()) return;
        (void)renderLine(carry, fence);  // keep fence state honest
        writeAll(STDOUT_FILENO, "\n");
        carry.clear();
        partRows = 0;
    }
    void write(const std::string& s) {
        endLine();
        writeAll(STDOUT_FILENO, s);
    }
    void flush() { endLine(); }
};

std::string shortModel(const std::string& spec) {
    size_t c = spec.find(':');
    return c == std::string::npos ? spec : spec.substr(c + 1);
}

long cachePct(const AgentStats& st) {  // -1 when the provider doesn't report
    if (!st.cacheSeen) return -1;
    long denom = st.cacheHit + st.cacheMiss;
    if (denom <= 0) denom = st.inTokens;  // hit-only reporting
    if (denom <= 0) return -1;
    return st.cacheHit * 100 / denom;
}

long recentPct(const AgentStats& st) {  // steady-state rate, -1 until warm
    if (st.cacheWindow.size() < 3) return -1;
    long denom = st.recentHit + st.recentMiss;
    if (denom <= 0) return -1;
    return st.recentHit * 100 / denom;
}

std::string tpsText(const AgentStats& st) {
    if (st.genMs <= 0 || st.outTokens <= 0) return "— tok/s";
    char b[32];
    snprintf(b, sizeof(b), "%.1f tok/s", st.outTokens * 1000.0 / (double)st.genMs);
    return b;
}

std::string kpiText(TuiOpts& opts, Agent& agent, int cols) {
    const AgentStats& st = agent.stats();
    long max = agent.contextMax();
    long used = agent.contextUsed();
    long pct = max > 0 ? used * 100 / max : 0;
    std::string trio = "ctx " + fmtK(used) + "/" + fmtK(max) + " " + std::to_string(pct) + "%";
    trio += " · " + tpsText(st);
    // Steady-state rate once warm (early cold requests would pin the
    // cumulative average down for the whole session); else cumulative.
    long cp = recentPct(st);
    if (cp < 0) cp = cachePct(st);
    trio += cp < 0 ? " · cache —" : " · cache " + std::to_string(cp) + "%";
    std::string spec = sanitizeTerminal(shortModel(opts.model.spec));
    std::string full = trio + " · " + spec + " · " + sanitizeTerminal(opts.thinking);
    if (visibleWidth(full) <= (size_t)cols) return full;
    std::string noThink = trio + " · " + spec;
    if (visibleWidth(noThink) <= (size_t)cols) return noThink;
    if (visibleWidth(trio) <= (size_t)cols) return trio;
    return "ctx " + std::to_string(pct) + "%";  // ponytail: tiniest KPI; never slice UTF-8
}

std::string shortPrompt() { return std::string(col(C_BOLD)) + "> " + col(C_RESET); }

// Persistent bottom area: input box above a KPI line, transcript scrolling
// in rows 1..region via DECSTBM. Stream output only appends or rewrites its
// own tail with relative moves, so a stale region mid-turn is harmless: the
// next draw() re-syncs. Degrades to legacy inline I/O on small/dumb terms.
struct BottomBar {
    Editor* ed = nullptr;
    std::function<std::string(int)> kpi;  // kpi(cols)
    bool active = false;
    int rows = 0, cols = 0, region = 0, inputTop = 0;
    bool firstDraw = true, inTranscript = false;
    int64_t lastKpi = 0;

    void setup() {
        rows = termRows();
        cols = termWidth();
        const char* t = getenv("TERM");
        active = isatty(STDOUT_FILENO) && rows >= 10 && (!t || std::string(t) != "dumb");
    }
    void teardown() {
        if (!active) return;
        writeAll(STDOUT_FILENO, "\033[r" + gotoRc(rows, 1) + "\033[K");
        active = false;
    }
    void toTranscript() {
        if (!active || inTranscript) return;
        writeAll(STDOUT_FILENO, gotoRc(region, 1) + "\n");
        inTranscript = true;
    }
    void drawKpi(bool save) {
        std::string out;
        if (save) out += "\0337";
        out += gotoRc(rows, 1) + "\033[K" + col(C_DIM) + kpi(cols) + col(C_RESET);
        if (save) out += "\0338";
        writeAll(STDOUT_FILENO, out);
    }
    void liveKpi() {
        if (!active) return;
        int64_t now = nowMs();
        if (now - lastKpi < 250) return;
        lastKpi = now;
        drawKpi(true);  // save/restore: never disturbs the stream cursor
    }
    void draw() {
        if (!active) {
            ed->winStart = 0;
            ed->redraw();
            return;
        }
        if (g_tstp) {  // resumed from suspend: region was reset, re-sync all
            g_tstp = 0;
            region = 0;
            firstDraw = true;
        }
        int r = termRows(), c = termWidth();
        if (r != rows || c != cols) {
            rows = r;
            cols = c;
            firstDraw = true;
        }
        int W = cols;
        std::vector<int> phys = ed->physRows(W);
        int maxH = rows - 6;
        if (maxH < 1) maxH = 1;
        int total = 0;
        for (int p : phys) total += p;
        ed->winStart = 0;
        while (total > maxH && ed->winStart < ed->row) total -= phys[ed->winStart++];
        if (total > maxH) {  // cursor line to the top, tail follows
            ed->winStart = ed->row;
            total = 0;
            for (size_t i = ed->row; i < phys.size(); ++i) total += phys[i];
        }
        // ponytail: one logical line taller than maxH overflows into the
        // transcript; upgrade: slice wrapped lines by column when it bites.
        int inputH = total < 1 ? 1 : total;
        int newTop = rows - inputH;
        int newRegion = newTop - 1;
        if (newRegion < 2) newRegion = 2;
        if (firstDraw) {
            std::string out;
            for (int rr = newTop; rr <= rows; ++rr) out += gotoRc(rr, 1) + "\033[K";
            writeAll(STDOUT_FILENO, out);
            ed->lastRows = 1;
            firstDraw = false;
        } else if (newTop != inputTop) {
            int a = inputTop < newTop ? inputTop : newTop;
            int b = inputTop < newTop ? newTop : inputTop;
            std::string out;
            for (int rr = a; rr < b; ++rr) out += gotoRc(rr, 1) + "\033[K";
            writeAll(STDOUT_FILENO, out);
        }
        if (newRegion != region) {
            writeAll(STDOUT_FILENO, "\033[1;" + std::to_string(newRegion) + "r");
            region = newRegion;
        }
        inputTop = newTop;
        drawKpi(false);
        writeAll(STDOUT_FILENO, gotoRc(inputTop + inputH - 1, 1));
        ed->redraw();
        inTranscript = false;
    }
};

std::string promptFor(TuiOpts& opts, Agent& agent) {
    long max = agent.contextMax();
    long pct = max > 0 ? agent.contextUsed() * 100 / max : 0;
    std::string p = std::string(col(C_BOLD)) + "pocket" + col(C_RESET) + col(C_DIM) + " [" +
                    sanitizeTerminal(shortModel(opts.model.spec)) + "·" +
                    sanitizeTerminal(opts.thinking) + "·" + std::to_string(pct) + "%" +
                    (opts.unsafe ? "·UNSAFE" : "") + (opts.allowNet ? "·NET" : "") + "] " +
                    sanitizeTerminal(baseName(opts.workspace)) + "> " + col(C_RESET);
    return p;
}

void printBanner(TuiOpts& opts) {
    const SandboxCaps& caps = sandboxCaps();
    // Sanitize dynamic parts only: sanitizing the composed string would strip
    // our own color sequences.
    std::string ws = sanitizeTerminal(opts.workspace);
    std::string sid = sanitizeTerminal(opts.sessionId);
    std::string spec = sanitizeTerminal(opts.model.spec);
    std::string think = sanitizeTerminal(opts.thinking);
    std::string b = std::string(col(C_BOLD)) + "pocket " + kVersion + col(C_RESET) + "  " +
                    col(C_DIM) + ws + col(C_RESET) + "\n" + col(C_DIM) + "session " +
                    sid + " · " + spec + " · thinking " + think +
                    col(C_RESET) + "\n";
    if (!opts.systemSource.empty())
        b += col(C_DIM) + std::string("system prompt: ") + sanitizeTerminal(opts.systemSource) +
             col(C_RESET) + "\n";
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
    writeAll(STDOUT_FILENO, b);
}

// Pasted/dropped image paths in submitted input become vision attachments
// for the turn; the tokens are rewritten to short markers. Callers keep
// slash commands away (a command is never a message).
std::string attachPastedImages(TuiOpts& opts, Agent& agent, const std::string& text) {
    std::vector<ImageToken> found = collectImageTokens(text, opts.workspace);
    if (found.empty()) return text;
    auto note = [&](const std::string& s) {
        writeAll(STDOUT_FILENO, col(C_DIM) + s + col(C_RESET) + "\n");
    };
    std::string out = text;
    for (const auto& it : found) {
        std::string err = agent.attachImage(it.path);
        if (!err.empty()) {
            note("(image skipped: " + sanitizeTerminal(err) + ")");
            continue;
        }
        std::string mark = "[attached image: " + baseName(it.path) + "]";
        // Replace the token as spelled (bare, quoted, or decorated with
        // file://); the tokenizer strips quotes, so try those forms too.
        bool done = false;
        for (const std::string& form :
             {it.token, "'" + it.token + "'", "\"" + it.token + "\"", it.path}) {
            size_t at = out.find(form);
            if (at != std::string::npos) {
                out.replace(at, form.size(), mark);
                done = true;
                break;
            }
        }
        if (!done) out += " " + mark;
        note("(attached image: " + sanitizeTerminal(baseName(it.path)) + ")");
    }
    return out;
}

int runTurnInteractive(TuiOpts& opts, Agent& agent, const std::string& input, SharedInput& shared,
                       std::atomic<bool>& cancel, BottomBar* bar) {
    struct Gate {
        SharedInput& s;
        explicit Gate(SharedInput& v) : s(v) { s.enabled.store(true); }
        ~Gate() { s.enabled.store(false); }
    } gate(shared);
    StreamRenderer rend;
    g_endPartial = [&] { rend.endLine(); };
    bool thinkHead = false;
    agent.setCallbacks(
        [&](std::string_view tok) {
            if (cancel.load()) return;
            rend.feed(tok);
            if (bar) bar->liveKpi();
        },
        [&](const std::string& note) {
            rend.write(col(C_DIM) + "(" + sanitizeTerminal(note) + ")" + col(C_RESET) + "\n");
        },
        [&](std::string_view chunk) {
            if (cancel.load()) return;
            if (!thinkHead) {
                thinkHead = true;
                rend.write(col(C_DIM) + std::string("💭 thinking:") + col(C_RESET) + "\n");
            }
            rend.feed(chunk, true);
            if (bar) bar->liveKpi();
        });
    opts.tools->onEvent = [&](const std::string& line) {
        rend.write(col(C_DIM) + "  ⚙ " + sanitizeTerminal(line) + col(C_RESET) + "\n");
    };
    opts.tools->onToolDone = [&](const std::string& name, bool ok, const std::string& summary) {
        std::string mark = ok ? (col(C_GREEN) + std::string("  ✓ ")) : (col(C_RED) + std::string("  ✗ "));
        std::string block = mark + sanitizeTerminal(name) + col(C_RESET) + "\n";
        int shown = 0;
        for (const std::string& ln : splitLines(summary)) {
            if (shown >= 3) {
                block += col(C_DIM) + "      [...]" + col(C_RESET) + "\n";
                break;
            }
            block += col(C_DIM) + "      " + sanitizeTerminal(ln).substr(0, 160) + col(C_RESET) + "\n";
            ++shown;
        }
        rend.write(block);
    };
    cancel.store(false);
    {
        std::lock_guard<std::mutex> lk(shared.mu);
        shared.q.clear();
    }
    writeAll(STDOUT_FILENO, col(C_MAGENTA) + std::string("assistant:") + col(C_RESET) + "\n");
    std::string err = agent.runTurn(input);
    rend.flush();
    g_endPartial = nullptr;
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
// Interactive picker: filter-as-you-type + arrow select, used by /model and
// /thinking. Raw mode draws in place in the transcript; without a raw TTY it
// degrades to a numbered list on cooked stdin.
// ---------------------------------------------------------------------------
struct PickResult {
    bool submitted = false;
    int index = -1;  // labels index, -1 = none
    std::string filter;
};

PickResult pickCooked(const std::string& title, const std::vector<std::string>& labels,
                      const std::string& initial) {
    PickResult r;
    writeAll(STDOUT_FILENO, sanitizeTerminal(title) + "\n");
    for (size_t i = 0; i < labels.size(); ++i)
        writeAll(STDOUT_FILENO,
                 "  [" + std::to_string(i + 1) + "] " + sanitizeTerminal(labels[i]) + "\n");
    writeAll(STDOUT_FILENO, "select number or type a value" +
                                (initial.empty() ? "" : " [" + sanitizeTerminal(initial) + "]") +
                                ": ");
    std::string line;
    if (!std::getline(std::cin, line)) return r;
    line = trim(line);
    if (line.empty()) line = initial;
    bool num = !line.empty();
    for (char c : line) num = num && c >= '0' && c <= '9';
    if (num) {
        long n = atol(line.c_str());
        if (n >= 1 && (size_t)n <= labels.size()) {
            r.submitted = true;
            r.index = (int)n - 1;
            return r;
        }
    }
    if (line.empty()) return r;
    r.submitted = true;
    r.filter = line;
    for (size_t i = 0; i < labels.size(); ++i)
        if (labels[i] == line) r.index = (int)i;
    return r;
}

PickResult pickRaw(const std::string& title, const std::vector<std::string>& labels,
                   const std::string& initial) {
    PickResult r;
    std::string filter = initial, lastFilter = initial + "\n";  // force first refilter
    size_t sel = 0, start = 0;
    int lastRows = 0;
    const int maxRows = 10;
    std::vector<size_t> vis;
    std::string esc;
    bool inEsc = false, paste = false;
    auto endEsc = [&]() {
        inEsc = false;
        esc.clear();
    };
    auto sync = [&] {  // refilter on change; callers must sync before using vis/sel
        if (filter != lastFilter) {
            vis = pickFilter(labels, filter);
            sel = 0;
            start = 0;
            lastFilter = filter;
        }
        if (vis.empty()) sel = 0;
        else if (sel >= vis.size()) sel = vis.size() - 1;
    };
    for (;;) {
        if (g_winch) g_winch = 0;  // redrawn below with the fresh size anyway
        sync();
        if (sel < start) start = sel;
        if (sel >= start + (size_t)maxRows) start = sel - maxRows + 1;
        int W = termWidth();
        size_t maxW = (size_t)(W > 8 ? W - 6 : 10);
        std::string out;
        // Cursor rests on the filter row; drop to the last row before erasing,
        // or each redraw paints a fresh copy below the stale one.
        if (lastRows > 2) out += "\033[" + std::to_string(lastRows - 2) + "B";
        out += "\r\033[K";
        for (int i = 1; i < lastRows; ++i) out += "\033[A\r\033[K";
        out += col(C_BOLD) + cutBytes(sanitizeTerminal(title), maxW + 4) + col(C_RESET) +
               "\033[K\n";
        std::string fshow = filter;
        if (fshow.size() > maxW) {  // show the tail while typing
            size_t b = fshow.size() - maxW;
            while (b < fshow.size() && ((unsigned char)fshow[b] & 0xC0) == 0x80) ++b;
            fshow = "..." + fshow.substr(b);
        }
        out += "> " + sanitizeTerminal(fshow) + "\033[K\n";
        size_t shown = 0;
        for (size_t k = start; k < vis.size() && shown < (size_t)maxRows; ++k, ++shown) {
            std::string lb = cutBytes(sanitizeTerminal(labels[vis[k]]), maxW);
            if (k == sel)
                out += col(C_BOLD) + "> " + col(C_REV) + lb + col(C_RESET) + "\033[K\n";
            else
                out += "  " + lb + "\033[K\n";
        }
        if (vis.empty()) {
            out += col(C_DIM) + "  (no match — enter uses the typed text)" + col(C_RESET) +
                   "\033[K\n";
            shown = 1;
        }
        out += col(C_DIM) + "filter · up/down · enter · esc" + col(C_RESET) + "\033[K";
        writeAll(STDOUT_FILENO, out);
        lastRows = 2 + (int)shown + 1;
        writeAll(STDOUT_FILENO, "\033[" + std::to_string(shown + 1) + "A\r\033[" +
                                    std::to_string(2 + visibleWidth(fshow)) + "C");
        auto finish = [&] {
            std::string end = "\r";
            for (size_t k = 0; k < shown + 1; ++k) end += "\033[B";
            writeAll(STDOUT_FILENO, end + "\n");
        };
        std::string chunk;
        if (inEsc && esc.empty()) {
            if (!readMore(chunk, 40)) {
                finish();
                return r;  // lone ESC: cancel
            }
        } else if (!readChunk(chunk)) {
            finish();
            return r;  // EOF: cancel
        }
        for (size_t i = 0; i < chunk.size(); ++i) {
            unsigned char c = (unsigned char)chunk[i];
            if (inEsc) {
                esc.push_back((char)c);
                if (paste) {
                    if (esc == "[201~") {
                        paste = false;
                        endEsc();
                    } else if (std::string("[201~").compare(0, esc.size(), esc) != 0) {
                        endEsc();
                    }
                    continue;
                }
                if (esc == "[200~") {
                    paste = true;
                    endEsc();
                } else if (esc == "[A") {
                    if (sel > 0) --sel;
                    endEsc();
                } else if (esc == "[B") {
                    ++sel;
                    endEsc();
                } else if (esc.size() >= 6) {
                    endEsc();
                } else if (esc.size() >= 2 && esc[0] == '[' &&
                           (isalpha((unsigned char)esc.back()) || esc.back() == '~')) {
                    endEsc();
                }
                continue;
            }
            if (c == 0x1b) {
                inEsc = true;
                esc.clear();
                continue;
            }
            if (paste) {
                if (c != '\r' && c != '\n' && (c == '\t' || (c >= 0x20 && c != 0x7f)))
                    filter.push_back((char)c);
                continue;
            }
            switch (c) {
                case '\r':
                case '\n':
                    sync();  // filter may have changed mid-chunk
                    r.submitted = true;
                    r.filter = filter;
                    r.index = vis.empty() ? -1 : (int)vis[sel];
                    g_stdinPend = chunk.substr(i + 1);
                    finish();
                    return r;
                case 0x03:
                case 0x04:
                    g_stdinPend = chunk.substr(i + 1);
                    finish();
                    return r;
                case 0x7f:
                case 0x08:
                    if (!filter.empty()) filter.erase(stepBack(filter, filter.size()));
                    break;
                case 0x15:
                    filter.clear();
                    break;
                case 0x10:
                    if (sel > 0) --sel;
                    break;
                case 0x0e:
                    ++sel;
                    break;
                default:
                    if (c >= 0x20 && c != 0x7f) filter.push_back((char)c);
                    break;
            }
        }
    }
}

PickResult pickOne(const std::string& title, const std::vector<std::string>& labels,
                   const std::string& initial) {
    if (g_term && g_term->active) return pickRaw(title, labels, initial);
    return pickCooked(title, labels, initial);
}

bool validThinking(const std::string& t) {
    return t == "off" || t == "low" || t == "medium" || t == "high" || t == "max";
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
            "  /model [spec]     pick or switch model (filterable; provider:model works too)\n"
            "  /thinking [level] pick or set thinking (off/low/medium/high/max)\n"
            "  /compact          summarize older context now\n"
            "  /skills [query]   list or search Markdown skills\n"
            "  /session          show session info and token usage\n"
            "  /security         show sandbox + authority state\n"
            "  /help             this text\n"
            "  /quit             exit\n"
            "keys: Enter submit · Ctrl-J/Alt-Enter newline · Up/Down history · Ctrl-C cancel/quit · Ctrl-D quit\n"
            "paste or drop an image file to attach it to the next message (vision models read it)\n");
        return true;
    }
    auto useModel = [&](const std::string& spec) {
        auto rm = resolveModel(*opts.cfg, spec);
        if (!rm.ok) {
            say(col(C_RED) + std::string("error: ") + rm.error + col(C_RESET) + "\n");
            return;
        }
        if (!hasExplicitContext(*opts.cfg, rm.value.provider.name, rm.value.model)) {
            long live = fetchModelContext(rm.value.provider, rm.value.model);
            if (live > 0) rm.value.context = live;
        }
        opts.model = rm.value;
        agent.setModel(rm.value, opts.thinking);
        SessionMeta m = sessionLoadMeta(opts.sessionId).value;
        m.modelSpec = rm.value.spec;
        sessionSaveMeta(opts.sessionId, m);
        say("model: " + rm.value.spec + "\n");
    };
    auto setLevel = [&](const std::string& t) {
        opts.thinking = t;
        agent.setModel(opts.model, t);
        SessionMeta m = sessionLoadMeta(opts.sessionId).value;
        m.thinking = t;
        sessionSaveMeta(opts.sessionId, m);
        say("thinking: " + t + "\n");
    };
    if (cmd == "model") {
        if (!args.empty() && resolveModel(*opts.cfg, args).ok) {
            useModel(args);  // exact spec: switch directly, no picker
            return true;
        }
        std::vector<std::string> labels, specs;
        for (const auto& m : opts.cfg->models) {
            std::string lb = m.alias + " = " + m.provider + ":" + m.model +
                             (m.routing.empty() ? "" : "@" + m.routing);
            auto rm = resolveModel(*opts.cfg, m.alias);
            if (rm.ok && rm.value.spec == opts.model.spec) lb += "  ●";
            labels.push_back(lb);
            specs.push_back(m.alias);
        }
        std::string provs;
        for (const auto& p : opts.cfg->providers) provs += p.name + " ";
        PickResult pr = pickOne("model — now: " + opts.model.spec + " (providers: " + trim(provs) +
                                    "); type provider:model to use it directly",
                                labels, args);
        if (!pr.submitted) {
            say("(cancelled)\n");
            return true;
        }
        if (!pr.filter.empty() && resolveModel(*opts.cfg, pr.filter).ok) useModel(pr.filter);
        else if (pr.index >= 0) useModel(specs[(size_t)pr.index]);
        else useModel(pr.filter);  // invalid text: precise error, nothing switched
        return true;
    }
    if (cmd == "thinking") {
        std::string t = toLower(args);
        if (!args.empty() && validThinking(t)) {
            setLevel(t);
            return true;
        }
        if (!args.empty()) {
            say("usage: /thinking off|low|medium|high|max\n");
            return true;
        }
        static const std::vector<std::string> kLevels = {"off", "low", "medium", "high",
                                                                 "max"};
        PickResult pr = pickOne("thinking — now: " + opts.thinking, kLevels, "");
        if (!pr.submitted) {
            say("(cancelled)\n");
            return true;
        }
        std::string f = toLower(pr.filter);
        if (!f.empty() && validThinking(f)) setLevel(f);
        else if (pr.index >= 0) setLevel(kLevels[(size_t)pr.index]);
        else say("usage: /thinking off|low|medium|high|max\n");
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
        say("gen: " + tpsText(st) + " avg\n");
        long cp = cachePct(st);
        if (cp >= 0) {
            say("cache: " + std::to_string(st.cacheHit) + " hit / " +
                std::to_string(st.cacheMiss) + " miss (" + std::to_string(cp) +
                "% reported reuse)\n");
            long rp = recentPct(st);
            if (rp >= 0)
                say("cache (last " + std::to_string(st.cacheWindow.size()) +
                    " requests): " + std::to_string(rp) + "% reuse\n");
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
        s += "tool network: " + std::string(opts.allowNet ? "ON (default)" : "OFF (--offline)") +
             " · env: sanitized allowlist · no_new_privs: yes · root: refused by default\n";
        s += "keys: $ENV only, brokered per-request (never in env/argv/child fs)\n";
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
    signal(SIGTSTP, onTstp);
    term.enter();
    if (!term.active) {
        g_term = nullptr;
        return lineRun(opts);  // termios failed: fall back
    }
    writeAll(STDOUT_FILENO, "\033[?2004h");  // bracketed paste; ignored if unsupported

    printBanner(opts);
    Agent& agent = *opts.agent;
    Editor ed;
    BottomBar bar;
    bar.ed = &ed;
    bar.kpi = [&](int cols) { return kpiText(opts, agent, cols); };
    bar.setup();
    auto draw = [&] { bar.draw(); };
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
        ed.prompt = bar.active ? shortPrompt() : promptFor(opts, agent);
        auto input = editLine(ed, draw, exitFlag);
        ed.reset();
        if (exitFlag || !input) break;
        std::string text = *input;
        if (trim(text).empty()) continue;
        bar.draw();  // clear the submitted draft now; output follows below
        bar.toTranscript();
        if (text[0] != '/') text = attachPastedImages(opts, agent, text);
        if (bar.active)  // pinned input isn't in the scrollback: echo it
            writeAll(STDOUT_FILENO,
                     col(C_BOLD) + "> " + col(C_RESET) + sanitizeTerminal(text) + "\n");
        if (text[0] == '/') {
            if (!runCommand(opts, agent, text)) break;
            continue;
        }
        runTurnInteractive(opts, agent, text, shared, cancel, &bar);
    }

    shared.stop.store(true);
    shared.cv.notify_all();
    if (watcher.joinable()) watcher.join();
    g_approval = nullptr;
    g_term = nullptr;
    bar.teardown();
    writeAll(STDOUT_FILENO, "\033[?2004l");
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
        [&](const std::string& note) {
            std::string s = "(" + sanitizeTerminal(note) + ")\n";
            if (g_plain) writeAll(STDOUT_FILENO, s);
            else rend.write(s);
        },
        [&](std::string_view chunk) {
            if (g_plain) {
                writeAll(STDOUT_FILENO, sanitizeTerminal(std::string(chunk)));
            } else {
                rend.feed(chunk, true);
            }
        });
    opts.tools->onEvent = [&](const std::string& line) {
        std::string s = "⚙ " + sanitizeTerminal(line) + "\n";
        if (g_plain) writeAll(STDOUT_FILENO, s);
        else rend.write(s);
    };
    opts.tools->onToolDone = [&](const std::string& name, bool ok, const std::string&) {
        std::string s = (ok ? std::string("✓ ") : std::string("✗ ")) + sanitizeTerminal(name) + "\n";
        if (g_plain) writeAll(STDOUT_FILENO, s);
        else rend.write(s);
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
        line = attachPastedImages(opts, agent, line);
        rend = StreamRenderer{};
        g_endPartial = [&] { rend.endLine(); };
        if (!g_plain) writeAll(STDOUT_FILENO, "assistant:\n");
        std::string err = agent.runTurn(line);
        rend.flush();
        g_endPartial = nullptr;
        if (!err.empty() && err != "cancelled")
            writeAll(STDOUT_FILENO, "error: " + sanitizeTerminal(err) + "\n");
        if (g_plain) writeAll(STDOUT_FILENO, "\n");
    }
    return 0;
}

}  // namespace pocket
