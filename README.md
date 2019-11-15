# QLiquidSFZ
Qt graphical user interface to the liquidsfz library, an SFZ synth player.

## Status
This program is a WIP, but it is functionnal.

## Prerequisites install
- Qt5 devel files and libraries
- libliquidsfz from the liquidsfz project (available on https://github.com/swesterfeld/liquidsfz)
- libsndfile devel files and libraries (needed by libliquidsfz)

## Configuring
in a terminal, type:

$ cd qliquidsfz

$ qmake -config release

## Building
in a terminal, inside the qliquidsfz directory, type:

$ make

## Installing
in a terminal, inside the qliquidsfz directory, type:

$ sudo make install

## Running
click on the newly created applications menu entry "QLiquidSFZ", or if you prefer, type in a terminal:

$ qliquidsfz

enjoy!
