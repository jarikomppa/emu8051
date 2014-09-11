#sudo apt-get install libncurses5 libncurses5-dev

HEADERS = emu8051.h  emulator.h
OBJ = core.o  disasm.o  emu.o  logicboard.o  mainview.o  memeditor.o  opcodes.o  options.o  popups.o

CC = gcc
CCPP = g++
CFLAGS = -g -Wall -Wextra

%.o: %.c $(HEADERS)
	$(CC) $(CLFLAGS)-c -o $@ $< $(CFLAGS)

# %.o: %.cpp $(HEADERS)
# 	$(CCPP) $(CLFLAGS)-c -o $@ $< $(CFLAGS)

emu: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o emu -lcurses
clean:
	-rm -rf *.o emu
