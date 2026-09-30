# PocketHarness - boring Makefile. No configure, no cmake, no vendored deps.
CXX ?= g++
# Note: -Wno-error=maybe-uninitialized works around GCC false positives on
# std::variant moves; genuine cases still print as warnings.
GCC_WARN_WORKAROUND := $(if $(findstring clang,$(notdir $(CXX))),,-Wno-error=maybe-uninitialized)
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Werror $(GCC_WARN_WORKAROUND) -MMD -MP -Isrc
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
	PREFIX="$(PREFIX)" ./scripts/install-built.sh .

check-workflows:
	python3 scripts/check-workflows.py

package: $(BIN)
	./scripts/package-release.sh

bench: tests/benchmark_agent
	./tests/benchmark_agent

tests/benchmark_agent: tests/benchmark_agent.cpp $(TEST_LIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(TEST_LIB) $(LDLIBS)

clean:
	rm -f $(OBJ) $(BIN) $(TEST_OBJ) $(TEST_BIN) tests/benchmark_agent tests/benchmark_agent.d $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)

-include $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)

.PHONY: all test sanitize install clean check-workflows package bench
