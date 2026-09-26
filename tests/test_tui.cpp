// PocketHarness tests - terminal line rendering + escape safety.
#include "mini.h"

#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <thread>

#include "../src/tui.h"

using namespace pocket;
using namespace pocket::test;

namespace {
// A real PTY exercises terminal ownership and raw-mode input without network
// requests or a terminal framework. A pipe holds the first mock response open.
struct TuiFixture {
    std::string home = makeTempDir("pocket-tui");
    int master = -1, release = -1;
    pid_t child = -1;
    std::string output;
    bool plain = false;
    explicit TuiFixture(int width = 80, int height = 24, bool lineMode = false, bool pausedGoal = false) : plain(lineMode) {
        master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
        if (master < 0 || grantpt(master) || unlockpt(master)) return;
        char* path = ptsname(master);
        if (!path) return;
        int slave = open(path, O_RDWR | O_NOCTTY | O_CLOEXEC);
        int gate[2];
        if (slave < 0 || pipe2(gate, O_CLOEXEC)) {
            if (slave >= 0) close(slave);
            return;
        }
        winsize size{};
        size.ws_col = (unsigned short)width;
        size.ws_row = (unsigned short)height;
        ioctl(slave, TIOCSWINSZ, &size);
        std::cout.flush();
        std::cerr.flush();
        child = fork();
        if (child == 0) {
            close(master);
            close(gate[1]);
            setsid();
            ioctl(slave, TIOCSCTTY, 0);
            dup2(slave, STDIN_FILENO);
            dup2(slave, STDOUT_FILENO);
            dup2(slave, STDERR_FILENO);
            close(slave);
            setenv("HOME", home.c_str(), 1);
            setenv("XDG_CONFIG_HOME", (home + "/config").c_str(), 1);
            setenv("XDG_STATE_HOME", (home + "/state").c_str(), 1);
            setenv("POCKET_NO_ANIM", "1", 1);
            setenv("NO_COLOR", "1", 1);
            setenv("TERM", "xterm", 1);
            Config cfg = defaultConfig();
            ToolEnv env;
            env.workspace = home;
            AgentOpts ao;
            ao.model = resolveModel(cfg, "glm").value;
            ao.tools = &env;
            bool holdGoalWork = false;
            ao.request = [&](const ChatRequest& req, const ChatCallbacks& cb) -> Result<ChatResponse> {
                std::string user = req.messages.back().content;
                if (req.system.find("You are the planning council") != std::string::npos)
                    holdGoalWork = user.find("REQUEST:\nhold") != std::string::npos;
                bool heldGoal = req.stream && holdGoalWork && user.find("[goal") != std::string::npos;
                if (heldGoal) holdGoalWork = false;
                if (user == "first" || user == "delayed approval" || heldGoal) {
                    if (env.onEvent) env.onEvent("judge: local LM ran successfully (fixture)");
                    if (cb.onToken) cb.onToken(heldGoal ? "GOAL_RUNNING\n" : "FIRST_RUNNING\n");
                    for (;;) {
                        if (cb.cancel && cb.cancel->load()) return Result<ChatResponse>::Err("cancelled");
                        pollfd ready{gate[0], POLLIN, 0};
                        if (poll(&ready, 1, 20) > 0) {
                            char go;
                            if (read(gate[0], &go, 1) == 1) break;
                        }
                    }
                }
                if (user == "approval" || user == "delayed approval") user = env.askApproval("fixture command", "fixture reason") ? "allowed" : "denied";
                if (user == "oversized approval")
                    user = env.askApproval(std::string(65537, 'x'), "fixture reason") ? "unexpected allowance" : "oversized rejected";
                if (user == "cancel" || user == "late approval") {
                    if (cb.onToken) cb.onToken("CANCEL_RUNNING\n");
                    for (int i = 0; i < 200 && cb.cancel && !cb.cancel->load(); ++i)
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (user == "late approval") env.askApproval("raced command", "after cancellation");
                    if (cb.cancel && cb.cancel->load()) return Result<ChatResponse>::Err("cancelled");
                }
                if (user == "longstream" && cb.onToken) {
                    for (int i = 0; i < 40; ++i) cb.onToken(std::string(80, 'z'));
                    cb.onToken("\nLONG_DONE\n");
                }
                ChatResponse reply;
                reply.text = req.system.find("You audit an autonomous agent") != std::string::npos ? "DONE" : "REPLY:" + json::stringify(user) + "\n";
                if (cb.onToken) cb.onToken(reply.text);
                return Result<ChatResponse>::Ok(reply);
            };
            Agent agent(ao);
            if (pausedGoal) {
                std::atomic<bool> stopped{true};
                agent.setCancel(&stopped);
                agent.runGoal("seeded goal");
                agent.setCancel(nullptr);
            }
            TuiOpts opts;
            opts.agent = &agent;
            opts.tools = &env;
            opts.cfg = &cfg;
            opts.model = ao.model;
            opts.workspace = home;
            int rc = plain ? lineRun(opts) : tuiRun(opts);
            if (env.cancel || env.onEvent || env.onToolDone || env.askApproval) rc = 91;
            _exit(rc);
        }
        close(slave);
        close(gate[0]);
        release = gate[1];
    }
    bool send(const std::string& bytes) {
        size_t off = 0;
        while (off < bytes.size()) {
            ssize_t n = write(master, bytes.data() + off, bytes.size() - off);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return false;
            off += (size_t)n;
        }
        return true;
    }
    bool sendDraining(const std::string& bytes) {
        int flags = fcntl(master, F_GETFL);
        if (flags < 0 || fcntl(master, F_SETFL, flags | O_NONBLOCK)) return false;
        size_t off = 0;
        int64_t until = nowMs() + 5000;
        while (off < bytes.size() && nowMs() < until) {
            pollfd p{master, POLLIN | POLLOUT, 0};
            if (poll(&p, 1, 30) <= 0) continue;
            if (p.revents & POLLIN) {
                char buffer[8192];
                ssize_t n = read(master, buffer, sizeof buffer);
                if (n > 0) output.append(buffer, (size_t)n);
            }
            if (p.revents & POLLOUT) {
                ssize_t n = write(master, bytes.data() + off, bytes.size() - off);
                if (n > 0) off += (size_t)n;
                else if (errno != EAGAIN && errno != EINTR) break;
            }
        }
        int restored = fcntl(master, F_SETFL, flags);
        return off == bytes.size() && restored == 0;
    }
    bool waitFor(const std::string& text, int timeout = 3000) {
        int64_t until = nowMs() + timeout;
        while (output.find(text) == std::string::npos && nowMs() < until) {
            pollfd p{master, POLLIN, 0};
            if (poll(&p, 1, 30) <= 0) continue;
            char b[8192];
            ssize_t n = read(master, b, sizeof b);
            if (n <= 0) return false;
            output.append(b, (size_t)n);
        }
        return output.find(text) != std::string::npos;
    }
    bool ready() { return child > 0 && waitFor(plain ? "pocket " : "cache "); }
    void finishFirst() { char c = 'x'; if (write(release, &c, 1) != 1) return; }
    bool quit() {
        if (!send("/quit\r")) return false;
        int status = 0;
        int64_t until = nowMs() + 3000;
        while (nowMs() < until) {
            pid_t r = waitpid(child, &status, WNOHANG);
            if (r == child) {
                child = -1;
                termios term{};
                return WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
                       tcgetattr(master, &term) == 0 && (term.c_lflag & ECHO) && (term.c_lflag & ICANON);
            }
            pollfd p{master, POLLIN, 0};
            if (poll(&p, 1, 20) > 0) {
                char b[8192];
                ssize_t n = read(master, b, sizeof b);
                if (n > 0) output.append(b, (size_t)n);
            }
        }
        return false;
    }
    ~TuiFixture() {
        if (child > 0) {
            kill(child, SIGKILL);
            while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {}
        }
        if (release >= 0) close(release);
        if (master >= 0) close(master);
        rmRf(home);
    }
};
}  // namespace

TEST(tui_Followup_Typed_During_Response) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.waitFor("provider orcarouter"));
    CHECK(t.waitFor("model z-ai/glm-5.3-flash"));
    CHECK(t.waitFor("thinking off"));
    CHECK(t.waitFor("total unreported"));
    CHECK(t.send("first\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    CHECK(t.waitFor("judge: local LM ran successfully"));
    CHECK(t.send("seconx\177d"));
    CHECK(t.waitFor("> second"));  // visible/editable before the provider finishes
    CHECK(t.send("\rthird\r"));
    CHECK(t.waitFor("queued 2"));
    t.finishFirst();
    CHECK(t.waitFor("REPLY:\"second\""));
    CHECK(t.waitFor("REPLY:\"third\""));
    CHECK(t.output.find("REPLY:\"second\"") < t.output.find("REPLY:\"third\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Paste_Multiline_And_Command_Boundary) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("\033[200~one\r\ntwo\033[201~\r"));
    CHECK(t.waitFor("REPLY:\"one\\ntwo\""));
    CHECK(t.send("/goalkeeper\r"));
    CHECK(t.waitFor("unknown command /goalkeeper"));
    CHECK(t.send("/thinking\thigh\r/session\r"));
    CHECK(t.waitFor("thinking: high"));
    CHECK(t.waitFor("messages:"));
    CHECK(t.send("\033[200~é\nx\033[201~\033[AQ\r"));
    CHECK(t.waitFor("REPLY:\"Qé\\nx\""));
    CHECK(t.send(std::string("\xc3", 1) + "\001\033[CX\r"));
    CHECK(t.waitFor("REPLY:" + json::stringify(std::string("\xc3", 1) + "X")));
    CHECK(t.quit());
    return "";
}

TEST(tui_Escape_Holds_Queue_And_Preserves_Draft) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("first\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    CHECK(t.send("older\rkept draft"));
    CHECK(t.waitFor("queued 1"));
    CHECK(t.waitFor("> kept draft"));
    CHECK(t.send("\033"));
    CHECK(t.waitFor("(paused;"));
    CHECK(!t.waitFor("REPLY:\"older\"", 150));
    // A status command must not release held prompts. Retain the draft by
    // using history after the command, as a user can do with Ctrl-P.
    CHECK(t.send("\025/session\r"));
    CHECK(t.waitFor("messages:"));
    CHECK(!t.waitFor("REPLY:\"older\"", 100));
    CHECK(t.send("kept draft"));
    CHECK(t.send(" continued\r"));
    CHECK(t.waitFor("REPLY:\"older\""));
    CHECK(t.waitFor("REPLY:\"kept draft continued\""));
    CHECK(t.output.find("REPLY:\"older\"") < t.output.find("REPLY:\"kept draft continued\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Busy_Paste_Decoder_Survives_Turn_End) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("first\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    CHECK(t.send("\033[200~café\nsecond"));
    CHECK(t.waitFor("... second"));
    t.finishFirst();
    CHECK(t.waitFor("REPLY:\"first\""));
    CHECK(t.send(" line\033[201~\033[A\001X\r"));
    CHECK(t.waitFor("REPLY:\"Xcafé\\nsecond line\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Queue_Limit_Retains_Draft_And_Controls_Held_Work) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("first\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    std::string many;
    for (int i = 0; i < 64; ++i) many += "queued-" + std::to_string(i) + "\r";
    CHECK(t.send(many + "overflow retained\r"));
    CHECK(t.waitFor("queued 64"));
    CHECK(t.waitFor("queue full"));
    CHECK(t.waitFor("> overflow retained"));
    CHECK(t.send("\033"));
    CHECK(t.waitFor("(paused;"));
    CHECK(t.send("\r\025/queue status\r"));  // same capacity guard also applies while idle
    CHECK(t.waitFor("queue: 64 held"));
    CHECK(t.output.find("REPLY:\"queued-0\"") == std::string::npos);
    CHECK(t.send("/queue clear\r/queue resume\r"));
    CHECK(t.waitFor("queue: 0 ready"));
    CHECK(t.send("after clearing\r"));
    CHECK(t.waitFor("REPLY:\"after clearing\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Queued_Yes_Does_Not_Approve) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("delayed approval\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    CHECK(t.send("yes do X\rretained draft"));
    CHECK(t.waitFor("queued 1"));
    CHECK(t.waitFor("> retained draft"));
    t.finishFirst();
    CHECK(t.waitFor("Allow once? [y/N]"));
    CHECK(!t.waitFor("allowed once", 120));
    CHECK(t.send("n"));
    CHECK(t.waitFor("REPLY:\"denied\""));
    CHECK(t.waitFor("REPLY:\"yes do X\""));
    CHECK(t.send(" done\r"));
    CHECK(t.waitFor("REPLY:\"retained draft done\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Goal_Controls_While_Busy) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("/goal hold\r"));
    if (!t.waitFor("GOAL_RUNNING")) return "goal start output: " + sanitizeTerminal(t.output);
    CHECK(t.waitFor("goal active"));
    CHECK(t.send("/goal pause\r"));
    CHECK(t.waitFor("goal pause requested"));
    CHECK(t.waitFor("(paused;"));
    CHECK(t.send("/goal status\r"));
    CHECK(t.waitFor("goal: paused"));
    CHECK(t.waitFor("goal paused"));
    CHECK(t.send("/goal resume\r"));
    CHECK(t.waitFor("goal met"));
    CHECK(t.send("/goal status\r"));
    CHECK(t.waitFor("goal: completed"));
    CHECK(t.waitFor("goal completed"));
    t.output.clear();
    CHECK(t.send("/goal hold\r"));
    if (!t.waitFor("GOAL_RUNNING")) return "goal restart output: " + sanitizeTerminal(t.output);
    CHECK(t.send("/goal clear\r"));
    CHECK(t.waitFor("goal clear requested"));
    CHECK(t.waitFor("(paused;"));
    CHECK(t.send("/goal status\r"));
    CHECK(t.waitFor("goal: none"));
    CHECK(t.quit());
    return "";
}

TEST(tui_Line_Mode_Goal_Controls) {
    TuiFixture t(80, 24, true);
    CHECK(t.ready());
    CHECK(t.send("/goal status\n/goal pause\n/goal clear\n/goal resume\n"));
    CHECK(t.waitFor("goal: none"));
    CHECK(t.waitFor("no paused goal to resume"));
    CHECK(t.output.find("REPLY:") == std::string::npos);
    CHECK(t.quit());
    return "";
}

TEST(tui_Oversized_Approval_Cannot_Approve_A_Truncated_Command) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("oversized approval\r"));
    CHECK(t.waitFor("approval text exceeds the 64 KiB display limit"));
    CHECK(t.waitFor("REPLY:\"oversized rejected\""));
    CHECK(t.output.find("Allow once?") == std::string::npos);
    CHECK(t.send("still responsive\r"));
    CHECK(t.waitFor("REPLY:\"still responsive\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Goal_Yields_To_Queued_Followup) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("/goal hold\r"));
    CHECK(t.waitFor("GOAL_RUNNING"));
    CHECK(t.send("change plan\r"));
    CHECK(t.waitFor("queued 1"));
    t.finishFirst();
    CHECK(t.waitFor("USER FOLLOW-UP:\\nchange plan"));
    CHECK(t.waitFor("goal met"));
    CHECK(t.quit());
    return "";
}

TEST(tui_Approval_Posted_After_Cancel_Does_Not_Claim_Input) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("late approval\r"));
    CHECK(t.waitFor("CANCEL_RUNNING"));
    CHECK(t.send("\033"));
    CHECK(t.waitFor("(paused;"));
    CHECK(!t.waitFor("Allow once?", 100));
    CHECK(t.send("still editable\r"));
    CHECK(t.waitFor("REPLY:\"still editable\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Paused_Goal_Explicit_Followup_Resumes) {
    for (bool lineMode : {false, true}) {
        TuiFixture t(80, 24, lineMode, true);
        CHECK(t.ready());
        CHECK(t.send(lineMode ? "resume me\n" : "resume me\r"));
        CHECK(t.waitFor("USER FOLLOW-UP:\\nresume me"));
        CHECK(t.waitFor("goal met"));
        CHECK(t.send(lineMode ? "/goal status\n" : "/goal status\r"));
        CHECK(t.waitFor("goal: completed"));
        CHECK(t.quit());
    }
    return "";
}

TEST(tui_Large_Draft_Resize_Cancel_And_Compact) {
    TuiFixture t(40, 12);
    CHECK(t.ready());
    std::string text(2000, 'x');
    CHECK(t.send("\033[200~" + text + "\033[201~"));
    CHECK(t.send("\033[H\003"));  // clear a long draft while cursor is at its start
    CHECK(t.send("next\r"));
    CHECK(t.waitFor("REPLY:\"next\""));
    CHECK(t.send("a\rb\rc\rd\re\rf\r/compact\r"));
    CHECK(t.waitFor("compacted."));
    CHECK(t.send("after compact\r"));
    CHECK(t.waitFor("REPLY:\"after compact\""));
    winsize small{};
    small.ws_col = 24;
    small.ws_row = 8;
    CHECK(ioctl(t.master, TIOCSWINSZ, &small) == 0);
    CHECK(kill(t.child, SIGWINCH) == 0);
    CHECK(t.send("\014after resize\r"));
    CHECK(t.waitFor("REPLY:\"after resize\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Approval_Cancel_And_Picker_Typeahead) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("approval\r"));
    CHECK(t.waitFor("Allow once? [y/N]"));
    CHECK(t.send("n"));
    CHECK(t.waitFor("REPLY:\"denied\""));
    CHECK(t.send("cancel\r"));
    CHECK(t.waitFor("CANCEL_RUNNING"));
    CHECK(t.send("\003after cancel\r"));
    CHECK(t.waitFor("(stopped;"));
    CHECK(t.waitFor("REPLY:\"after cancel\""));
    // Selection and navigation in a single read must retain the arrow move.
    CHECK(t.send("/thinking\r\033[B\033[B\r"));
    CHECK(t.waitFor("thinking: none"));
    CHECK(t.send("/models\r"));
    CHECK(t.waitFor("subagent"));
    CHECK(t.send("\003/models subagent glm\r"));
    CHECK(t.waitFor("subagent: glm (saved)"));
    CHECK(t.send("/models review glm,glm\r"));
    CHECK(t.waitFor("review: glm,glm (saved)"));
    CHECK(t.send("/models fast glm,glm\r"));
    CHECK(t.waitFor("only the review council accepts multiple"));
    CHECK(t.quit());
    return "";
}

TEST(tui_Queued_Picker_Owns_Its_Typeahead) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("first\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    // An older queued user turn must not consume the picker's arrow keys.
    CHECK(t.send("older\r/thinking\r\033[B\033[B\r"));
    CHECK(t.waitFor("queued 2"));
    CHECK(!t.waitFor("thinking: none", 80));
    t.finishFirst();
    CHECK(t.waitFor("REPLY:\"older\""));
    CHECK(t.waitFor("thinking: none"));
    CHECK(t.output.find("REPLY:\"older\"") < t.output.find("thinking: none"));
    CHECK(t.quit());
    return "";
}

TEST(tui_Queued_Picker_Escape_Holds_Work) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("first\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    CHECK(t.send("/thinking\r\033[B\033[B\r"));
    CHECK(t.waitFor("picker waiting"));
    CHECK(t.send("\033"));
    CHECK(t.waitFor("(paused;"));
    CHECK(!t.waitFor("thinking: none", 100));
    CHECK(t.send("/queue resume\r"));
    CHECK(t.waitFor("thinking: none"));
    CHECK(t.send("composer restored\r"));
    CHECK(t.waitFor("REPLY:\"composer restored\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Queued_Picker_Keys_Never_Approve) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("delayed approval\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    CHECK(t.send("/thinking\r\033[200~yes\033[201~\025high\r"));
    CHECK(t.waitFor("picker waiting"));
    t.finishFirst();
    CHECK(t.waitFor("Allow once? [y/N]"));
    CHECK(!t.waitFor("allowed once", 80));
    CHECK(t.send("n"));
    CHECK(t.waitFor("REPLY:\"denied\""));
    CHECK(t.waitFor("thinking: high"));
    CHECK(t.quit());
    return "";
}

TEST(tui_Queued_Models_Picker_Keeps_Split_Sequences) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("first\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    CHECK(t.send("/models\r"));
    CHECK(t.waitFor("picker waiting"));
    t.output.clear();
    CHECK(t.send("\033["));
    CHECK(t.waitFor("picker waiting"));
    CHECK(t.send("B\033[B\033[B\033[B\rglm\r"));
    t.finishFirst();
    CHECK(t.waitFor("subagent: glm (saved)"));
    CHECK(t.quit());
    return "";
}

TEST(tui_Queued_Picker_Overflow_Preserves_Paste_Boundary) {
    // Exercise an ordinary paste, a split start delimiter at the limit, and
    // nested ESC bytes that must not masquerade as a paste terminator.
    const std::vector<std::string> starts = {
        "\033[200~" + std::string(68000, 'x'),
        std::string(65527, 'x') + "\033[200~" + std::string(3000, 'x'),
        "\033[200~" + std::string(65522, 'x') + "\033\033[201~" + std::string(3000, 'x')
    };
    for (const auto& start : starts) {
    TuiFixture t;
    CHECK(t.ready());
    CHECK(t.send("first\r"));
    CHECK(t.waitFor("FIRST_RUNNING"));
    CHECK(t.send("/thinking\r"));
    CHECK(t.waitFor("picker waiting"));
    CHECK(t.sendDraining(start + "\r/goal clear\003OVERFLOW_TAIL\033[201~"));
    CHECK(t.waitFor("queued picker input full"));
    CHECK(t.waitFor("OVERFLOW_TAIL"));
    CHECK(t.waitFor("(paused;"));
    CHECK(t.output.find("goal clear requested") == std::string::npos);
    CHECK(t.output.find("REPLY:") == std::string::npos);
    CHECK(t.send("\003/queue clear\r"));
    CHECK(t.waitFor("queue: 0 held"));
    CHECK(t.send("after overflow\r"));
    CHECK(t.waitFor("REPLY:\"after overflow\""));
    CHECK(t.quit());
    }
    return "";
}

TEST(tui_Long_Streaming_Line_Stays_In_Viewport) {
    TuiFixture t(40, 12);
    CHECK(t.ready());
    CHECK(t.send("longstream\r"));
    CHECK(t.waitFor("LONG_DONE"));
    const std::string clearPrevious = "\033[A\r\033[K";
    size_t most = 0;
    for (size_t at = 0; (at = t.output.find(clearPrevious, at)) != std::string::npos;) {
        size_t count = 0;
        while (t.output.compare(at, clearPrevious.size(), clearPrevious) == 0) {
            ++count;
            at += clearPrevious.size();
        }
        most = std::max(most, count);
    }
    CHECK(most <= 3);  // four live rows: never erase the older transcript above
    CHECK(t.send("after longstream\r"));
    CHECK(t.waitFor("REPLY:\"after longstream\""));
    CHECK(t.quit());
    return "";
}

TEST(tui_Editor_Layout_Wrap_And_Tabs) {
    auto exact = layoutTuiInput({"abcdef"}, 0, 6, "> ", 8);
    CHECK(exact.rows == std::vector<std::string>({"> abcdef", ""}));
    CHECK(exact.cursorRow == 1 && exact.cursorCol == 0);
    auto tabs = layoutTuiInput({"a\tb"}, 0, 3, "> ", 10);
    CHECK(tabs.rows == std::vector<std::string>({"> a     b"}));
    CHECK(tabs.cursorRow == 0 && tabs.cursorCol == 9);
    auto multiline = layoutTuiInput({"one", "two"}, 0, 1, "> ", 80);
    CHECK(multiline.rows == std::vector<std::string>({"> one", "... two"}));
    CHECK(multiline.cursorRow == 0 && multiline.cursorCol == 3);
    auto huge = layoutTuiInput({std::string(10000, 'x')}, 0, 9000, "> ", 40);
    CHECK(huge.cursorRow == 225 && huge.cursorCol == 2);
    for (const auto& row : huge.rows) CHECK(visibleWidth(row) <= 40);
    return "";
}

TEST(tui_Unicode_Columns) {
    struct Locale {
        locale_t value = newlocale(LC_CTYPE_MASK, "C.UTF-8", nullptr);
        locale_t old = value ? uselocale(value) : (locale_t)0;
        ~Locale() { if (value) { uselocale(old); freelocale(value); } }
    } locale;
    CHECK(locale.value != nullptr);
    CHECK(visibleWidth("你好") == 4);
    CHECK(visibleWidth("é") == 1);
    auto wrap = layoutTuiInput({"éx"}, 0, 4, "> ", 3);
    CHECK(wrap.rows == std::vector<std::string>({"> é", "x"}));
    CHECK(wrap.cursorRow == 1 && wrap.cursorCol == 1);
    auto wide = layoutTuiInput({"你好"}, 0, 6, "", 3);
    CHECK(wide.rows == std::vector<std::string>({"你", "好"}));
    CHECK(wide.cursorRow == 1 && wide.cursorCol == 2);
    return "";
}

TEST(tui_RenderLine) {
    bool fence = false;
    CHECK(renderLine("# Title", fence).find("Title") != std::string::npos);
    CHECK(!fence);
    CHECK(renderLine("- item", fence).find("item") != std::string::npos);
    renderLine("```", fence);
    CHECK(fence);
    std::string code = renderLine("rm -rf x", fence);
    CHECK(code.find("rm -rf x") != std::string::npos);
    renderLine("```", fence);
    CHECK(!fence);
    CHECK(renderLine("use `code` here", fence).find("code") != std::string::npos);
    // Untrusted escape sequences are neutralized, never passed through.
    std::string evil = renderLine("\033]0;pwned\a\033[2Jtext", fence);
    CHECK(evil.find("\033") == std::string::npos && evil.find("text") != std::string::npos);
    std::string c0 = renderLine("a\x01\x07" "b", fence);
    CHECK(c0.find("ab") != std::string::npos);
    return "";
}

TEST(tui_VisibleWidth) {
    CHECK(visibleWidth("hello") == 5);
    CHECK(visibleWidth("") == 0);
    // ANSI color sequences are zero-width (prompt width math depends on this).
    CHECK(visibleWidth("\033[1mhi\033[0m") == 2);
    CHECK(visibleWidth("\033]0;title\aok") == 2);
    // Tabs advance to 8-column stops, like the terminal does.
    CHECK(visibleWidth("a\tb") == 9);
    CHECK(visibleWidth("\t") == 8);
    // C0 controls / DEL are dropped, multibyte chars count 1.
    CHECK(visibleWidth("a\x01\x07" "b") == 2);
    CHECK(visibleWidth("\xc3\xa9") == 1);
    return "";
}

TEST(tui_FmtK) {
    CHECK(fmtK(0) == "0");
    CHECK(fmtK(999) == "999");
    CHECK(fmtK(1000) == "1k");
    CHECK(fmtK(12400) == "12.4k");
    CHECK(fmtK(200000) == "200k");
    CHECK(fmtK(1500000) == "1.5M");
    CHECK(fmtK(-5) == "0");
    return "";
}

TEST(tui_PickFilter) {
    std::vector<std::string> labels = {"glm = orcarouter:z-ai/glm", "deepseek = deepseek:chat"};
    CHECK(pickFilter(labels, "").size() == 2);
    auto g = pickFilter(labels, "GLM");
    CHECK(g.size() == 1 && g[0] == 0);  // case-insensitive
    auto d = pickFilter(labels, "deep");
    CHECK(d.size() == 1 && d[0] == 1);
    CHECK(pickFilter(labels, "zzz").empty());
    return "";
}

TEST(tui_CutBytes) {
    CHECK(cutBytes("hello", 10) == "hello");
    CHECK(cutBytes("hello", 3) == "hel");
    // Never splits a UTF-8 sequence: "é" is 2 bytes, cut at 1 keeps nothing.
    CHECK(cutBytes("a\xc3\xa9" "b", 2) == "a");
    CHECK(cutBytes("a\xc3\xa9" "b", 3) == "a\xc3\xa9");
    return "";
}
