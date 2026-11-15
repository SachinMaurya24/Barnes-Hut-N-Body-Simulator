#ifndef BARNES_HUT_HERNQUIST_H
#define BARNES_HUT_HERNQUIST_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <stdexcept>
#include <vector>

#include "bodies.h"

inline long double hernquist_isotropic_df_shape(long double relative_energy) {
    if (relative_energy <= 0.0L || relative_energy >= 1.0L) {
        return 0.0L;
    }
    if (relative_energy < 1.0e-5L) {
        // The closed form subtracts nearly equal terms at low binding energy.
        return (144.0L / 5.0L) * relative_energy * relative_energy
            * std::sqrt(relative_energy);
    }
    const long double root_energy = std::sqrt(relative_energy);
    const long double one_minus_energy = 1.0L - relative_energy;
    const long double bracket = (1.0L - 2.0L * relative_energy)
            * (8.0L * relative_energy * relative_energy - 8.0L * relative_energy - 3.0L)
        + 3.0L * std::asin(root_energy)
            / std::sqrt(relative_energy * one_minus_energy);
    const long double value = root_energy * bracket
        / (one_minus_energy * one_minus_energy);
    return std::max(0.0L, value);
}

inline double sample_hernquist_speed(
    double radius,
    double mass,
    double scale_radius,
    std::mt19937_64& random_engine,
    std::uniform_real_distribution<double>& uniform) {
    constexpr std::size_t grid_size = 2048;
    const double dimensionless_potential = scale_radius / (radius + scale_radius);
    std::array<double, grid_size + 1> cumulative_probability{};
    const auto speed_density = [dimensionless_potential](double speed_fraction) {
        const long double relative_energy = static_cast<long double>(dimensionless_potential)
            * (1.0L - static_cast<long double>(speed_fraction) * speed_fraction);
        const long double shape = hernquist_isotropic_df_shape(relative_energy);
        return static_cast<double>(static_cast<long double>(speed_fraction) * speed_fraction * shape);
    };

    const double step = 1.0 / static_cast<double>(grid_size);
    double previous_density = speed_density(0.0);
    for (std::size_t i = 1; i <= grid_size; ++i) {
        const double current_density = speed_density(static_cast<double>(i) * step);
        cumulative_probability[i] = cumulative_probability[i - 1]
            + 0.5 * (previous_density + current_density) * step;
        previous_density = current_density;
    }
    const double normalization = cumulative_probability.back();
    if (normalization <= 0.0) {
        throw std::runtime_error("Could not normalize Hernquist speed distribution");
    }

    const double sample = uniform(random_engine) * normalization;
    const auto upper = std::lower_bound(
        cumulative_probability.begin(), cumulative_probability.end(), sample);
    const std::size_t upper_index = static_cast<std::size_t>(
        std::distance(cumulative_probability.begin(), upper));
    const std::size_t lower_index = upper_index == 0 ? 0 : upper_index - 1;
    const double interval_probability = cumulative_probability[upper_index]
        - cumulative_probability[lower_index];
    const double fraction = interval_probability > 0.0
        ? (sample - cumulative_probability[lower_index]) / interval_probability : 0.0;
    const double speed_fraction = (static_cast<double>(lower_index) + fraction) * step;
    const double escape_speed = std::sqrt(2.0 * mass / (radius + scale_radius));
    return speed_fraction * escape_speed;
}

inline Bodies make_hernquist_galaxy(
    std::size_t particle_count,
    double model_mass,
    double scale_radius,
    double truncation_radius,
    unsigned long long seed) {
    if (particle_count == 0 || model_mass <= 0.0
        || scale_radius <= 0.0 || truncation_radius <= 0.0) {
        throw std::invalid_argument(
            "Hernquist particle count, mass, scale radius, and truncation radius must be positive");
    }

    const double radius_ratio = truncation_radius / (truncation_radius + scale_radius);
    const double retained_mass_fraction = radius_ratio * radius_ratio;
    const double represented_mass = model_mass * retained_mass_fraction;
    const double particle_mass = represented_mass / static_cast<double>(particle_count);
    std::mt19937_64 random_engine(seed);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    Bodies bodies;
    bodies.positions.reserve(particle_count);
    bodies.velocities.reserve(particle_count);
    bodies.masses.assign(particle_count, particle_mass);

    for (std::size_t i = 0; i < particle_count; ++i) {
        const double cumulative_mass_fraction = uniform(random_engine) * retained_mass_fraction;
        const double square_root_fraction = std::sqrt(cumulative_mass_fraction);
        const double radius = scale_radius * square_root_fraction
            / (1.0 - square_root_fraction);
        const double cos_theta = 2.0 * uniform(random_engine) - 1.0;
        const double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);
        const double azimuth = 2.0 * std::numbers::pi * uniform(random_engine);
        bodies.positions.push_back({
            radius * sin_theta * std::cos(azimuth),
            radius * sin_theta * std::sin(azimuth),
            radius * cos_theta,
        });

        const double speed = sample_hernquist_speed(
            radius, model_mass, scale_radius, random_engine, uniform);
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
    center_of_mass_position = center_of_mass_position / represented_mass;
    center_of_mass_velocity = center_of_mass_velocity / represented_mass;
    for (std::size_t i = 0; i < particle_count; ++i) {
        bodies.positions[i] = bodies.positions[i] - center_of_mass_position;
        bodies.velocities[i] = bodies.velocities[i] - center_of_mass_velocity;
    }
    return bodies;
}

#endif
