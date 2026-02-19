#pragma once
#include "api/common/types.h"

float map(int x, int in_min, int in_max, int out_min, int out_max);
float map(float x, float in_min, float in_max, float out_min, float out_max);
float lerpf(float a, float b, float amount);
int lerpi(int a, int b, Byte amount);
int max(int a, int b);
int min(int a, int b);
float max(float a, float b);
float min(float a, float b);
void swap(int &a, int &b);
void swap(short &a, short &b);
float fastsqrt(float val);
float decimals(float val);
float rdecimals(float val);
int integer(float val);
int clamp(int value, int min, int max);