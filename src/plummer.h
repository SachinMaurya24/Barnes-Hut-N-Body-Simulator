#ifndef BARNES_HUT_PLUMMER_H
#define BARNES_HUT_PLUMMER_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <stdexcept>

#include "bodies.h"

inline Bodies make_plummer_sphere(std::size_t particle_count, unsigned long long seed) {
    if (particle_count == 0) {
        throw std::invalid_argument("Plummer sphere must contain at least one particle");
    }

    constexpr double total_mass = 1.0;
    constexpr double scale_radius = 1.0;
    std::mt19937_64 random_engine(seed);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    Bodies bodies;
    bodies.positions.reserve(particle_count);
    bodies.velocities.reserve(particle_count);
    bodies.masses.assign(particle_count, total_mass / static_cast<double>(particle_count));

    for (std::size_t i = 0; i < particle_count; ++i) {
        const double radial_sample = std::max(uniform(random_engine), 1.0e-12);
        const double radius = scale_radius
            / std::sqrt(std::pow(radial_sample, -2.0 / 3.0) - 1.0);
        const double cos_theta = 2.0 * uniform(random_engine) - 1.0;
        const double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);
        const double azimuth = 2.0 * std::numbers::pi * uniform(random_engine);
        bodies.positions.push_back({
            radius * sin_theta * std::cos(azimuth),
            radius * sin_theta * std::sin(azimuth),
            radius * cos_theta,
        });

        double speed_fraction = 0.0;
        while (true) {
            const double candidate = uniform(random_engine);
            const double acceptance = candidate * candidate
                * std::pow(1.0 - candidate * candidate, 3.5);
            if (0.1 * uniform(random_engine) <= acceptance) {
                speed_fraction = candidate;
                break;
            }
        }

        const double escape_speed = std::sqrt(2.0)
            * std::pow(1.0 + (radius / scale_radius) * (radius / scale_radius), -0.25);
        const double speed = speed_fraction * escape_speed;
        const double velocity_cos_theta = 2.0 * uniform(random_engine) - 1.0;
        const double velocity_sin_theta = std::sqrt(1.0 - velocity_cos_theta * velocity_cos_theta);
        const double velocity_azimuth = 2.0 * std::numbers::pi * uniform(random_engine);
        bodies.velocities.push_back({
            speed * velocity_sin_theta * std::cos(velocity_azimuth),
            speed * velocity_sin_theta * std::sin(velocity_azimuth),
            speed * velocity_cos_theta,
        });
    }

    Vec3 center_of_mass_position{};
    Vec3 center_of_mass_velocity{};
    for (std::size_t i = 0; i < particle_count; ++i) {
        center_of_mass_position += bodies.positions[i] * bodies.masses[i];
        center_of_mass_velocity += bodies.velocities[i] * bodies.masses[i];
    }
    center_of_mass_position = center_of_mass_position / total_mass;
    center_of_mass_velocity = center_of_mass_velocity / total_mass;
    for (std::size_t i = 0; i < particle_count; ++i) {
        bodies.positions[i] = bodies.positions[i] - center_of_mass_position;
        bodies.velocities[i] = bodies.velocities[i] - center_of_mass_velocity;
    }
    return bodies;
}

#endif
