# Simple host-test Makefile (portable core library)
# Usage:
#   make            # build all
#   make test       # build + run all tests
#   make clean      # remove build artifacts

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
INCLUDES = -Icommon/include

BUILD   := build
BIN     := $(BUILD)/bin
OBJ     := $(BUILD)/obj

COMMON_SRCS := common/src/aer_codec.c \
               common/src/aer_burst.c \
               common/src/ringbuf.c

TEST_CODEC_SRC := tests/test_codec.c
TEST_BURST_SRC := tests/test_burst.c

TEST_CODEC_BIN := $(BIN)/test_codec
TEST_BURST_BIN := $(BIN)/test_burst

HOST_SRCS := host/aer_tx_model.c \
             host/aer_rx_replay.c

TEST_REPLAY_SRC := tests/test_replay.c
TEST_REPLAY_BIN := $(BIN)/test_replay


.PHONY: all test run clean dirs

all: dirs $(TEST_CODEC_BIN) $(TEST_BURST_BIN) $(TEST_REPLAY_BIN)

dirs:
	@mkdir -p $(BIN) $(OBJ)

# --- build executables ---
$(TEST_CODEC_BIN): $(TEST_CODEC_SRC) $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

$(TEST_BURST_BIN): $(TEST_BURST_SRC) $(COMMON_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

$(TEST_REPLAY_BIN): $(TEST_REPLAY_SRC) $(COMMON_SRCS) $(HOST_SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

# --- run tests ---
test: all run


run:
	@echo "== Running codec tests =="
	@$(TEST_CODEC_BIN)
	@echo "== Running burst tests =="
	@$(TEST_BURST_BIN)
	@echo "== Running replay tests =="
	@$(TEST_REPLAY_BIN)

clean:
	@rm -rf $(BUILD)
