SDIR=src
ODIR=out
CFLAGS=-g -Wall
LDLIBS=-lXrender -lX11 -lXcomposite -lXdamage -lXfixes -lXext
CC=gcc
EXEC=$(ODIR)/axcomp
SRC= $(wildcard $(SDIR)/*.c)
OBJ= $(SRC:$(SDIR)/%.c=$(ODIR)/%.o)
SHELL=/bin/bash

all: out $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDLIBS)

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) -o $@ $< $(CFLAGS) -c -MMD
	$(CC) -o $@ -c $< $(CFLAGS) $(LDLIBS)

out:
	mkdir $@

run: all
	./$(EXEC) -d :1

run_in_xephyr: all run_xephyr.sh
	sh ./run_xephyr.sh :1 1

run_xephyr: run_xephyr.sh
	sh ./run_xephyr.sh :1

check: $(SDIR)/*.c
	cppcheck --enable=all --suppress=missingIncludeSystem $(SDIR)

clean:
	rm -f $(OBJ) $(ODIR)/*.d

cleaner: clean
	rm -f $(EXEC)

-include $(ODIR)/*.d

.PHONY: all clean run
