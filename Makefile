CC=gcc
CFLAGS=-O2
LFLAGS=-lm -lsexp

all: ipl-xilinxkicad

ipl-xilinxkicad: main.c Makefile
	@echo [CC] $@
	@$(CC) $(CFLAGS) -o $@ $< $(LFLAGS)

install: ipl-xilinxkicad
	@echo [INSTALL] $<
	@install -m 755 $< /usr/local/bin/
