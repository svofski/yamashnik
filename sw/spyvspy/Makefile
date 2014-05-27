CFLAGS=-g
CPPFLAGS=-g -Wall
CC=g++
CPP=g++
LINK=g++

#all:	ncopy send
all:	msxnet spy_bootstrap.bin tpa.com spy_bdos_e900.ram

clean:	
	rm -f msxnet *.o spy_bootstrap.bin tpa.com spy_bdos_e900.ram

msxnet: msxnet.o serial.o ncopy.o sendpacket.o commands.h diags.o basicsend.o spy.o

serial.o:	serial.h

sendpacket.o:	sendpacket.h

spy.o:	spy*.h spybdos.h dosglob.h

tpa.com:	tpa.asm
	../z80asm-1.8/z80asm -I../z80asm-1.8/headers/ tpa.asm -o tpa.com

spy_bootstrap.bin:	spy_bootstrap.asm
	../z80asm-1.8/z80asm -I../z80asm-1.8/headers/ spy_bootstrap.asm -o spy_bootstrap.bin

spy_bdos_e900.ram:	spy_bdos_e900.asm inputline.inc
	../z80asm-1.8/z80asm -I../z80asm-1.8/headers/  spy_bdos_e900.asm -o spy_bdos_e900.ram 

spy_bdos_e900.lst:	spy_bdos_e900.asm
	../z80asm-1.8/z80asm -l  -I../z80asm-1.8/headers/  spy_bdos_e900.asm 2>spybdos.lst

