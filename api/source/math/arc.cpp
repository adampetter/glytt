#include "api/math/arc.h"

Arc::Arc()
{
}

Arc::Arc(Point center, unsigned short radius, ArcDirection direction)
{
    this->Center = center;
    this->Radius = radius;
    this->Direction = direction;
}

Arc::~Arc(){

}