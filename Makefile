# PocketHarness - boring Makefile. No configure, no cmake, no vendored deps.
CXX ?= g++
# Note: -Wno-error=maybe-uninitialized works around GCC false positives on
# std::variant moves; genuine cases still print as warnings.
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Werror -Wno-error=maybe-uninitialized -MMD -MP -Isrc
LDFLAGS ?=
LDLIBS ?= -lpthread

SRC := src/common.cpp src/json.cpp src/config.cpp src/session.cpp src/skills.cpp \
       src/sandbox.cpp src/process.cpp src/provider.cpp src/tools.cpp src/agent.cpp \
       src/tui.cpp src/main.cpp
OBJ := $(SRC:.cpp=.o)
BIN := pocket

TEST_SRC := tests/test_main.cpp tests/test_json.cpp tests/test_common.cpp tests/test_config.cpp \
            tests/test_session.cpp tests/test_skills.cpp tests/test_sandbox.cpp \
            tests/test_provider.cpp tests/test_tools.cpp tests/test_agent.cpp \
            tests/test_tui.cpp tests/test_process.cpp
TEST_OBJ := $(TEST_SRC:.cpp=.o)
TEST_LIB := $(filter-out src/main.o,$(OBJ))
TEST_BIN := tests/run_tests

PREFIX ?= $(HOME)/.local
BINDIR := $(PREFIX)/bin

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
	$(MAKE) $(TEST_BIN) CXXFLAGS="-std=c++20 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc -Wall -Wextra" LDFLAGS="-fsanitize=address,undefined"
	./$(TEST_BIN)

install: $(BIN)
	mkdir -p $(BINDIR)
	install -m 0755 $(BIN) $(BINDIR)/pocket
	@echo "installed $(BINDIR)/pocket"

# Optional: copy the bundled skills into the user skill dir. Never runs as
# part of `install`: skills are user configuration, not program files.
install-skills:
	mkdir -p $(HOME)/.config/pocketharness/skills
	cp -r skills/* $(HOME)/.config/pocketharness/skills/
	@echo "installed skills to $(HOME)/.config/pocketharness/skills"

clean:
	rm -f $(OBJ) $(BIN) $(TEST_OBJ) $(TEST_BIN) $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)

-include $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)

.PHONY: all test sanitize install install-skills clean
