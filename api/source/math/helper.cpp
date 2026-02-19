#include "api/math/helper.h"
#include "api/common/types.h"

float map(int x, int in_min, int in_max, int out_min, int out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float map(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float lerpf(float a, float b, float percent)
{
    return (b - a) * percent + a;
}

int lerpi(int a, int b, Byte amount)
{
    float amountf = (float)amount / 255;
    return (b - a) * amountf + a;
}

int max(int a, int b)
{
    return a > b ? a : b;
}

int min(int a, int b)
{
    return a < b ? a : b;
}

float max(float a, float b){
    return a > b ? a : b;
}

float min(float a, float b){
    return a < b ? a : b;
}

void swap(int &a, int &b)
{
    int c = a;
    a = b;
    b = c;
}

void swap(short &a, short &b)
{
    short c = a;
    a = b;
    b = c;
}

float fastsqrt(float val)
{
    const float halfval = 0.5f * val;
    union
    {
        float f;
        uint32_t i;
    } conv = {val};
    conv.i = 0x5f3759df - (conv.i >> 1);
    conv.f = conv.f * (1.5f - halfval * conv.f * conv.f);
    return 1.0f / conv.f;
}

float decimals(float val)
{
    if (val > 0)
        return val - (int)val;
    else
        return val - ((int)val + 1);
}

float rdecimals(float val)
{
    return 1 - decimals(val);
}

int integer(float val)
{
    return (int)val;
}

int clamp(int value, int min, int max)
{
    if (value > max)
        return max;
    else if (value < min)
        return min;
    else
        return value;
}