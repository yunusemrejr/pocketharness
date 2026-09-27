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
#include <wchar.h>

#include "brain.h"
#include "catalog.h"
#include "oversee.h"
#include "skills.h"

namespace pocket {

namespace {

// --- colors (TTY only; callers ensure that) ---
const char* C_RESET = "\033[0m";
const char* C_BOLD = "\033[1m";
const char* C_DIM = "\033[2m";
const char* C_GREEN = "\033[32m";
const char* C_YELLOW = "\033[33m";
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
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) active = true;
    }
    void leave() {
        if (!active) return;
        tcsetattr(STDIN_FILENO, TCSANOW, &orig);
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
    // SIGTSTP is blocked inside its handler. Re-raising it and reinstalling
    // this handler before returning recurses instead of suspending.
    raise(SIGSTOP);
    if (g_term) g_term->enter();
    writeAll(STDOUT_FILENO, "\033[?2004h");
    g_winch = 1;
    g_tstp = 1;
}

struct SignalGuard {
    static constexpr int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGPIPE, SIGWINCH, SIGTSTP};
    struct sigaction previous[std::size(signals)] {};
    SignalGuard() {
        for (size_t i = 0; i < std::size(signals); ++i) {
            struct sigaction action {};
            sigemptyset(&action.sa_mask);
            action.sa_handler = signals[i] == SIGPIPE ? SIG_IGN :
                                signals[i] == SIGWINCH ? onWinch :
                                signals[i] == SIGTSTP ? onTstp : onFatalSignal;
            sigaction(signals[i], &action, &previous[i]);
        }
    }
    ~SignalGuard() {
        for (size_t i = 0; i < std::size(signals); ++i)
            sigaction(signals[i], &previous[i], nullptr);
    }
};

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
    size_t n = (size_t)utf8Len((unsigned char)l[col]);
    if (col + n > l.size()) return col + 1;
    for (size_t i = 1; i < n; ++i)
        if (((unsigned char)l[col + i] & 0xc0) != 0x80) return col + 1;
    unsigned char c = (unsigned char)l[col];
    if ((n == 2 && c < 0xc2) || c > 0xf4) return col + 1;
    if (n >= 3) {
        unsigned char second = (unsigned char)l[col + 1];
        if ((c == 0xe0 && second < 0xa0) || (c == 0xed && second >= 0xa0) ||
            (c == 0xf0 && second < 0x90) || (c == 0xf4 && second >= 0x90)) return col + 1;
    }
    return col + n;
}

size_t columnBoundary(const std::string& line, size_t col) {
    col = std::min(col, line.size());
    while (col > 0 && col < line.size() && ((unsigned char)line[col] & 0xc0) == 0x80) --col;
    return col;
}

int termWidth() {
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    return 80;
}

int termRows() {
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0) return ws.ws_row;
    return 24;
}

std::string gotoRc(int r, int c = 1) {
    if (r < 1) r = 1;
    if (c < 1) c = 1;
    return "\033[" + std::to_string(r) + ";" + std::to_string(c) + "H";
}

const char* kCommands[] = {"/models", "/model", "/goal", "/queue", "/thinking", "/compact", "/undo", "/skills",
                            "/session", "/brain", "/catalog", "/security", "/double", "/help", "/quit", nullptr};

struct Editor {
    std::vector<std::string> lines{""};
    size_t row = 0, col = 0;
    std::vector<std::string> history;
    long histIdx = -1;
    std::string histSaved;
    std::string prompt;
    int lastRows = 1;
    bool pinned = false;
    // A turn may finish between bytes of a key or paste. Decoder state belongs
    // to the composer, not to one invocation of the input loop.
    std::string escape;
    bool inEscape = false, pasting = false, pasteCR = false;

    bool empty() const { return lines.size() == 1 && lines[0].empty(); }

    size_t lastCursorRow = 0;

    TuiInputLayout layout() const {
        return layoutTuiInput(lines, row, col, prompt, (size_t)termWidth());
    }

    void redraw() {
        auto view = layout();
        size_t height = (size_t)std::max(1, termRows() - 1);
        size_t start = view.cursorRow >= height ? view.cursorRow - height + 1 : 0;
        size_t count = std::min(height, view.rows.size() - start);
        std::string out = "\r";
        if (lastCursorRow) out += "\033[" + std::to_string(lastCursorRow) + "A";
        for (int i = 0; i < lastRows; ++i) {
            out += "\033[K";
            if (i + 1 < lastRows) out += "\033[B\r";
        }
        if (lastRows > 1) out += "\033[" + std::to_string(lastRows - 1) + "A";
        for (size_t i = 0; i < count; ++i) {
            if (i) out += "\r\n";
            out += view.rows[start + i] + "\033[K";
        }
        size_t cursor = view.cursorRow - start;
        if (count - 1 > cursor) out += "\033[" + std::to_string(count - 1 - cursor) + "A";
        out += "\r";
        if (view.cursorCol) out += "\033[" + std::to_string(view.cursorCol) + "C";
        writeAll(STDOUT_FILENO, out);
        lastRows = (int)count;
        lastCursorRow = cursor;
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
        lastCursorRow = 0;
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
    ssize_t n;
    do { n = read(STDIN_FILENO, buf, sizeof(buf)); } while (n < 0 && errno == EINTR && !g_winch);
    if (n < 0 && errno == EINTR) { chunk.clear(); return true; }
    if (n <= 0) return false;
    chunk.assign(buf, (size_t)n);
    return true;
}

bool readMore(std::string& extra, int ms) {
    if (!g_stdinPend.empty()) return readChunk(extra);
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
struct EditControl {
    std::function<bool()> pump;  // false when the background turn has finished
    std::function<void()> interrupt;
    std::function<bool(const std::string&)> consume;  // approval owns this chunk
};
std::optional<std::string> editLine(Editor& ed, const std::function<void()>& draw,
                                    bool& exitFlag, EditControl* control = nullptr) {
    draw();
    std::string& esc = ed.escape;
    bool& inEsc = ed.inEscape;
    bool& paste = ed.pasting;
    bool& pasteCR = ed.pasteCR;
    auto endEsc = [&]() {
        inEsc = false;
        esc.clear();
    };
    for (;;) {
        if (control && !control->pump()) return std::nullopt;
        if (g_winch) {
            g_winch = 0;
            draw();
        }
        std::string chunk;
        if (inEsc) {
            // Lone ESC vs sequence: wait briefly for more bytes.
            if (!readMore(chunk, 40)) {
                bool lone = esc.empty();
                endEsc();
                if (lone && !paste && control) {
                    control->interrupt();
                    draw();
                    continue;
                }
                if (lone && !ed.empty() && !paste) {
                    ed.reset();  // Esc clears the draft, including history state
                    draw();
                }
                continue;
            }
        } else {
            if (control && g_stdinPend.empty()) {
                struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
                if (poll(&pfd, 1, 30) <= 0) continue;
            }
            if (!readChunk(chunk)) {
                exitFlag = true;
                if (control) control->interrupt();
                return std::nullopt;
            }
        }
        if (control && control->consume && control->consume(chunk)) continue;
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
                if (esc == "[A" || esc == "OA") {  // Up
                    if (ed.lines.size() > 1 && ed.row > 0) {
                        --ed.row;
                        ed.col = columnBoundary(ed.lines[ed.row], ed.col);
                    } else if (ed.lines.size() == 1) {
                        ed.histPrev();
                    }
                    endEsc();
                } else if (esc == "[B" || esc == "OB") {  // Down
                    if (ed.row + 1 < ed.lines.size()) {
                        ++ed.row;
                        ed.col = columnBoundary(ed.lines[ed.row], ed.col);
                    } else if (ed.lines.size() == 1) {
                        ed.histNext();
                    }
                    endEsc();
                } else if (esc == "[C" || esc == "OC") {  // Right
                    ed.col = stepFwd(ed.lines[ed.row], ed.col);
                    endEsc();
                } else if (esc == "[D" || esc == "OD") {  // Left
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
                } else if (esc == "OH") {
                    ed.col = 0;
                    endEsc();
                } else if (esc == "OF") {
                    ed.col = ed.lines[ed.row].size();
                    endEsc();
                } else if (esc.size() >= 32 || (esc.size() == 1 && esc[0] != '[' && esc[0] != 'O')) {
                    endEsc();  // unknown sequence: ignore
                } else if (esc.size() >= 2 && (esc[0] == '[' || esc[0] == 'O') &&
                           esc.back() >= 0x40 && esc.back() <= 0x7e) {
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
                    if (!ed.pinned) {
                        std::string out;
                        if ((size_t)ed.lastRows > ed.lastCursorRow + 1)
                            out += "\033[" + std::to_string((size_t)ed.lastRows - ed.lastCursorRow - 1) + "B";
                        writeAll(STDOUT_FILENO, out + "\r\n");
                    }
                    return t;
                }
                case '\n':  // Ctrl-J: newline
                    ed.newline();
                    break;
                case 0x03:  // Ctrl-C
                    if (control) { control->interrupt(); break; }
                    if (ed.empty()) {
                        exitFlag = true;
                        return std::nullopt;
                    }
                    ed.reset();
                    break;
                case 0x04:  // Ctrl-D
                    if (ed.empty()) {
                        exitFlag = true;
                        if (control) control->interrupt();
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
                    ed.lastCursorRow = 0;
                    draw();
                    break;  // Ctrl-L
                case 0x1a:
                    raise(SIGTSTP);
                    break;
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
    std::string draft;
    std::atomic<bool> stop{false};
    std::atomic<bool> eof{false};
    std::atomic<bool> approvalMode{false};
    // True only while the agent streams a turn. While the editor owns stdin
    // the watcher must not read at all: competing reads swallow keystrokes.
    std::atomic<bool> enabled{false};
    std::atomic<bool>* cancel = nullptr;
    bool paste = false;  // only the watcher mutates this
    void queueDraft(char c) {  // called while mu is held
        if (c == 0x03 && !paste) {
            if (cancel) cancel->store(true);
            return;
        }
        draft.push_back(c);
        if (endsWith(draft, "\033[200~")) paste = true;
        else if (endsWith(draft, "\033[201~")) paste = false;
    }
};

struct InputGate {
    SharedInput* input;
    explicit InputGate(SharedInput* in) : input(in) {
        if (!input) return;
        std::lock_guard<std::mutex> lk(input->mu);
        if (input->cancel) input->cancel->store(false);
        for (char c : g_stdinPend) input->queueDraft(c);
        g_stdinPend.clear();
        input->enabled.store(true);
    }
    ~InputGate() {
        if (!input) return;
        std::lock_guard<std::mutex> lk(input->mu);
        input->enabled.store(false);
        g_stdinPend += input->draft;
        input->draft.clear();
        input->paste = false;
    }
};

void watcherMain(SharedInput* in) {
    while (!in->stop.load()) {
        if (!in->enabled.load() && !in->approvalMode.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            continue;
        }
        struct pollfd pfd{STDIN_FILENO, POLLIN, 0};
        if (poll(&pfd, 1, 60) <= 0) continue;
        // The gate and this final ownership check share a lock. Otherwise a
        // read already past the check may race the next editor and eat keys.
        std::lock_guard<std::mutex> lk(in->mu);
        if (!in->enabled.load() && !in->approvalMode.load()) continue;  // turn ended: leave bytes for the editor
        char buf[64];
        ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            in->eof.store(true);
            if (in->cancel) in->cancel->store(true);
            in->cv.notify_all();
            return;
        }
        for (ssize_t i = 0; i < n; ++i) {
            if (in->approvalMode.load()) {
                in->q.push_back(buf[i]);
                in->cv.notify_one();
            } else {
                in->queueDraft(buf[i]);
            }
            // Preserve typeahead exactly, including paste delimiters, for the
            // editor after this turn. It must never act as approval input.
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
// Use the active terminal locale for wide and combining Unicode characters.
size_t visibleWidth(const std::string& s) {
    size_t w = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b) {
            ++i;
            if (i < s.size() && s[i] == '[') {
                ++i;
                while (i < s.size() && !((unsigned char)s[i] >= 0x40 && (unsigned char)s[i] <= 0x7e))
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
        size_t next = stepFwd(s, i);
        unsigned cp = c;
        if (next - i > 1) {
            cp = c & (0x7f >> (next - i));
            for (size_t j = i + 1; j < next; ++j) cp = (cp << 6) | ((unsigned char)s[j] & 0x3f);
        }
        int cells = wcwidth((wchar_t)cp);
        w += (size_t)(cells < 0 ? 1 : cells);
        i = next;
    }
    return w;
}

TuiInputLayout layoutTuiInput(const std::vector<std::string>& lines, size_t row,
                             size_t col, const std::string& prompt, size_t width) {
    TuiInputLayout out;
    out.rows.push_back("");
    width = std::max((size_t)1, width);
    size_t cell = 0;
    auto nextRow = [&] { out.rows.push_back(""); cell = 0; };
    auto append = [&](const std::string& text, bool track, size_t cursor) {
        for (size_t i = 0; i < text.size();) {
            if (track && i == cursor) {
                out.cursorRow = out.rows.size() - 1;
                out.cursorCol = cell;
            }
            size_t end = stepFwd(text, i);
            std::string glyph = text.substr(i, end - i);
            size_t n = glyph == "\t" ? 8 - cell % 8 : visibleWidth(glyph);
            if (glyph == "\t") {
                while (n--) {
                    out.rows.back() += ' ';
                    if (++cell == width) nextRow();
                }
            } else {
                if (n > width) { glyph = "?"; n = 1; }
                if (!n && cell == 0 && out.rows.size() > 1 && out.rows.back().empty()) {
                    out.rows[out.rows.size() - 2] += glyph;
                    i = end;
                    continue;
                }
                if (n && cell + n > width) nextRow();
                out.rows.back() += glyph;
                cell += n;
                if (cell >= width) nextRow();
            }
            i = end;
        }
        if (track && cursor >= text.size()) {
            out.cursorRow = out.rows.size() - 1;
            out.cursorCol = cell;
        }
    };
    for (size_t r = 0; r < lines.size(); ++r) {
        if (r) nextRow();
        append(sanitizeTerminal(r == 0 ? prompt : "... "), false, 0);
        append(sanitizeTerminal(lines[r]), r == row, col);
    }
    return out;
}

std::string busyLine(int64_t now, int64_t start, size_t queued, bool held,
                     const std::string& hint) {
    std::string q = "queued " + std::to_string(queued);
    if (held) return "paused · " + q + " · " + hint;
    int64_t el = now - start;
    if (el < 0) el = 0;
    std::string frame;
    if (getenv("POCKET_NO_ANIM")) {
        frame = "•";  // motion off: the bullet + elapsed still show liveness
    } else {
        static const char* kSpin[] = {"⠋", "⠙", "⠹", "⠸", "⠼",
                                      "⠴", "⠦", "⠧", "⠇", "⠏"};
        frame = kSpin[(size_t)((el / 100) % 10)];
    }
    std::string out = frame + " working";
    if (el >= 1000) out += " " + std::to_string(el / 1000) + "s";
    return out + " · " + q + " · " + hint;
}

std::string fmtK(long n) {
    if (n < 0) n = 0;
    if (n < 1000) return std::to_string(n);
    if (n < 1000000) {
        if (n >= 999950) return "1.0M";  // %.1f would print "1000.0k"
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
    // Substring hits first (predictable), then fuzzy hits for typos and
    // scattered words ("glm flsh" -> glm-5.3-flash), best first.
    std::vector<size_t> out, fuzzy;
    std::string f = toLower(filter);
    for (size_t i = 0; i < labels.size(); ++i)
        if (f.empty() || toLower(labels[i]).find(f) != std::string::npos) out.push_back(i);
    if (f.empty()) return out;
    std::vector<bool> taken(labels.size());
    for (size_t i : out) taken[i] = true;
    for (size_t i : fuzzyRank(labels, filter, 0.6))
        if (!taken[i]) out.push_back(i);
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
                      col(C_DIM) + sanitizeTerminal(reason) + col(C_RESET) + "\n\n  " + sanitizeTerminal(cmd) +
                      "\n\nAllow once? [y/N] ";
    if (g_approval && g_approval->in) {
        SharedInput* in = g_approval->in;
        {
            std::lock_guard<std::mutex> lk(in->mu);
            in->q.clear();
            in->approvalMode.store(true);
        }
        writeAll(STDOUT_FILENO, box);
        bool allow = false;
        for (;;) {
            std::unique_lock<std::mutex> lk(in->mu);
            in->cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
                return !in->q.empty() || in->stop.load() || in->eof.load() ||
                       (in->cancel && in->cancel->load());
            });
            if (in->stop.load() || in->eof.load() || (in->cancel && in->cancel->load())) break;
            if (in->q.empty()) continue;
            char c = in->q.front();
            in->q.pop_front();
            lk.unlock();
            if (c == 'y' || c == 'Y') {
                allow = true;
                break;
            }
            if (c == 0x03 && in->cancel) in->cancel->store(true);
            if (c == 'n' || c == 'N' || c == '\r' || c == '\n' || c == 0x1b || c == 0x03 || c == 0x04)
                break;  // default = reject
        }
        {
            std::lock_guard<std::mutex> lk(in->mu);
            in->approvalMode.store(false);
            in->q.clear();  // never replay approval keystrokes as a user turn
        }
        writeAll(STDOUT_FILENO, allow ? "allowed once\n" : "rejected\n");
        return allow;
    }
    writeAll(STDOUT_FILENO, box);
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
    int viewportRows = 0;  // actual transcript region; 0 = unpinned fallback
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
        // Keep the rewritable tail inside the transcript viewport. Otherwise
        // a long line scrolls offscreen and clearPart erases older messages.
        int maxRows = std::max(1, termRows() - 8);
        if (viewportRows > 0) maxRows = std::min(maxRows, viewportRows);
        size_t maxCells = (size_t)maxRows * (size_t)termWidth();
        while (carry.find('\n') == std::string::npos &&
               (carry.size() > 8192 || visibleWidth(carry) > maxCells)) {
            size_t cut = cutBytes(carry, std::min((size_t)8192, maxCells)).size();
            if (!cut) cut = stepFwd(carry, 0);
            while (cut > 1 && visibleWidth(carry.substr(0, cut)) > maxCells) {
                size_t smaller = cutBytes(carry, cut / 2).size();
                if (!smaller) break;
                cut = smaller;
            }
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
    if (st.genMs <= 0 || st.genTokens <= 0) return "— tok/s";
    char b[32];
    snprintf(b, sizeof(b), "%.1f tok/s", st.genTokens * 1000.0 / (double)st.genMs);
    return b;
}

std::string kpiText(TuiOpts& opts, Agent& agent, int) {
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
    if (st.costSeen) {
        char b[32];
        snprintf(b, sizeof b, " · total %s$%.4f", st.costEstimated ? "~" : "", st.cost);
        trio += b;
        if (st.costIncomplete) trio += " + unreported";
    }
    if (agent.goalStatus() != GoalStatus::None) {
        const char* state = agent.goalStatus() == GoalStatus::Active ? "active" :
                            agent.goalPaused() ? "paused" : "completed";
        trio = std::string("◎ goal ") + state + " · " + trio;
    }
    if (!st.costSeen) trio += " · total unreported";
    std::string identity = opts.sessionId.empty() ? "" : "session " + sanitizeTerminal(opts.sessionId) + "\n";
    return identity + trio + "\nprovider " + sanitizeTerminal(opts.model.provider.name) +
           " · model " + sanitizeTerminal(opts.model.model) +
           " · thinking " + sanitizeTerminal(opts.thinking);
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
    int footerH = 1;
    int fixedInputH = 0, fixedFooterH = 0;

    void setup() {
        rows = termRows();
        cols = termWidth();
        const char* t = getenv("TERM");
        active = isatty(STDOUT_FILENO) && rows >= 6 && (!t || std::string(t) != "dumb");
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
    std::vector<std::string> statusRows() const {
        std::vector<std::string> result;
        for (const auto& line : splitLines(kpi(cols))) {
            auto wrapped = layoutTuiInput({line}, 0, line.size(), "", (size_t)cols).rows;
            if (wrapped.size() > 1 && wrapped.back().empty()) wrapped.pop_back();
            result.insert(result.end(), wrapped.begin(), wrapped.end());
        }
        return result;
    }
    void drawKpi(bool save) {
        auto lines = statusRows();
        std::string out;
        if (save) out += "\0337";
        for (int i = 0; i < footerH; ++i) {
            out += gotoRc(rows - footerH + i + 1) + "\033[K";
            if ((size_t)i < lines.size()) out += col(C_DIM) + lines[(size_t)i] + col(C_RESET);
        }
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
        ed->pinned = active;
        if (!active) {
            ed->redraw();
            return;
        }
        if (g_tstp) {  // resumed from suspend: region was reset, re-sync all
            g_tstp = 0;
            region = 0;
            firstDraw = true;
        }
        int r = termRows(), c = termWidth();
        if (r < 6) {
            teardown();
            ed->pinned = false;
            ed->lastRows = 1;
            ed->lastCursorRow = 0;
            ed->redraw();
            return;
        }
        if (r != rows || c != cols) {
            rows = r;
            cols = c;
            firstDraw = true;
        }
        footerH = std::min(fixedFooterH ? fixedFooterH : (int)statusRows().size(), std::max(1, rows / 2));
        auto view = ed->layout();
        size_t maxH = (size_t)std::max(1, rows - footerH - 4);
        size_t start = view.cursorRow >= maxH ? view.cursorRow - maxH + 1 : 0;
        int inputH = fixedInputH ? (int)std::min(maxH, (size_t)fixedInputH) : (int)std::min(maxH, view.rows.size() - start);
        size_t visibleH = (size_t)inputH;
        start = view.cursorRow >= visibleH ? view.cursorRow - visibleH + 1 : 0;
        int newTop = std::max(2, rows - footerH - inputH + 1);
        int newRegion = newTop - 1;
        int clearFrom = firstDraw || !inputTop ? newTop : std::min(inputTop, newTop);
        std::string out;
        for (int rr = clearFrom; rr <= rows; ++rr) out += gotoRc(rr) + "\033[K";
        if (newRegion != region || firstDraw) {
            out += "\033[1;" + std::to_string(newRegion) + "r";
            region = newRegion;
        }
        firstDraw = false;
        inputTop = newTop;
        for (int i = 0; i < inputH && start + (size_t)i < view.rows.size(); ++i)
            out += gotoRc(inputTop + i) + view.rows[start + (size_t)i];
        writeAll(STDOUT_FILENO, out);
        drawKpi(false);
        writeAll(STDOUT_FILENO, gotoRc(inputTop + (int)(view.cursorRow - start), (int)view.cursorCol + 1));
        inTranscript = false;
    }
};

std::string promptFor(TuiOpts& opts, Agent& agent) {
    long max = agent.contextMax();
    long pct = max > 0 ? agent.contextUsed() * 100 / max : 0;
    std::string p = std::string(col(C_BOLD)) + "pocket" + col(C_RESET) + col(C_DIM) + " [" +
                    sanitizeTerminal(shortModel(opts.model.spec)) + "·" +
                    sanitizeTerminal(opts.thinking) + "·" + std::to_string(pct) + "%" +
                    (agent.doubleEnabled() ? "·2×" : "") +
                    (opts.unsafe ? "·UNSAFE" : "") + (opts.allowNet ? "·NET" : "") + "] " +
                    sanitizeTerminal(baseName(opts.workspace)) +
                    (opts.sessionId.empty() ? "" : " · session " + sanitizeTerminal(opts.sessionId)) + "> " + col(C_RESET);
    return p;
}

// ---------------------------------------------------------------------------
// Banner: a tiny Game Boy drops into a denim pocket, info to its right.
// ---------------------------------------------------------------------------
const char* kGameBoy[] = {"╭─────────╮", "│ ┌─────┐ │", "│ │ ▒▒▒ │ │", "│ └─────┘ │",
                          "│ ┼   ● ● │", "│   ═ ═   │", "╰─────────╯"};
const char* kPocket[] = {"┏━━━━━━━━━━━━━┓", "┃┄┄┄┄┄┄┄┄┄┄┄┄┄┃", "┃             ┃", "╲             ╱",
                         " ╲▁▁▁▁▁▁▁▁▁▁▁╱ "};
constexpr int kArtRows = 10, kRim = 5, kArtW = 15;

// Visible substring [from, from+n) of a UTF-8 string (all glyphs width 1).
std::string glyphs(const std::string& s, size_t from, size_t n) {
    std::string out;
    size_t g = 0;
    for (size_t i = 0; i < s.size(); i += (size_t)utf8Len((unsigned char)s[i]), ++g)
        if (g >= from && g < from + n) out += s.substr(i, (size_t)utf8Len((unsigned char)s[i]));
    return out;
}

std::string paintGlyph(const std::string& g) {
    const char* c = g == "▒" ? "\033[38;5;149m" : g == "●" ? "\033[38;5;168m" : g == "┼" || g == "═" ? "\033[38;5;240m"
                  : g == "┄" ? "\033[38;5;179m" : (g == "┏" || g == "━" || g == "┓" || g == "┃" || g == "╲" ||
                                                    g == "╱" || g == "▁") ? "\033[38;5;67m" : "\033[38;5;252m";
    return g == " " ? g : col(c) + g + col(C_RESET);
}

std::string artRow(int row, int gbTop) {
    std::string out;
    for (int x = 0; x < kArtW; ++x) {
        std::string g = " ";
        if (row >= kRim && row - kRim < 5) g = glyphs(kPocket[row - kRim], (size_t)x, 1);
        int gy = row - gbTop, gx = x - 2;
        bool behind = row >= kRim;  // the pocket's front hides the lower Game Boy
        if (!behind && gy >= 0 && gy < 7 && gx >= 0 && gx < 11) g = glyphs(kGameBoy[gy], (size_t)gx, 1);
        out += paintGlyph(g);
    }
    return out;
}

void printBanner(TuiOpts& opts) {
    const SandboxCaps& caps = sandboxCaps();
    int cols = termWidth();
    auto fit = [&](const std::string& plain) { return cutBytes(sanitizeTerminal(plain), (size_t)std::max(0, cols - kArtW - 3)); };
    std::string roles;
    for (const char* r : {"fast", "fallback", "review"})
        if (opts.cfg && opts.cfg->roles.count(r)) roles += std::string(roles.empty() ? "" : " · ") + r + " " + shortModel(opts.cfg->roles.at(r));
    std::vector<std::string> info = {
        "",
        col(C_BOLD) + std::string("pocket") + col(C_RESET) + col(C_DIM) + " " + kVersion + col(C_RESET),
        col("\033[38;5;37m") + fit(opts.model.spec + " · thinking " + opts.thinking) + col(C_RESET),
        col(C_DIM) + fit(opts.workspace) + col(C_RESET),
        col(C_DIM) + fit("session " + opts.sessionId) + col(C_RESET),
        col(C_DIM) + fit(roles.empty() ? "roles: /models to assign fast · fallback · review" : roles) + col(C_RESET),
        col(C_DIM) + fit(opts.cfg ? judgeStatus(*opts.cfg) : "") + col(C_RESET),
        col(C_DIM) + fit(std::string("tools net ") + (opts.allowNet ? "on" : "off") + " · sandbox " +
                         (caps.landlock ? "landlock" : "no-landlock") + (caps.seccompNet ? "+seccomp" : "")) + col(C_RESET),
        col(C_DIM) + fit("Enter send · Ctrl-J newline · /goal · /models · /help") + col(C_RESET),
        ""};
    bool tiny = cols < kArtW + 20 || termRows() < 16;
    bool animate = useColor() && !tiny && !getenv("POCKET_NO_ANIM");
    auto frame = [&](int top) {
        std::string f;
        for (int r = 0; r < kArtRows; ++r) f += " " + artRow(r, top) + "  " + info[(size_t)r] + "\033[K\n";
        return f;
    };
    if (tiny) {
        for (const auto& l : info)
            if (!l.empty()) writeAll(STDOUT_FILENO, l + "\n");
    } else if (animate) {
        const int drops[] = {-7, -4, -1, 2, 3, 2};
        for (size_t i = 0; i < std::size(drops); ++i) {
            if (i) writeAll(STDOUT_FILENO, "\033[" + std::to_string(kArtRows) + "A");
            writeAll(STDOUT_FILENO, frame(drops[i]));
            usleep(i + 1 < std::size(drops) ? 60000 : 0);
        }
    } else writeAll(STDOUT_FILENO, frame(2));
    std::string b;
    if (opts.agent && opts.agent->messageCount()) {
        b += "restored " + std::to_string(opts.agent->messageCount()) + " messages";
        if (opts.agent->goalPaused()) b += " · goal paused · send a follow-up or /goal resume to continue";
        b += "\n";
    }
    if (!opts.systemSource.empty())
        b += col(C_DIM) + std::string("system prompt: ") + sanitizeTerminal(opts.systemSource) + col(C_RESET) + "\n";
    if (opts.unsafe)
        b += std::string(col(C_RED)) + col(C_BOLD) +
             "UNSAFE MODE — model tools have broader user-account authority." + col(C_RESET) + "\n";
    if (!caps.landlock)
        b += col(C_YELLOW) + std::string("note: Landlock unavailable — model commands run without kernel filesystem confinement") + col(C_RESET) + "\n";
    if (!caps.seccompNet && !opts.allowNet)
        b += col(C_YELLOW) + std::string("note: seccomp unavailable — model network isolation is OFF (run with --network only if intended)") + col(C_RESET) + "\n";
    if (!caps.openat2)
        b += col(C_YELLOW) + std::string("note: openat2 unavailable — native file tools refuse symlinks entirely (strict fallback)") + col(C_RESET) + "\n";
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

struct QueuedMessage {
    std::string text;
    std::string pickerKeys;
};

struct QueuedInput {
    std::deque<QueuedMessage> messages;
    bool held = false;
    bool quit = false;
    bool continueGoal = false;
    // deque preserves references across pushes and removal of earlier items.
    // A modal command owns its typeahead until its actual picker opens.
    QueuedMessage* picker = nullptr;
    std::string pickerEscape;
    bool pickerPaste = false;
    int64_t pickerEscAt = 0;
    void releasePicker() { picker = nullptr; pickerEscape.clear(); pickerPaste = false; }
    void clear() { releasePicker(); messages.clear(); }
    size_t bytes() const {
        size_t count = 0;
        for (const auto& message : messages) count += message.text.size() + message.pickerKeys.size();
        return count;
    }
    bool expirePickerEscape(const std::function<void()>& interrupt) {
        if (!picker || pickerEscape.empty() || pickerPaste || nowMs() - pickerEscAt < 40) return false;
        bool lone = pickerEscape == "\033";
        if (lone) {
            picker->pickerKeys.pop_back();
            releasePicker();
            interrupt();
        } else {
            // Replay has no original timing gaps: omit a timed-out prefix so
            // later bytes cannot turn it into a new escape/paste sequence.
            picker->pickerKeys.resize(picker->pickerKeys.size() - pickerEscape.size());
            pickerEscape.clear();
        }
        return lone;
    }
    // false means this chunk belongs to the composer or an approval instead.
    // On overflow/cancel, leave every unconsumed byte for the next reader.
    bool capturePicker(const std::string& chunk, const std::function<void()>& interrupt,
                       const std::function<void(const std::string&)>& notice) {
        if (!picker) return false;
        size_t used = bytes();
        for (size_t i = 0; i < chunk.size(); ++i) {
            unsigned char c = (unsigned char)chunk[i];
            if ((c == 3 || c == 4) && !pickerPaste) {
                if (!pickerEscape.empty()) picker->pickerKeys.resize(picker->pickerKeys.size() - pickerEscape.size());
                if (c == 4) quit = true;
                releasePicker();
                interrupt();
                g_stdinPend = chunk.substr(i + 1) + g_stdinPend;
                return true;
            }
            // Reserve the closing delimiter so a bounded saved paste never
            // leaves its future picker in paste mode after an overflow.
            if (used >= 262138 || picker->pickerKeys.size() >= 65530) {
                std::string pending = pickerEscape;
                if (!pending.empty()) picker->pickerKeys.resize(picker->pickerKeys.size() - pending.size());
                bool paste = pickerPaste;
                if (paste) picker->pickerKeys += "\033[201~";
                releasePicker();
                interrupt();
                // Preserve the remainder as a paste too: embedded Enter or
                // Ctrl-C must not become live submissions or cancellation.
                g_stdinPend = (paste ? "\033[200~" : "") + pending + chunk.substr(i) + g_stdinPend;
                notice("queued picker input full; saved keys are held and remaining input returns to the composer\n");
                return true;
            }
            picker->pickerKeys.push_back((char)c);
            ++used;
            if (!pickerEscape.empty()) {
                pickerEscape.push_back((char)c);
                if (pickerPaste) {
                    if (pickerEscape == "\033[201~") {
                        pickerPaste = false;
                        pickerEscape.clear();
                    } else if (std::string("\033[201~").compare(0, pickerEscape.size(), pickerEscape) != 0) {
                        pickerEscape.clear();
                    }
                    continue;
                }
                if (pickerEscape == "\033[200~") pickerPaste = true;
                bool single = pickerEscape.size() == 2 && c != '[' && c != 'O';
                bool final = pickerEscape.size() > 2 && c >= 0x40 && c <= 0x7e;
                if (single || final || pickerEscape.size() >= 32) pickerEscape.clear();
            } else if (c == 0x1b) {
                pickerEscape = "\033";
                pickerEscAt = nowMs();
            }
        }
        return true;
    }
};

bool enqueue(QueuedInput& queue, const std::string& text, bool picker = false) {
    size_t bytes = text.size() + queue.bytes();
    if (queue.messages.size() >= 64 || bytes > 262144) return false;
    queue.messages.push_back({text, ""});
    if (picker) {
        queue.releasePicker();
        queue.picker = &queue.messages.back();
    }
    return true;
}

std::pair<std::string, std::string> slashParts(const std::string& text) {
    std::string value = trim(text);
    if (value.empty() || value[0] != '/') return {};
    size_t end = value.find_first_of(" \t\r\n");
    return {toLower(value.substr(0, end)), end == std::string::npos ? "" : trim(value.substr(end))};
}

bool opensPicker(const Config& cfg, const std::string& text) {
    auto [command, args] = slashParts(text);
    if (command == "/thinking") return args.empty();
    if (command == "/model") return args.empty() || !resolveModel(cfg, args).ok;
    if (command != "/models") return false;
    size_t split = args.find_first_of(" \t\r\n");
    std::string role = toLower(args.substr(0, split));
    bool known = role == "main" || role == "fast" || role == "fallback" || role == "review" || role == "subagent";
    return !known || split == std::string::npos || trim(args.substr(split)).empty();
}

std::string queueDescription(const QueuedInput& queue) {
    std::string out = "queue: " + std::to_string(queue.messages.size()) + (queue.held ? " held" : " ready") + "\n";
    size_t i = 0;
    for (const auto& message : queue.messages)
        out += "  " + std::to_string(++i) + ". " + cutBytes(sanitizeTerminal(message.text), 180) + "\n";
    return out;
}

std::string goalDescription(const Agent& agent) {
    std::string status = agent.goalStatus() == GoalStatus::Active ? "active" :
                         agent.goalStatus() == GoalStatus::Paused ? "paused" :
                         agent.goalStatus() == GoalStatus::Completed ? "completed" : "none";
    return "goal: " + status + (agent.goal().empty() ? "" : " · " + sanitizeTerminal(agent.goal())) + "\n";
}

// Agent work runs on one worker; only this main thread touches the terminal,
// editor, or message queue. Immutable events are the only output crossing.
int runTurnInteractive(TuiOpts& opts, Agent& agent, const std::string& input, SharedInput&,
                       std::atomic<bool>& cancel, BottomBar* bar, QueuedInput& queue,
                       bool goal = false, bool resume = false) {
    enum class Kind { Token, Reasoning, Notice, Tool, Status, Approval, Done };
    struct Event { Kind kind; std::string text; std::string extra; };
    struct Events {
        std::mutex mu;
        std::condition_variable cv;
        std::deque<Event> items;
        size_t bytes = 0;
        int approval = 0;  // 0 waiting, 1 allow, -1 reject
    } events;
    auto post = [&](Kind kind, const std::string& text, const std::string& extra = "") {
        bool stream = kind == Kind::Token || kind == Kind::Reasoning;
        std::string bounded = !stream && text.size() > 65536 ? cutBytes(text, 65536) + "\n[display truncated]\n" : text;
        size_t offset = 0;
        do {
            size_t count = stream ? std::min((size_t)16384, bounded.size() - offset) : bounded.size();
            std::unique_lock<std::mutex> lock(events.mu);
            events.cv.wait(lock, [&] { return events.bytes < 262144 && events.items.size() < 256; });
            if (cancel.load() && (kind == Kind::Token || kind == Kind::Reasoning)) return;
            if ((kind == Kind::Token || kind == Kind::Reasoning) && !events.items.empty() && events.items.back().kind == kind)
                events.items.back().text.append(bounded, offset, count);
            else events.items.push_back({kind, bounded.substr(offset, count), extra});
            events.bytes += count + extra.size();
            events.cv.notify_all();
            offset += count;
        } while (offset < bounded.size());
    };
    Editor& ed = *bar->ed;
    StreamRenderer rend;
    // A held flag with nothing held is stale (Esc on an empty queue): drop
    // it so this turn's footer reports "working", not "paused". A held
    // non-empty queue is real and stays held.
    if (queue.messages.empty()) queue.held = false;
    bool finished = false, interrupted = false, approvalPending = false;
    bool clearRequested = false, pauseRequested = false;
    std::atomic<bool> queuedReady{!queue.held && !queue.messages.empty()};
    std::string result;
    std::string status = kpiText(opts, agent, termWidth());
    std::string goalStatus = goalDescription(agent);
    if (goal || resume) goalStatus = "goal: active · " + (resume ? agent.goal() : input) + "\n";
    auto originalKpi = bar->kpi;
    int64_t turnStart = nowMs();
    bar->kpi = [&](int) {
        std::string hint = approvalPending ? "approval: y/N" : queue.picker ? "picker waiting · Esc pause" : "Enter queue · Esc pause";
        return status + "\n" + busyLine(nowMs(), turnStart, queue.messages.size(), queue.held, hint);
    };
    bar->fixedInputH = 3;
    bar->fixedFooterH = (int)bar->statusRows().size();
    bar->draw();
    bar->toTranscript();
    writeAll(STDOUT_FILENO, col("\033[38;5;37m") + std::string(goal || resume ? "◎ goal" : agent.doubleEnabled() ? "◆ pocket 2×" : "◆ pocket") + col(C_RESET) + "\n");
    if (bar->active) writeAll(STDOUT_FILENO, "\0337");
    bar->draw();
    auto draw = [&] {
        if (bar->active && (g_tstp || termRows() != bar->rows || termWidth() != bar->cols)) {
            writeAll(STDOUT_FILENO, "\0338");
            rend.endLine();
            bar->draw();
            bar->toTranscript();
            writeAll(STDOUT_FILENO, "\0337");
        }
        bar->draw();
    };
    auto output = [&](const std::function<void()>& emit) {
        if (bar->active && (g_tstp || termRows() != bar->rows || termWidth() != bar->cols)) draw();
        rend.viewportRows = bar->active ? std::max(1, bar->region) : 0;
        if (bar->active) {
            writeAll(STDOUT_FILENO, "\0338");
            emit();
            writeAll(STDOUT_FILENO, "\0337");
        } else {
            writeAll(STDOUT_FILENO, "\r\033[K");
            emit();
            rend.endLine();
            ed.lastRows = 1;
            ed.lastCursorRow = 0;
        }
    };
    auto interrupt = [&] {
        cancel.store(true);
        interrupted = true;
        queue.held = true;
        queue.releasePicker();
        queue.continueGoal = false;
        queuedReady.store(false);
        events.cv.notify_all();
    };
    cancel.store(false);
    agent.setGoalYield([&] { return queuedReady.load(); });
    int64_t lastStatus = 0;
    auto updateStatus = [&](bool force = false) {
        int64_t now = nowMs();
        if (force || now - lastStatus >= 250) {
            lastStatus = now;
            post(Kind::Status, kpiText(opts, agent, termWidth()), goalDescription(agent));
        }
    };
    // These callbacks execute only on the agent worker. Agent stats and goal
    // strings are therefore never read concurrently by the editor thread.
    agent.setCallbacks(
        [&](std::string_view text) { if (!cancel.load()) post(Kind::Token, std::string(text)); updateStatus(); },
        // Notices may announce a lifecycle transition. Publish its snapshot
        // first so an immediate /goal status cannot read the turn-start state.
        [&](const std::string& text) { updateStatus(true); post(Kind::Notice, text); },
        [&](std::string_view text) { if (!cancel.load()) post(Kind::Reasoning, std::string(text)); updateStatus(); });
    opts.tools->onEvent = [&](const std::string& text) { post(Kind::Tool, text); updateStatus(); };
    opts.tools->onToolDone = [&](const std::string& name, bool ok, const std::string& summary) {
        std::string text = (ok ? "✓ " : "✗ ") + name;
        size_t count = 0;
        for (const auto& line : splitLines(summary)) {
            if (++count > 3) { text += "\n    [...]"; break; }
            text += "\n    " + cutBytes(sanitizeTerminal(line), 120);
        }
        post(Kind::Tool, text);
        updateStatus();
    };
    auto originalApproval = opts.tools->askApproval;
    opts.tools->askApproval = [&](const std::string& command, const std::string& reason) {
        if (command.size() + reason.size() > 65536) {
            post(Kind::Notice, "command rejected: approval text exceeds the 64 KiB display limit");
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(events.mu);
            events.approval = 0;
        }
        post(Kind::Approval, command, reason);
        std::unique_lock<std::mutex> lock(events.mu);
        while (!events.approval && !cancel.load()) events.cv.wait_for(lock, std::chrono::milliseconds(50));
        return events.approval > 0 && !cancel.load();
    };
    std::thread worker([&] {
        std::string error;
        try { error = resume ? agent.resumeGoal(input) : goal ? agent.runGoal(input) : agent.runTurn(input); }
        catch (const std::exception& e) { error = e.what(); }
        catch (...) { error = "unexpected agent failure"; }
        updateStatus(true);
        post(Kind::Done, error);
    });
    bool thinkingHead = false;
    std::optional<Event> waitingApproval;
    int64_t lastSpin = 0;
    auto pump = [&] {
        if (!approvalPending && queue.expirePickerEscape(interrupt)) draw();
        std::deque<Event> batch;
        {
            std::lock_guard<std::mutex> lock(events.mu);
            batch.swap(events.items);
            events.bytes = 0;
            events.cv.notify_all();
        }
        for (const auto& event : batch) {
            if (event.kind == Kind::Status) { status = event.text; goalStatus = event.extra; }
            else if (event.kind == Kind::Done) { finished = true; result = event.text; }
            else if (event.kind == Kind::Approval) {
                waitingApproval = event;
            } else output([&] {
                if (event.kind == Kind::Token) rend.feed(event.text);
                else if (event.kind == Kind::Reasoning) {
                    if (!thinkingHead) { thinkingHead = true; rend.write("∴ thinking\n"); }
                    rend.feed(event.text, true);
                } else rend.write(col(C_DIM) + std::string(event.kind == Kind::Tool ? "  › " : "  ◇ ") +
                                  sanitizeTerminal(event.text) + col(C_RESET) + "\n");
            });
        }
        // Consume composer typeahead before establishing approval ownership.
        // A buffered "yes do X" must remain a follow-up, never permission.
        if (finished || cancel.load()) {
            waitingApproval.reset();
            approvalPending = false;
        }
        if (waitingApproval && !finished && !cancel.load() && g_stdinPend.empty() && !stdinReady() && !ed.inEscape && !ed.pasting &&
            queue.pickerEscape.empty() && !queue.pickerPaste) {
            const Event& event = *waitingApproval;
            approvalPending = true;
            output([&] { rend.write("\nPocketHarness blocked a potentially destructive command.\n" +
                sanitizeTerminal(event.extra) + "\n\n  " + sanitizeTerminal(event.text) + "\n\nAllow once? [y/N] "); });
            waitingApproval.reset();
            draw();
        }
        // Redraw periodically even with no events so the footer spinner and
        // elapsed time advance while the model or a tool is silently busy.
        // Unpinned terminals have no footer: spare them the extra redraws.
        int64_t now = nowMs();
        if (!batch.empty() || (bar->active && now - lastSpin >= 120)) {
            lastSpin = now;
            draw();
        }
        return !finished;
    };
    EditControl control;
    control.pump = pump;
    control.interrupt = interrupt;
    control.consume = [&](const std::string& chunk) {
        if (!approvalPending) {
            bool consumed = queue.capturePicker(chunk, interrupt, [&](const std::string& text) {
                output([&] { rend.write(text); });
            });
            if (consumed) draw();
            return consumed;
        }
        for (char c : chunk) {
            int answer = c == 'y' || c == 'Y' ? 1 : c == 'n' || c == 'N' || c == '\r' || c == '\n' || c == 0x1b || c == 3 || c == 4 ? -1 : 0;
            if (!answer) continue;
            if (c == 0x1b || c == 3 || c == 4) interrupt();
            {
                std::lock_guard<std::mutex> lock(events.mu);
                events.approval = answer;
            }
            events.cv.notify_all();
            approvalPending = false;
            output([&] { rend.write(answer > 0 ? "allowed once\n" : "rejected\n"); });
            draw();
            break;
        }
        return true;
    };
    bool exitFlag = false;
    while (!finished) {
        auto submitted = editLine(ed, draw, exitFlag, &control);
        if (exitFlag) {
            queue.quit = true;
            interrupt();
            while (!finished) {
                pump();
                std::unique_lock<std::mutex> lock(events.mu);
                if (!finished) events.cv.wait_for(lock, std::chrono::milliseconds(30));
            }
            break;
        }
        if (!submitted) continue;
        std::string text = *submitted;
        ed.reset();
        auto [command, args] = slashParts(text);
        if (command == "/goal" && (args == "pause" || args == "clear")) {
            clearRequested |= args == "clear";
            pauseRequested |= args == "pause";
            interrupt();
            output([&] { rend.write("goal " + args + " requested; waiting for the active operation to stop\n"); });
        } else if (command == "/goal" && (args.empty() || args == "status")) {
            output([&] { rend.write(goalStatus); });
        } else if (command == "/queue") {
            if (args == "clear") queue.clear();
            else if (args == "resume") queue.held = false;
            else if (!args.empty() && args != "status") {
                output([&] { rend.write("usage: /queue [status|clear|resume]\n"); });
            }
            output([&] { rend.write(queueDescription(queue)); });
        } else if (command == "/quit" || command == "/exit" || command == "/q") {
            queue.quit = true;
            interrupt();
        } else if (command == "/resume" || command == "/sessions") {
            output([&] { rend.write("session unchanged: pause active work with Esc, then use /resume; clear held follow-ups with /queue clear first\n"); });
        } else {
            if (!enqueue(queue, text, opensPicker(*opts.cfg, text))) {
                ed.setLines(text);
                output([&] { rend.write("queue full (64 messages / 256 KiB); your draft is retained\n"); });
            } else {
                // A new explicit submission releases a queue held by Esc/Ctrl-C.
                if (command.empty() || (command == "/goal" && args == "resume")) queue.held = false;
            }
        }
        queuedReady.store(!queue.held && !queue.messages.empty());
        draw();
    }
    worker.join();
    agent.setGoalYield({});
    agent.setCallbacks({}, {});
    opts.tools->onEvent = {};
    opts.tools->onToolDone = {};
    opts.tools->askApproval = std::move(originalApproval);
    if (clearRequested) {
        std::string error = agent.clearGoal();
        if (!error.empty()) result = error;
    } else if (pauseRequested || agent.goalStatus() == GoalStatus::Active) {
        std::string error = agent.pauseGoal();
        if (!error.empty()) result = error;
    }
    if ((goal || resume) && !interrupted && result.empty())
        queue.continueGoal = agent.goalPaused() && !queue.held && !queue.messages.empty();
    if (interrupted && !queue.held && !queue.messages.empty() && agent.goalPaused()) queue.continueGoal = true;
    if (result == "cancelled" && !interrupted) queue.held = true;
    if (!result.empty() && result != "cancelled") { queue.held = true; queue.continueGoal = false; }
    output([&] {
        rend.flush();
        if (result == "cancelled") writeAll(STDOUT_FILENO, queue.held ?
            "\n(paused; queued messages held until a new message or /queue resume)\n" :
            "\n(stopped; continuing with explicitly queued follow-up)\n");
        else if (!result.empty()) writeAll(STDOUT_FILENO, "\nerror: " + sanitizeTerminal(result) + "\n");
        else writeAll(STDOUT_FILENO, "\n");
    });
    bar->fixedInputH = bar->fixedFooterH = 0;
    bar->kpi = std::move(originalKpi);
    bar->draw();
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
    const int maxRows = std::max(1, std::min(10, termRows() - 8));
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
        if (inEsc) {
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
                } else if (esc == "[A" || esc == "OA") {
                    sync();
                    if (sel > 0) --sel;
                    endEsc();
                } else if (esc == "[B" || esc == "OB") {
                    sync();
                    ++sel;
                    endEsc();
                } else if (esc.size() >= 32 || (esc.size() == 1 && esc[0] != '[' && esc[0] != 'O')) {
                    endEsc();
                } else if (esc.size() >= 2 && (esc[0] == '[' || esc[0] == 'O') &&
                           esc.back() >= 0x40 && esc.back() <= 0x7e) {
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
                    sync();
                    if (sel > 0) --sel;
                    break;
                case 0x0e:
                    sync();
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

// Return true only after main has reserved the target. The current Agent and
// terminal stay intact on picker cancellation or any validation failure.
bool prepareSessionResume(TuiOpts& opts, Agent& agent, const std::string& args) {
    auto say = [&](const std::string& text) { writeAll(STDOUT_FILENO, sanitizeTerminal(text) + "\n"); };
    if (!opts.prepareResume) {
        say("session switching is unavailable here; use pocket --resume <id>");
        return false;
    }
    if (agent.pendingImages()) {
        say("session unchanged: send attached images before switching sessions");
        return false;
    }
    std::string id = args;
    if (id == "last") {
        auto latest = sessionResolve("last", opts.workspace);
        if (!latest.ok) { say("cannot resume: " + latest.error); return false; }
        id = latest.value;
    }
    if (id.empty()) {
        std::vector<SessionInfo> sessions;
        std::vector<std::string> labels;
        for (const auto& session : sessionList(50, opts.workspace)) {
            if (session.id == opts.sessionId) continue;
            sessions.push_back(session);
            std::string state = session.active ? "active (unavailable)" : "saved";
            if (!session.goalStatus.empty()) state += " · goal " + session.goalStatus;
            if (!session.lastStopReason.empty()) state += " · " + session.lastStopReason;
            labels.push_back(session.id + " · " + state +
                             (session.firstLine.empty() ? "" : " · " + session.firstLine));
        }
        if (sessions.empty()) {
            say("no other saved sessions in this workspace");
            return false;
        }
        PickResult picked = pickOne("Resume a session · " + opts.workspace + " · Esc cancel", labels, "");
        if (!picked.submitted) { say("session unchanged"); return false; }
        if (picked.index < 0) {
            say("no matching session; use /resume <exact id>");
            return false;
        }
        const auto& selected = sessions[(size_t)picked.index];
        if (selected.active) {
            say("session unchanged: that session is active in another process");
            return false;
        }
        id = selected.id;
    }
    if (id == opts.sessionId) {
        say("already in session " + id);
        return false;
    }
    // Picker selection keys are consumed above; anything still buffered is a
    // separate draft/command and must not silently move to another session.
    if (g_term && g_term->active && (!g_stdinPend.empty() || stdinReady())) {
        say("session unchanged: finish or clear buffered input before /resume");
        return false;
    }
    std::string error = opts.prepareResume(id);
    if (!error.empty()) {
        say("cannot resume: " + error);
        return false;
    }
    opts.resumeId = id;
    say("continuing session: " + id);
    return true;
}

// ---------------------------------------------------------------------------
// Slash commands. Returns false when the session should end.
// ---------------------------------------------------------------------------
bool runCommand(TuiOpts& opts, Agent& agent, const std::string& input) {
    std::string rest = trim(input.substr(1));
    size_t sp = rest.find_first_of(" \t\r\n");
    std::string cmd = sp == std::string::npos ? rest : rest.substr(0, sp);
    std::string args = sp == std::string::npos ? "" : trim(rest.substr(sp + 1));
    cmd = toLower(cmd);

    auto say = [&](const std::string& s) { writeAll(STDOUT_FILENO, sanitizeTerminal(s)); };

    if (cmd == "quit" || cmd == "exit" || cmd == "q") return false;
    if (cmd == "resume" || cmd == "sessions") return !prepareSessionResume(opts, agent, args);
    if (cmd == "goal" && (args.empty() || args == "status" || args == "pause" || args == "clear")) {
        std::string error = args == "clear" ? agent.clearGoal() : args == "pause" ? agent.pauseGoal() : "";
        if (!error.empty()) say("error: " + error + "\n");
        say(goalDescription(agent));
        return true;
    }
    if (cmd == "help") {
        say("commands:\n"
            "  /models           assign models to roles: main · fast · fallback · review · subagent (fuzzy search)\n"
            "  /model [spec]     switch the main model directly\n"
            "  /goal [text]      start a goal; /goal status|pause|resume|clear controls it\n"
            "  /queue [action]   status, clear, or resume held follow-ups\n"
            "  /sessions         choose a saved session in this workspace\n"
            "  /resume [id|last]  continue a saved session (no argument opens the picker)\n"
            "  /undo             revert the newest file change made by the agent\n"
            "  /brain            learned quirks, provider health, judges, overseer stats\n"
            "  /catalog          refresh the live model catalog now\n"
            "  /thinking [level] auto/off/none/minimal/low/medium/high/xhigh/max\n"
            "  /double [on|off]  twin first-pass analyses (2× mode), reconciled into one plan\n"
            "  /compact          summarize older context now\n"
            "  /skills [query]   list or search Markdown skills\n"
            "  /session          show session info and token usage\n"
            "  /security         show sandbox + authority state\n"
            "  /help             this text\n"
            "  /quit             exit\n"
            "keys: Enter send/queue · Ctrl-J/Alt-Enter newline · Up/Down history · Esc/Ctrl-C pause active work\n"
            "queued follow-ups wait after a pause; a new message or /queue resume releases them\n"
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
            InputGate gate(g_approval ? g_approval->in : nullptr);
            long live = fetchModelContext(rm.value.provider, rm.value.model);
            if (live > 0) rm.value.context = live;
        }
        if (!opts.sessionId.empty()) {
            auto m = sessionLoadMeta(opts.sessionId);
            if (!m.ok) { say("model not changed: " + m.error + "\n"); return; }
            m.value.modelSpec = rm.value.spec;
            auto saved = sessionSaveMeta(opts.sessionId, m.value);
            if (!saved.ok) { say("model not changed: " + saved.error + "\n"); return; }
        }
        opts.model = rm.value;
        opts.cfg->defaultModel = rm.value.spec;
        agent.setModel(rm.value, opts.thinking);
        say("model: " + rm.value.spec + "\n");
    };
    auto setLevel = [&](const std::string& t) {
        if (!opts.sessionId.empty()) {
            auto m = sessionLoadMeta(opts.sessionId);
            if (!m.ok) { say("thinking not changed: " + m.error + "\n"); return; }
            m.value.thinking = t;
            auto saved = sessionSaveMeta(opts.sessionId, m.value);
            if (!saved.ok) { say("thinking not changed: " + saved.error + "\n"); return; }
        }
        opts.thinking = t;
        opts.cfg->thinking = t;
        agent.setModel(opts.model, t);
        say("thinking: " + t + "\n");
    };
    auto pickModel = [&](const std::string& title, const std::string& initial, bool allowNone, bool allowMany = false) -> std::string {
        auto all = catalogLoad(*opts.cfg);
        std::vector<std::string> labels, specs;
        for (const auto& m : opts.cfg->models) {
            labels.push_back(m.alias + " = " + m.provider + ":" + m.model + (m.routing.empty() ? "" : "@" + m.routing));
            specs.push_back(m.alias);
        }
        for (const auto& m : all) {
            labels.push_back(catalogLabel(m));
            specs.push_back(m.provider + ":" + m.id);
        }
        // Last, never the default row: Enter must not silently clear a role.
        if (allowNone) labels.push_back("(none) — clear this role"), specs.push_back("-");
        PickResult pr = pickOne(title, labels, initial);
        if (!pr.submitted) return "";
        if (allowMany && pr.filter.find(',') != std::string::npos) return pr.filter;
        if (allowNone && trim(pr.filter) == "-") return "-";
        if (!pr.filter.empty() && resolveModel(*opts.cfg, pr.filter).ok) return pr.filter;
        if (pr.index >= 0) return specs[(size_t)pr.index];
        return pr.filter;  // invalid text: the caller reports the precise error
    };
    if (cmd == "model") {
        if (!args.empty() && resolveModel(*opts.cfg, args).ok) {
            useModel(args);  // exact spec: switch directly, no picker
            return true;
        }
        std::string spec = pickModel("main model — now " + opts.model.spec + " · type to search", args, false);
        if (spec.empty()) say("(cancelled)\n");
        else useModel(spec);
        return true;
    }
    if (cmd == "models") {
        static const char* kRoles[] = {"main", "fast", "fallback", "review", "subagent"};
        static const char* kWhat[] = {"does the work", "briefs, summaries, audits", "takes over when main fails",
                                      "the review council (comma-separate several)", "default model for delegated tasks"};
        std::vector<std::string> labels;
        for (size_t i = 0; i < std::size(kRoles); ++i) {
            std::string now = i == 0 ? opts.model.spec : opts.cfg->roles.count(kRoles[i]) ? opts.cfg->roles[kRoles[i]] : "—";
            char b[256];
            snprintf(b, sizeof b, "%-9s %-44s %s", kRoles[i], now.c_str(), kWhat[i]);
            labels.push_back(b);
        }
        size_t sep = args.find_first_of(" \t\n");
        std::string roleArg = toLower(args.substr(0, sep));
        std::string specArg = sep == std::string::npos ? "" : trim(args.substr(sep));
        PickResult r;
        for (size_t i = 0; i < std::size(kRoles); ++i)
            if (roleArg == kRoles[i]) { r.submitted = true; r.index = (int)i; }
        if (!r.submitted) r = pickOne("models — pick a role to assign", labels, args);
        if (!r.submitted || r.index < 0) {
            say("(cancelled)\n");
            return true;
        }
        std::string roleName = kRoles[r.index];
        std::string spec = specArg.empty() ? pickModel(roleName + " model · type to search " + std::to_string(catalogLoad(*opts.cfg).size()) +
                                         " catalog models (provider:model works too)",
                                     "", roleName != "main", roleName == "review") : specArg;
        if (spec.empty()) { say("(cancelled)\n"); return true; }
        if (spec == "-" && roleName == "main") { say("main role cannot be cleared\n"); return true; }
        if (roleName != "review" && spec.find(',') != std::string::npos) {
            say("only the review council accepts multiple comma-separated models\n");
            return true;
        }
        std::vector<ResolvedModel> resolved;
        if (spec != "-") {
            std::string selections = spec;
            std::replace(selections.begin(), selections.end(), ',', '\n');
            for (const std::string& one : splitLines(selections)) {
                if (trim(one).empty()) { say("empty model in selection\n"); return true; }
                auto rm = resolveModel(*opts.cfg, trim(one));
                if (!rm.ok) { say("error: " + rm.error + "\n"); return true; }
                catalogApply(*opts.cfg, rm.value);
                resolved.push_back(rm.value);
            }
        }
        auto nextRoles = opts.cfg->roles;
        if (spec == "-") nextRoles.erase(roleName);
        else {
            std::vector<std::string> specs;
            for (const auto& model : resolved) specs.push_back(model.spec);
            nextRoles[roleName] = join(specs, ",");
        }
        auto saved = saveRole(roleName, spec == "-" ? "" : spec);
        if (!saved.ok) { say("role not changed: " + saved.error + "\n"); return true; }
        if (!opts.sessionId.empty()) {
            auto meta = sessionLoadMeta(opts.sessionId);
            if (!meta.ok) { say("default saved, but session role not changed: " + meta.error + "\n"); return true; }
            meta.value.roles = nextRoles;
            meta.value.rolesSet = true;
            if (roleName == "main") meta.value.modelSpec = resolved[0].spec;
            auto persisted = sessionSaveMeta(opts.sessionId, meta.value);
            if (!persisted.ok) { say("default saved, but session role not changed: " + persisted.error + "\n"); return true; }
        }
        opts.cfg->roles = std::move(nextRoles);
        if (roleName == "main") {
            opts.model = resolved[0];
            opts.cfg->defaultModel = opts.model.spec;
            agent.setModel(opts.model, opts.thinking);
        } else agent.setRole(roleName, resolved);
        say(roleName + ": " + (spec == "-" ? "cleared" : spec) + " (saved)\n");
        return true;
    }
    if (cmd == "undo") {
        say(undoLast(*opts.tools) + "\n");
        return true;
    }
    if (cmd == "brain") {
        const AgentStats& st = agent.stats();
        say(judgeStatus(*opts.cfg) + "\n");
        char b[256];
        snprintf(b, sizeof b, "overseer: %d nudges · %d reviews · %d fallbacks · %d distilled/deduped · side cost $%.5f\n",
                 st.nudges, st.reviews, st.fallbacks, st.deduped, st.sideCost);
        say(b);
        say("learned (" + stateDir() + "/brain.json):\n" + brainStatus());
        return true;
    }
    if (cmd == "catalog") {
        InputGate gate(g_approval ? g_approval->in : nullptr);
        say("refreshing catalog from every keyed provider...\n");
        say(catalogRefresh(*opts.cfg) + "\n");
        return true;
    }
    if (cmd == "thinking") {
        std::string t = toLower(args);
        if (!args.empty() && validThinking(t)) {
            setLevel(t);
            return true;
        }
        if (!args.empty()) {
            say("usage: /thinking auto|off|none|minimal|low|medium|high|xhigh|max\n");
            return true;
        }
        static const std::vector<std::string> kLevels = {"auto", "off", "none", "minimal", "low", "medium", "high", "xhigh", "max"};
        PickResult pr = pickOne("thinking — now: " + opts.thinking, kLevels, "");
        if (!pr.submitted) {
            say("(cancelled)\n");
            return true;
        }
        std::string f = toLower(pr.filter);
        if (!f.empty() && validThinking(f)) setLevel(f);
        else if (pr.index >= 0) setLevel(kLevels[(size_t)pr.index]);
        else say("usage: /thinking auto|off|none|minimal|low|medium|high|xhigh|max\n");
        return true;
    }
    if (cmd == "double") {
        std::string a = toLower(args);
        bool on;
        if (a.empty()) on = !agent.doubleEnabled();
        else if (a == "on") on = true;
        else if (a == "off") on = false;
        else if (a == "status") on = agent.doubleEnabled();
        else { say("usage: /double [on|off|status]\n"); return true; }
        std::string err = agent.setDouble(on);
        if (!err.empty()) { say("error: " + err + "\n"); return true; }
        say(std::string("Double mode: ") +
            (on ? "ON\n2× " + opts.model.spec + " (twin first-pass mode; total cost varies)\n" : "OFF\n"));
        return true;
    }
    if (cmd == "compact") {
        InputGate gate(g_approval ? g_approval->in : nullptr);
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
            " · compactions: " + std::to_string(st.compactions) +
            " · double passes: " + std::to_string(st.doubles) + "\n" + "context: ~" +
            std::to_string(agent.contextUsed()) + " / " + std::to_string(agent.contextMax()) +
            " tokens" + " · usage in/out: " + std::to_string(st.inTokens) + "/" +
            std::to_string(st.outTokens) + "\n");
        auto meta = sessionLoadMeta(opts.sessionId);
        if (meta.ok && !meta.value.lastStopReason.empty())
            say("last stop: " + meta.value.lastStopReason +
                (meta.value.lastStopDetail.empty() ? "" : " · " + meta.value.lastStopDetail) + "\n");
        say("provider: " + opts.model.provider.name + " · model: " + opts.model.model +
            " · thinking: " + opts.thinking + (agent.doubleEnabled() ? " · double: ON (2×)" : " · double: off") + "\n");
        say("gen: " + tpsText(st) + " avg · child sessions: " + std::to_string(st.childSessions) + "\n");
        for (const auto& peer : sessionList(30, opts.workspace))
            if (peer.active && peer.id != opts.sessionId)
                say("active workspace peer: " + peer.id + "\n");
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
            char buf[256];
            snprintf(buf, sizeof(buf), "cost: %s$%.4f (%s) · overseer/judges $%.5f\n", st.costEstimated ? "~" : "",
                     st.cost, st.costEstimated ? "partly estimated from catalog prices" : "reported", st.sideCost);
            say(buf);
            if (st.costIncomplete) say("additional model usage has unreported cost\n");
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
    opts.resumeId.clear();
    TermGuard term;
    SignalGuard signals;
    g_term = &term;
    g_stdinPend.clear();
    g_winch = 0;
    g_tstp = 0;
    term.enter();
    if (!term.active) {
        g_term = nullptr;
        return lineRun(opts);  // termios failed: fall back
    }
    writeAll(STDOUT_FILENO, "\033[?2004h");  // bracketed paste; ignored if unsupported

    printBanner(opts);
    Agent& agent = *opts.agent;
    Editor ed;
    QueuedInput queue;
    BottomBar bar;
    bar.ed = &ed;
    bar.kpi = [&](int cols) {
        return kpiText(opts, agent, cols) + (queue.messages.empty() ? "" :
            "\nqueued " + std::to_string(queue.messages.size()) + (queue.held ? " · held · /queue resume" : " · ready"));
    };
    bar.setup();
    auto draw = [&] { bar.draw(); };
    std::atomic<bool> cancel{false};
    opts.tools->cancel = &cancel;
    agent.setCancel(&cancel);
    struct ResetCallbacks {
        Agent& agent;
        ToolEnv& tools;
        ~ResetCallbacks() {
            agent.setCallbacks({}, {});
            agent.setCancel(nullptr);
            tools.cancel = nullptr;
            tools.askApproval = {};
            tools.onEvent = {};
            tools.onToolDone = {};
            g_endPartial = nullptr;
        }
    } resetCallbacks{agent, *opts.tools};
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
    while (!exitFlag && !queue.quit) {
        ed.prompt = bar.active ? shortPrompt() : promptFor(opts, agent);
        std::optional<std::string> input;
        std::string pickerKeys;
        bool fromQueue = false;
        if (!queue.held && queue.continueGoal && queue.messages.empty()) {
            input = "/goal resume";
            fromQueue = true;
            queue.continueGoal = false;
        } else if (!queue.held && !queue.messages.empty()) {
            if (!g_stdinPend.empty() || stdinReady() || ed.inEscape || ed.pasting || !queue.pickerEscape.empty()) {
                EditControl pending;
                pending.interrupt = [&] { queue.held = true; queue.releasePicker(); };
                pending.pump = [&] {
                    queue.expirePickerEscape(pending.interrupt);
                    return !queue.held && (!g_stdinPend.empty() || stdinReady() || ed.inEscape || ed.pasting || !queue.pickerEscape.empty());
                };
                pending.consume = [&](const std::string& chunk) {
                    return queue.capturePicker(chunk, pending.interrupt, [&](const std::string& text) {
                        bar.toTranscript(); writeAll(STDOUT_FILENO, text); draw();
                    });
                };
                input = editLine(ed, draw, exitFlag, &pending);
                if (!input) continue;
            } else {
                if (queue.picker == &queue.messages.front()) queue.releasePicker();
                input = std::move(queue.messages.front().text);
                pickerKeys = std::move(queue.messages.front().pickerKeys);
                queue.messages.pop_front();
                fromQueue = true;
            }
        } else input = editLine(ed, draw, exitFlag);
        if (exitFlag || !input) break;
        if (!fromQueue) ed.reset();
        std::string text = *input;
        size_t nonspace = text.find_first_not_of(" \t\r\n");
        if (nonspace != std::string::npos && text[nonspace] == '/') text.erase(0, nonspace);
        if (trim(text).empty()) continue;
        auto [command, args] = slashParts(text);
        bar.draw();
        bar.toTranscript();
        if (command == "/queue") {
            if (args == "clear") queue.clear();
            else if (args == "resume") {
                queue.held = false;
                queue.continueGoal = agent.goalPaused() && !queue.messages.empty();
            }
            else if (!args.empty() && args != "status") writeAll(STDOUT_FILENO, "usage: /queue [status|clear|resume]\n");
            writeAll(STDOUT_FILENO, queueDescription(queue));
            continue;
        }
        bool goalControl = command == "/goal" && (args.empty() || args == "status" || args == "pause" || args == "clear");
        if (goalControl) {
            if (args == "pause" || args == "clear") { queue.held = true; queue.continueGoal = false; }
            runCommand(opts, agent, text);
            continue;
        }
        if ((command == "/resume" || command == "/sessions") &&
            (!queue.messages.empty() || !ed.empty() || ed.pasting || ed.inEscape)) {
            writeAll(STDOUT_FILENO, "session unchanged: finish or clear your draft and held follow-ups (/queue clear) before /resume\n");
            continue;
        }
        bool wasHeld = queue.held, wasContinuation = queue.continueGoal;
        if (!fromQueue && queue.held && (command.empty() || (command == "/goal" && args == "resume"))) {
            queue.held = false;
            queue.continueGoal = agent.goalPaused();
            if (command == "/goal" && !queue.messages.empty()) continue;
        }
        if (!fromQueue && !queue.messages.empty() && command != "/quit" && command != "/exit" && command != "/q" &&
            (!queue.held || command.empty() || (command == "/goal" && args == "resume"))) {
            // Explicit input releases a held FIFO; older accepted messages keep
            // their order, while the newly submitted follow-up joins its tail.
            if (!enqueue(queue, text, opensPicker(*opts.cfg, text))) {
                ed.setLines(text);
                queue.held = wasHeld;
                queue.continueGoal = wasContinuation;
                writeAll(STDOUT_FILENO, "queue full (64 messages / 256 KiB); your draft is retained\n");
                continue;
            }
            queue.held = false;
            continue;
        }
        if (command.empty()) text = attachPastedImages(opts, agent, text);
        if (bar.active) writeAll(STDOUT_FILENO, col(C_BOLD) + "> " + col(C_RESET) + sanitizeTerminal(text) + "\n");
        if (command == "/goal") {
            bool resume = args == "resume";
            runTurnInteractive(opts, agent, resume ? "" : args, shared, cancel, &bar, queue, !resume, resume);
        } else if (!command.empty()) {
            // The saved bytes belong to this modal command, not to older FIFO
            // turns, the live composer, or a destructive-command approval.
            g_stdinPend = pickerKeys + g_stdinPend;
            if (!runCommand(opts, agent, text)) {
                if (!opts.resumeId.empty()) rc = kTuiResume;
                break;
            }
        } else {
            bool resume = agent.goalPaused();
            queue.continueGoal = false;
            runTurnInteractive(opts, agent, text, shared, cancel, &bar, queue, false, resume);
        }
    }

    shared.stop.store(true);
    shared.cv.notify_all();
    if (watcher.joinable()) watcher.join();
    g_approval = nullptr;
    bar.teardown();
    writeAll(STDOUT_FILENO, "\033[?2004l");
    term.leave();
    g_term = nullptr;
    writeAll(STDOUT_FILENO, "\n");
    return rc;
}

int lineRun(TuiOpts& opts) {
    opts.resumeId.clear();
    bool g_plain = !isatty(STDOUT_FILENO);
    bool ttyIn = isatty(STDIN_FILENO);
    Agent& agent = *opts.agent;
    std::atomic<bool> cancel{false};
    opts.tools->cancel = &cancel;
    agent.setCancel(&cancel);
    struct ResetCallbacks {
        Agent& agent;
        ToolEnv& tools;
        ~ResetCallbacks() {
            agent.setCallbacks({}, {});
            agent.setCancel(nullptr);
            tools.cancel = nullptr;
            tools.askApproval = {};
            tools.onEvent = {};
            tools.onToolDone = {};
            g_endPartial = nullptr;
        }
    } resetCallbacks{agent, *opts.tools};
    opts.tools->askApproval = [&](const std::string& cmd, const std::string& reason) {
        return askApprovalCli(cmd, reason);
    };
    if (ttyIn) {
        std::string b = "pocket " + std::string(kVersion) + " (" + opts.workspace + ") model " +
                        opts.model.spec + "\n" + (opts.sessionId.empty() ? "" : "session " + opts.sessionId + "\n");
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
        size_t nonspace = line.find_first_not_of(" \t\r\n");
        if (nonspace != std::string::npos && line[nonspace] == '/') line.erase(0, nonspace);
        size_t end = line.find_first_of(" \t\r\n");
        bool goal = toLower(line.substr(0, end)) == "/goal";
        std::string goalText = goal && end != std::string::npos ? trim(line.substr(end)) : "";
        if (line[0] == '/' && (!goal || goalText.empty() || goalText == "clear" || goalText == "pause" || goalText == "status")) {
            if (!runCommand(opts, agent, line)) break;
            continue;
        }
        if (!goal) line = attachPastedImages(opts, agent, line);
        rend = StreamRenderer{};
        g_endPartial = [&] { rend.endLine(); };
        if (!g_plain) writeAll(STDOUT_FILENO, "assistant:\n");
        std::string err = goal && goalText == "resume" ? agent.resumeGoal() : goal ? agent.runGoal(goalText) :
                          agent.goalPaused() ? agent.resumeGoal(line) : agent.runTurn(line);
        rend.flush();
        g_endPartial = nullptr;
        if (!err.empty() && err != "cancelled")
            writeAll(STDOUT_FILENO, "error: " + sanitizeTerminal(err) + "\n");
        if (g_plain) writeAll(STDOUT_FILENO, "\n");
    }
    return opts.resumeId.empty() ? 0 : kTuiResume;
}

}  // namespace pocket
