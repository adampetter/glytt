#pragma once
#include <stdint.h>
#include <climits>

#define DEBUG true
#define DEVELOPMENT true

#define MS_PER_SECOND 1000
#define BITS_PER_Byte 8
#define Byte_MAX_VAL 255
#define PIN GPIO
#define GPIO_NONE -1
#define PIXEL_MAX USHRT_MAX

typedef uint8_t Byte;
typedef int8_t GpioNum;
typedef uint16_t Pixel;
typedef uint8_t Alpha;
typedef unsigned int Identifier;
typedef unsigned int Flags;