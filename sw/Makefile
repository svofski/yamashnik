Z80ASM=z80asm-1.8
Z80ASMTAR=z80asm-1.8.tar.gz
Z80ASMURL=http://download.savannah.gnu.org/releases/z80asm/$(Z80ASMTAR)

all:	spyvspy

spyvspy:	z80asm
	make -C spyvspy

z80asm:	$(Z80ASM)/z80asm

$(Z80ASM)/z80asm:	$(Z80ASMTAR)
	tar zxvf $(Z80ASMTAR)
	make -C $(Z80ASM)

$(Z80ASMTAR):
	wget $(Z80ASMURL)

