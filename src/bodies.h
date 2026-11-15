#ifndef BARNES_HUT_BODIES_H
#define BARNES_HUT_BODIES_H

#include <vector>

#include "vec3.h"

enum class ComponentType : int {
    Disk = 0,
    Bulge = 1,
    Halo = 2,
    BlackHole = 3,
};

struct Bodies {
    std::vector<Vec3> positions{};
    std::vector<Vec3> velocities{};
    std::vector<Vec3> accelerations{};
    std::vector<double> masses{};
    std::vector<int> components{};
};

#endif
