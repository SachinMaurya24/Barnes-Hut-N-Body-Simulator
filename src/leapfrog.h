#ifndef BARNES_HUT_LEAPFROG_H
#define BARNES_HUT_LEAPFROG_H

#include <cstddef>
#include <stdexcept>

#include "gravity.h"
#include "octree.h"

enum class ForceSolver {
    Direct,
    BarnesHut,
};

inline const char* force_solver_name(ForceSolver solver) {
    return solver == ForceSolver::Direct ? "direct" : "Barnes-Hut";
}

inline void compute_accelerations(
    Bodies& bodies,
    ForceSolver solver,
    double gravitational_constant,
    double softening_length,
    std::size_t thread_count,
    double theta,
    Octree::OctreeStats* out_stats = nullptr) {
    if (solver == ForceSolver::Direct) {
        compute_direct_accelerations(bodies, gravitational_constant, softening_length, thread_count);
    } else {
        bodies.accelerations = compute_barnes_hut_accelerations(
            bodies, gravitational_constant, softening_length, theta, thread_count, out_stats);
    }
}

inline void leapfrog_step(
    Bodies& bodies,
    double timestep,
    double gravitational_constant,
    double softening_length,
    std::size_t thread_count = default_thread_count(),
    ForceSolver solver = ForceSolver::Direct,
    double theta = 0.5,
    Octree::OctreeStats* out_stats = nullptr) {
    if (bodies.accelerations.size() != bodies.positions.size()) {
        compute_accelerations(
            bodies, solver, gravitational_constant, softening_length, thread_count, theta, out_stats);
    }

    const double half_timestep = 0.5 * timestep;
    for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
        bodies.velocities[i] += bodies.accelerations[i] * half_timestep;
        bodies.positions[i] += bodies.velocities[i] * timestep;
    }

    compute_accelerations(
        bodies, solver, gravitational_constant, softening_length, thread_count, theta, out_stats);

    for (std::size_t i = 0; i < bodies.velocities.size(); ++i) {
        bodies.velocities[i] += bodies.accelerations[i] * half_timestep;
    }
}

#endif
