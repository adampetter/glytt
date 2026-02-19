#pragma once
#include "api/math/vector2.h"
#include "api/math/rectangle.h"
#include "api/navigation/coordinate.h"

struct Region{

    public:
        Vector2 Start;
        Vector2 End;

        Region();
        Region(Vector2 start, Vector2 end);
        ~Region();

        bool operator ==(const Region &s) const;
        bool operator !=(const Region &s) const;

        bool Intersect(const Region& region) const;
        bool Intersect(const Vector2& coordinate) const;
        bool Intersect(const Vector2& coordinate, const CoordinateType type) const;
        Region Intersection(const Region& region) const;
        float Width() const;
        float Height() const;
        float Area() const;
        void Center(Vector2* output) const;
        bool Empty() const;
        void Displacement(const Vector2& coordinate, const CoordinateType type, Vector2* output) const;
};