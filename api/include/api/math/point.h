

#pragma once

struct Point
{

public:
    int X = 0;
    int Y = 0;

    Point();
    Point(const Point& source);
    Point(int x, int y);
    ~Point();

    Point operator-(Point point) const;
    Point operator+(Point point) const;
    Point operator*(int value) const;
    Point operator/(int value) const;

    void operator-=(int value);
    void operator+=(int value);
    void operator*=(int value);
    void operator/=(int value);

    void operator-=(const Point &point);
    void operator+=(const Point &point);
    void operator*=(const Point &point);
    void operator/=(const Point &point);

    bool operator==(const Point &point) const;
    bool operator!=(const Point &point) const;    

    unsigned short Length() const;
    unsigned int LengthSquared() const;
    long long int Pack() const;
    void Pack(long long int* output) const;
    bool Zero();
    static void Swap(Point& a, Point& b);
    static int DistanceSquared(const Point& a, const Point& b);
    static Point Rotate(const Point& point, const Point& center, float angle);
    static Point Unpack(const long long int& packed);
    static void Unpack(const long long int &packed, Point* output);
};