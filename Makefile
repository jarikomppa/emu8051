#####################################################################
# Config
#####################################################################
BIN := emu

OPTIM=g
CFLAGS += -O$(OPTIM)
CFLAGS += -pipe
CFLAGS += -g -Wall -Wextra -Wno-unused-parameter -Wshadow

ifeq ($(USE_SWITCH_DISPATCH), 1)
CFLAGS += -DUSE_SWITCH_DISPATCH
endif

ifdef CYCLES_PER_INSTR
CFLAGS += -DCYCLES_PER_INSTR=$(CYCLES_PER_INSTR)
endif

# activate LTO
ifeq ($(USE_LTO), 1)
CFLAGS += -flto -DUSE_LTO=$(USE_LTO)
endif

CFLAGS += $(EXTRA)

LDLIBS += -lcurses

#####################################################################
# Rules
#####################################################################
HEADERS := $(wildcard *.h)
SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

%.o: %.c $(HEADERS)
	 $(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $<

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	-rm -f $(BIN) $(OBJ)

.PHONY: clean all

all: $(BIN)
