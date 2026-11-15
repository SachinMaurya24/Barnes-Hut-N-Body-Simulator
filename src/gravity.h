#ifndef BARNES_HUT_GRAVITY_H
#define BARNES_HUT_GRAVITY_H

#include <cmath>
#include <cstddef>
#include <stdexcept>

#include "bodies.h"
#include "parallel.h"

inline void compute_direct_accelerations(
    Bodies& bodies,
    double gravitational_constant = 1.0,
    double softening_length = 0.0,
    std::size_t thread_count = default_thread_count()) {
    const std::size_t body_count = bodies.positions.size();
    if (bodies.masses.size() != body_count || bodies.velocities.size() != body_count) {
        throw std::invalid_argument("Bodies positions, velocities, and masses must have equal sizes");
    }
    if (softening_length < 0.0) {
        throw std::invalid_argument("Softening length must be non-negative");
    }

    bodies.accelerations.assign(body_count, Vec3{});
    const double softening_squared = softening_length * softening_length;

    parallel_for_indices(body_count, thread_count, [&](std::size_t i) {
        Vec3 acceleration{};
        for (std::size_t j = 0; j < body_count; ++j) {
            if (i == j) {
                continue;
            }

            const Vec3 displacement = bodies.positions[j] - bodies.positions[i];
            const double distance_squared = displacement.x * displacement.x
                + displacement.y * displacement.y
                + displacement.z * displacement.z
                + softening_squared;
            const double inverse_distance_cubed = 1.0 / (distance_squared * std::sqrt(distance_squared));
            acceleration += displacement
                * (gravitational_constant * bodies.masses[j] * inverse_distance_cubed);
        }
        bodies.accelerations[i] = acceleration;
    });
}

#endif
