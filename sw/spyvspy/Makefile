CFLAGS=-g
CPPFLAGS=-g -Wall
CC=g++
CPP=g++
LINK=g++

BUILDDIR=build
MSXNET_CFILES=msxnet.cpp serial.cpp ncopy.cpp sendpacket.cpp diags.c basicsend.cpp spy.cpp util.cpp

MSXNET_OFILES = $(foreach CFILE, $(MSXNET_CFILES), $(BUILDDIR)/$(basename $(notdir $(CFILE))).o)

all:	$(BUILDDIR) $(BUILDDIR)/msxnet $(BUILDDIR)/spy_bootstrap.bin $(BUILDDIR)/tpa.com $(BUILDDIR)/spy_bdos_e900.ram

clean:	
	rm -rf $(BUILDDIR)

$(BUILDDIR)/msxnet: $(MSXNET_OFILES)

$(BUILDDIR)/msxnet.o:	*.h

$(BUILDDIR)/serial.o:	serial.h

$(BUILDDIR)/sendpacket.o:	sendpacket.h

$(BUILDDIR)/spy.o:	spy*.h spybdos.h dosglob.h spydata.h

$(BUILDDIR)/tpa.com:	tpa.asm
	../z80asm-1.8/z80asm -I../z80asm-1.8/headers/ tpa.asm -o $@

$(BUILDDIR)/spy_bootstrap.bin:	spy_bootstrap.asm
	../z80asm-1.8/z80asm -I../z80asm-1.8/headers/ spy_bootstrap.asm -o $@

$(BUILDDIR)/spy_bdos_e900.ram:	spy_bdos_e900.asm inputline.inc
	../z80asm-1.8/z80asm -I../z80asm-1.8/headers/  spy_bdos_e900.asm -o $@

$(BUILDDIR)/spy_bdos_e900.lst:	spy_bdos_e900.asm
	../z80asm-1.8/z80asm -l  -I../z80asm-1.8/headers/  spy_bdos_e900.asm 2>$@

$(BUILDDIR)/%.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/%.o:	%.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

$(BUILDDIR):
	mkdir -p $(BUILDDIR)