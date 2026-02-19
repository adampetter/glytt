#pragma once
#include "api/common/types.h"

void printDecHexBin(Byte value);
void printDecimal(Byte value, bool newline = false);
void printBinary(Byte value, bool newline = false);
void printHex(Byte value, bool newline = false);
int boolToDec(bool bools[]);
bool contains(const char* haystack, const char* key);
int indexof(const char* haystack, const char* key);