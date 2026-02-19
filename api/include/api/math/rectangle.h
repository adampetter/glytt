#pragma once
#include "api/math/point.h"
#include "api/math/circle.h"

struct Rectangle
{

public:
    Point Start;
    Point End;

    Rectangle();
    Rectangle(const Rectangle &source);
    Rectangle(int width, int height);
    Rectangle(Point start, Point end);
    ~Rectangle();

    Rectangle operator-(Point point) const;
    Rectangle operator+(Point point) const;
    bool operator==(const Rectangle &s) const;
    bool operator!=(const Rectangle &s) const;
    void operator-=(const Point& point);
    void operator+=(const Point& point);

    bool Intersect(const Rectangle &rectangle) const;
    bool Intersect(const Point &point) const;
    bool Intersect(const Circle &circle) const;
    Rectangle Intersection(const Rectangle &rectangle) const;
    int Width() const;
    int Height() const;
    void Width(int width);
    void Height(int height);
    unsigned int Area() const;
    Point Center() const;
    void Center(const Point center);
    Point Corner(int index) const;
    bool Square() const;
    bool Empty() const;
    int Left() const;
    void Left(int left);
    int Right() const;
    void Right(int right);
    int Top() const;
    void Top(int top);
    int Bottom() const;
    void Bottom(int bottom);
    void Expand(const int amount);
    void Expand(const int x, const int y);
    void Contract(const int amount);
    void Contract(const int x, const int y);
    void Reset();

    static Rectangle Reset(const Rectangle& rectangle);
    static Rectangle Expand(const Rectangle& rectangle, const int amount);
    static Rectangle Expand(const Rectangle& rectangle, const int x, const int y);
    static Rectangle Contract(const Rectangle& rectangle, const int amount);
    static Rectangle Contract(const Rectangle& rectangle, const int x, const int y);
    static Rectangle Centered(const Rectangle& rectangle, const Point center);
};