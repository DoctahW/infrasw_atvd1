CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -g
TARGET  = processflow
SRCDIR  = source
OBJDIR  = objects
TESTDIR = tests

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

MAIN_OBJ = $(OBJDIR)/process_flow.o
LIB_OBJS = $(filter-out $(MAIN_OBJ),$(OBJS))

TEST_BIN = test_tasks

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TEST_BIN): $(TESTDIR)/test_tasks.c $(LIB_OBJS)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $^

test: $(TARGET) $(TEST_BIN)
	@./$(TEST_BIN); u=$$?; \
	 ./$(TESTDIR)/run_tests.sh; e=$$?; \
	 if [ $$u -ne 0 ] || [ $$e -ne 0 ]; then \
	     echo "*** ha testes falhando ***"; exit 1; \
	 fi

test-unit: $(TEST_BIN)
	./$(TEST_BIN)

test-e2e: $(TARGET)
	./$(TESTDIR)/run_tests.sh

clean:
	rm -f $(TARGET) $(TEST_BIN) $(OBJS) $(DEPS)
	rm -rf $(OBJDIR)

.PHONY: all test test-unit test-e2e clean

-include $(DEPS)
