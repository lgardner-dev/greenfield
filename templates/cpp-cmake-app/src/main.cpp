#include <iostream>

#include "engine/core/Rect.h"
#include "engine/core/Vec2.h"

namespace
{

greenfield::Rect MakeInitialAppBounds()
{
    return greenfield::Rect{
        .position = greenfield::Vec2{0.0f, 0.0f},
        .size = greenfield::Vec2{1280.0f, 720.0f},
    };
}

} // namespace

int main()
{
    const greenfield::Rect initialAppBounds = MakeInitialAppBounds();

    std::cout << "Greenfield app scaffold\n";
    std::cout << "Initial bounds: " << initialAppBounds.size.x << "x" << initialAppBounds.size.y << "\n";
    std::cout << "Wire platform and renderer policy in this app composition root.\n";

    return 0;
}
