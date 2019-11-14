# QLiquidSFZ
Qt graphical user interface to the liquidsfz library, an SFZ synth player.

## status
This program is a WIP, but it is functionnal.

## prerequisites install
- Qt5 devel files and libraries
- libliquidsfz from the liquidsfz project (available on https://github.com/swesterfeld/liquidsfz)
- libsndfile devel files and libraries (needed by libliquidsfz)

## configuration
in a terminal, type:
$ cd qliquidsfz
$ qmake -config release

## building
in a terminal, inside the qliquidsfz directory, type:
$ make

## installing
in a terminal, inside the qliquidsfz directory, type:
$ sudo make install

## running
click on the newly created applications menu entry "QLiquidSFZ", or if you prefer, type in a terminal:

$ qliquidsfz

enjoy!
