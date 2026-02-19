#pragma once

struct Acceleration
{
    short X = 0;
    short Y = 0;
    short Z = 0;

    static const Acceleration Empty;

    Acceleration();
    Acceleration(short x, short y, short z);
    ~Acceleration();

    long long LengthSquared() const;
    float Length() const;

    static short Deadband(short value, short deadband);
    static Acceleration Deadband(const Acceleration &value, short deadband);

    Acceleration operator+(const Acceleration &a) const;
    Acceleration operator-(const Acceleration &a) const;
    Acceleration &operator+=(const Acceleration &a);
    Acceleration &operator-=(const Acceleration &a);

    bool operator==(const Acceleration &a) const;
    bool operator!=(const Acceleration &a) const;
};