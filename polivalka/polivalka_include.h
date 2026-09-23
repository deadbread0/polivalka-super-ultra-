#include <stdio.h>

enum COMMANDS_T {
    NOTHING,
    WATER
};

byte backslash[8] = {
  0b10000,
  0b01000,
  0b00100,
  0b00010,
  0b00001,
  0b00000,
  0b00000,
  0b00000
};
