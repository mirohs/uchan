CC = gcc
LINKER = gcc
# https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
CFLAGS = -std=c11 -Wall -Wpointer-arith -Wfatal-errors #-Wpedantic
DEBUG = -g
MATH = -lm

EXE_QS = quicksort
SRC_QS = quicksort.c uchan.c vqueue.c util.c countdown.c
OBJ_QS = $(SRC_QS:.c=.o)

# disable default suffixes
.SUFFIXES:

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEBUG) $< 
	$(CC) -MM $< > $(<:.c=.d)

$(EXE_QS): $(OBJ_QS)
	$(LINKER) $(MATH) -o $(EXE_QS) $(OBJ_QS)

# include dependency rules
-include $(OBJ:.o=.d)

.PHONY: clean
clean:
	rm -f $(EXE_QS)
	rm -f $(OBJ_QS)
	rm -f $(SRC_QS:.c=.d)
	rm -rf *.dSYM

