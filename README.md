yamashnik
=========
YIS503II MSX-2 MSXNET hardware + software

kicad/ USB to MSXNET adapter schematic + pcb
sw/spyvspy software that emulates server software and supports various boot protocols

HOW TO BUILD
============
You need Linux or Darwin/OSX and a sane build environment. 
wget is needed to fetch z80asm from savannah. 
It should work in Windows with some POSIX-emulation layer but it's not proven. 

1) check out the source
2) cd yamashnik/sw
3) type make


LICENSE
=======
This package is distributed as is. You can do whatever you want with it as long as you give credit to the authors.

Viacheslav Slavinsky http://sensi.org/~svo
Based on research and software by Tim Tashpulatov http://sensi.org/~tnt23/msx
