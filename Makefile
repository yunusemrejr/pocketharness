# PocketHarness - boring Makefile. No configure, no cmake, no vendored deps.
CXX ?= g++
# Note: -Wno-error=maybe-uninitialized works around GCC false positives on
# std::variant moves; genuine cases still print as warnings.
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Werror -Wno-error=maybe-uninitialized -MMD -MP -Isrc
LDFLAGS ?=
LDLIBS ?= -lpthread

SRC := src/common.cpp src/json.cpp src/config.cpp src/session.cpp src/skills.cpp \
       src/sandbox.cpp src/process.cpp src/provider.cpp src/tools.cpp src/agent.cpp \
       src/brain.cpp src/catalog.cpp src/kit.cpp src/kit_audio.cpp src/kit_studio.cpp src/kit_media.cpp src/kit_video.cpp src/kit_ops.cpp src/kit_lint.cpp src/oversee.cpp src/wisdom.cpp \
       src/tui.cpp src/main.cpp
OBJ := $(SRC:.cpp=.o)
BIN := pocket

TEST_SRC := tests/test_main.cpp tests/test_json.cpp tests/test_common.cpp tests/test_config.cpp \
            tests/test_session.cpp tests/test_skills.cpp tests/test_sandbox.cpp \
            tests/test_provider.cpp tests/test_tools.cpp tests/test_agent.cpp \
            tests/test_tui.cpp tests/test_process.cpp tests/test_transport.cpp \
            tests/test_brain.cpp tests/test_kit.cpp tests/test_audio.cpp tests/test_video.cpp tests/test_studio.cpp tests/test_cli.cpp
TEST_OBJ := $(TEST_SRC:.cpp=.o)
TEST_LIB := $(filter-out src/main.o,$(OBJ))
TEST_BIN := tests/run_tests

PREFIX ?= $(HOME)/.local
BINDIR := $(PREFIX)/bin
SKILLDIR := $(HOME)/.local/share/pocketharness/skills

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_OBJ) $(TEST_LIB)
	$(CXX) $(LDFLAGS) -o $@ $(TEST_OBJ) $(TEST_LIB) $(LDLIBS)

test: $(BIN) $(TEST_BIN)
	./$(TEST_BIN)

# Allocator/UB/race debugging (not part of default build).
sanitize:
	$(MAKE) clean >/dev/null
	$(MAKE) $(BIN) $(TEST_BIN) CXXFLAGS="-std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc -Wall -Wextra" LDFLAGS="-fsanitize=address,undefined"
	./$(TEST_BIN)

install: $(BIN)
	mkdir -p $(BINDIR)
	@set -eu; staged=$$(mktemp "$(BINDIR)/.pocket-install.XXXXXX"); \
	  trap 'rm -f "$$staged"' EXIT; \
	  install -m 0755 $(BIN) "$$staged"; mv -f "$$staged" "$(BINDIR)/pocket"
	@# Bundled skills are program data: replaced wholesale on every install.
	@# Your own skills live in ~/.config/pocketharness/skills and win on name.
	rm -rf $(SKILLDIR) && mkdir -p $(SKILLDIR) && cp -r skills/. $(SKILLDIR)/
	@echo "installed $(BINDIR)/pocket and $$(ls skills | wc -l) bundled skills"

clean:
	rm -f $(OBJ) $(BIN) $(TEST_OBJ) $(TEST_BIN) $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)

-include $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)

.PHONY: all test sanitize install clean
