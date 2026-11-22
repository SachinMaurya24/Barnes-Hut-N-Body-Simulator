#ifndef BARNES_HUT_ENERGY_H
#define BARNES_HUT_ENERGY_H

#include <cmath>
#include <cstddef>
#include <stdexcept>

#include "bodies.h"
#include "parallel.h"

inline double compute_kinetic_energy(const Bodies& bodies) {
    if (bodies.velocities.size() != bodies.positions.size()
        || bodies.masses.size() != bodies.positions.size()) {
        throw std::invalid_argument("Bodies positions, velocities, and masses must have equal sizes");
    }

    double energy = 0.0;
    for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
        const Vec3& velocity = bodies.velocities[i];
        const double speed_squared = velocity.x * velocity.x
            + velocity.y * velocity.y
            + velocity.z * velocity.z;
        energy += 0.5 * bodies.masses[i] * speed_squared;
    }
    return energy;
}

inline double compute_softened_potential_energy(
    const Bodies& bodies,
    double gravitational_constant,
    double softening_length,
    std::size_t thread_count = default_thread_count()) {
    const std::size_t body_count = bodies.positions.size();
    if (bodies.masses.size() != body_count) {
        throw std::invalid_argument("Bodies positions and masses must have equal sizes");
    }
    if (softening_length < 0.0) {
        throw std::invalid_argument("Softening length must be non-negative");
    }
    if (body_count < 2) {
        return 0.0;
    }

    const double softening_squared = softening_length * softening_length;
    std::vector<double> partial_energies(body_count, 0.0);

    parallel_for_indices(body_count, thread_count, [&](std::size_t i) {
        double partial = 0.0;
        const Vec3 pos_i = bodies.positions[i];
        const double mass_i = bodies.masses[i];
        for (std::size_t j = i + 1; j < body_count; ++j) {
            const Vec3 displacement = bodies.positions[j] - pos_i;
            const double softened_distance = std::sqrt(
                displacement.x * displacement.x
                + displacement.y * displacement.y
                + displacement.z * displacement.z
                + softening_squared);
            partial -= gravitational_constant * mass_i * bodies.masses[j]
                / softened_distance;
        }
        partial_energies[i] = partial;
    });

    double total_energy = 0.0;
    for (double partial : partial_energies) {
        total_energy += partial;
    }
    return total_energy;
}

inline double compute_total_energy(
    const Bodies& bodies,
    double gravitational_constant,
    double softening_length,
    std::size_t thread_count = default_thread_count()) {
    return compute_kinetic_energy(bodies)
        + compute_softened_potential_energy(
            bodies, gravitational_constant, softening_length, thread_count);
}

inline double compute_virial_ratio(
    const Bodies& bodies,
    double gravitational_constant,
    double softening_length,
    std::size_t thread_count = default_thread_count()) {
    const double K = compute_kinetic_energy(bodies);
    const double W = std::abs(compute_softened_potential_energy(
        bodies, gravitational_constant, softening_length, thread_count));
    return W > 1.0e-12 ? (2.0 * K / W) : 0.0;
}

inline double compute_unbound_fraction(
    const Bodies& bodies,
    double gravitational_constant,
    double softening_length,
    int target_component = -1,
    std::size_t thread_count = default_thread_count()) {
    const std::size_t N = bodies.positions.size();
    if (N == 0) return 0.0;
    const double softening_squared = softening_length * softening_length;

    std::vector<int> is_unbound(N, 0);
    std::size_t target_count = 0;

    for (std::size_t i = 0; i < N; ++i) {
        if (target_component == -1 || (i < bodies.components.size() && bodies.components[i] == target_component)) {
            ++target_count;
        }
    }
    if (target_count == 0) return 0.0;

    parallel_for_indices(N, thread_count, [&](std::size_t i) {
        if (target_component != -1 && i < bodies.components.size() && bodies.components[i] != target_component) {
            return;
        }
        const Vec3 v = bodies.velocities[i];
        const double specific_kinetic = 0.5 * (v.x * v.x + v.y * v.y + v.z * v.z);
        double specific_potential = 0.0;
        const Vec3 pos_i = bodies.positions[i];

        for (std::size_t j = 0; j < N; ++j) {
            if (i == j) continue;
            const Vec3 d = bodies.positions[j] - pos_i;
            const double dist = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z + softening_squared);
            specific_potential -= gravitational_constant * bodies.masses[j] / dist;
        }
        if (specific_kinetic + specific_potential > 0.0) {
            is_unbound[i] = 1;
        }
    });

    std::size_t unbound_count = 0;
    for (int u : is_unbound) {
        unbound_count += static_cast<std::size_t>(u);
    }
    return static_cast<double>(unbound_count) / static_cast<double>(target_count);
}

#endif
