#include "api/common/utility.h"
#include <cstdio>

void printDecHexBin(Byte value)
{
    printDecimal(value);
    printHex(value);
    printBinary(value, true);
}

void printDecimal(Byte value, bool newline)
{
    printf("DEC: %i %s", value, newline ? "\n" : "");
}

void printBinary(Byte value, bool newline)
{
    printf("BIN: ");

    int a[10] = {0};
    int i;

    // Loop to calculate and store the binary format
    for (i = 0; value > 0; i++)
    {
        a[i] = value % 2;
        value = value / 2;
    }

    if (i - 1 < 0)
    {
        printf("0");
    }
    else
    {
        for (int k = 8 - i; k > 0; k--)
        {
            printf("0");
        }
    }

    // Loop to print the binary format of given number
    for (i = i - 1; i >= 0; i--)
        printf("%d", a[i]);

    if (newline)
        printf("\n");
}

void printHex(Byte value, bool newline)
{
    printf("HEX: %#x %s", value, newline ? "\n" : "");
}

int boolToDec(bool bools[])
{
    int num = 0;

    for (int i = 0; i < 8; i++)
        num += bools[i] * (1 << (7 - i));

    return num;
}

bool contains(const char *haystack, const char *key)
{
    return indexof(haystack, key) >= 0;
}

int indexof(const char *haystack, const char *key)
{
    int i = 0;
    int j = 0;

    while (haystack[i] != '\0')
    {
        if (haystack[i] == key[j])
        {
            while (haystack[i] == key[j] && key[j] != '\0')
            {
                j++;
                i++;
            }

            if (key[j] == '\0')
                return i;

            j = 0;
        }
        i++;
    }
    return -1;
}