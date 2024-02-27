yamashnik
=========
Yamaha YIS-503/IIIR MSX-2 MSX-Link hardware and software

This package facilitates use of a remote MSX-2 workstation equipped with network 
module. The only known instance of such workstation is Yamaha YIS-503/IIIR that used
to be deployed in Russian schools in the 1980s.

To operate, a hardware adapter is needed for the PC. It's a USB to Serial converter
with optocouplers needed to connect to MSX workstations. KiCad schematics and PCB
layout are provided.

The host software supports 3 different modes:

1. CP/M upload: files can be uploaded to the RAM disk of builtin CP/M into remote machine
2. BASIC send: BASIC and ROM files can be transferred over network to remote machines
3. A special version of MSX-DOS can be deployed to the remote machine. Its transfers file system requests to the host machine, which then becomes a file server. Disk based MSX-DOS software can be used using this mode. MSX-DOS files MSXDOS.SYS and COMMAND.COM are not included in this package.

#### Compatibility list
- Turbo Pascal 3.0
- Zork

PACKAGE CONTENTS
----------------
- `kicad/` USB to MSXNET adapter [schematic](kicad/yamashnik.pdf) + PCB layout for home making
- `sw/spyvspy/` software that emulates server software and supports various boot protocols

HOW TO BUILD
------------

### MSX-Link Hardware
If you need help, ask someone else to do it for you. There are RS-232 based variants of the adapter too. All through-hole version [here](https://sensi.org/~tnt23/msx) and a breadboard version [here](https://web.archive.org/web/20080620084238/http://msxlink.chat.ru/).

### Software
You need Linux or Darwin/OSX and a sane build environment. `wget` utility is needed to fetch z80asm from savannah. 

It should work in Windows with some POSIX-emulation layer but it's not proven. 

  1. check out or download the source
  2. cd yamashnik/sw
  3. type make
  4. if all is well, msxnet and remote bootstrap files are in yamashnik/sw/spyvspy/build subdirectory

HOW TO USE
----------
[ ] todo

LICENSE
-------
This package is distributed as is. You can do whatever you want with it as long as you give credit to the authors.

- Viacheslav Slavinsky http://sensi.org/~svo
- Based on research and software by Tim Tashpulatov http://sensi.org/~tnt23/msx
