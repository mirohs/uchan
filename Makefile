CC = gcc
LINKER = gcc
# https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
CFLAGS = -std=c11 -Wall -Wpointer-arith -Wfatal-errors #-Wpedantic
DEBUG = -g
MATH = -lm

EXE_QS = quicksort
SRC_QS = quicksort.c uchan.c vqueue.c util.c countdown.c
OBJ_QS = $(SRC_QS:.c=.o)

EXE_CST = chan_select_test
SRC_CST = chan_select_test.c uchan.c vqueue.c util.c
OBJ_CST = $(SRC_CST:.c=.o)

EXE_CT = uchan_test
SRC_CT = uchan_test.c uchan.c vqueue.c util.c
OBJ_CT = $(SRC_CT:.c=.o)

EXE_FIB = fib
SRC_FIB = fib.c uchan.c vqueue.c util.c
OBJ_FIB = $(SRC_FIB:.c=.o)

# disable default suffixes
.SUFFIXES:

%.o: %.c
	$(CC) -c $(CFLAGS) $(DEBUG) $< 
	$(CC) -MM $< > $(<:.c=.d)

$(EXE_QS): $(OBJ_QS)
	$(LINKER) $(MATH) -o $(EXE_QS) $(OBJ_QS)

$(EXE_CST): $(OBJ_CST)
	$(LINKER) $(MATH) -o $(EXE_CST) $(OBJ_CST)

$(EXE_CT): $(OBJ_CT)
	$(LINKER) $(MATH) -o $(EXE_CT) $(OBJ_CT)

$(EXE_FIB): $(OBJ_FIB)
	$(LINKER) $(MATH) -o $(EXE_FIB) $(OBJ_FIB)

# include dependency rules
-include $(OBJ:.o=.d)

.PHONY: clean
clean:
	rm -f $(EXE_QS)
	rm -f $(OBJ_QS)
	rm -f $(SRC_QS:.c=.d)
	rm -f $(EXE_CST)
	rm -f $(OBJ_CST)
	rm -f $(SRC_CST:.c=.d)
	rm -f $(EXE_CT)
	rm -f $(OBJ_CT)
	rm -f $(SRC_CT:.c=.d)
	rm -f $(EXE_FIB)
	rm -f $(OBJ_FIB)
	rm -f $(SRC_FIB:.c=.d)
	rm -rf *.dSYM

