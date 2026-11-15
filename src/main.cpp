#include <cstddef>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "bodies.h"
#include "disk.h"
#include "energy.h"
#include "gravity.h"
#include "hernquist.h"
#include "leapfrog.h"
#include "octree.h"
#include "plummer.h"

constexpr double gravitational_constant = 1.0;
constexpr double softening_length = 0.01;

struct EnclosingRadii {
    double r10{};
    double r50{};
    double r90{};
};

struct HernquistProfileSummary {
    EnclosingRadii radii;
    double mass_inside_a{};
    double mass_inside_2a{};
    double mass_inside_5a{};
};

Vec3 compute_total_momentum(const Bodies& bodies) {
    Vec3 momentum{};
    for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
        momentum += bodies.velocities[i] * bodies.masses[i];
    }
    return momentum;
}

EnclosingRadii compute_enclosing_radii(const Bodies& bodies) {
    double total_mass = 0.0;
    Vec3 center_of_mass{};
    for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
        total_mass += bodies.masses[i];
        center_of_mass += bodies.positions[i] * bodies.masses[i];
    }
    if (total_mass <= 0.0) {
        throw std::invalid_argument("Enclosing radii require positive total mass");
    }
    center_of_mass = center_of_mass / total_mass;

    std::vector<std::pair<double, double>> radii_and_masses;
    radii_and_masses.reserve(bodies.positions.size());
    for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
        const Vec3 offset = bodies.positions[i] - center_of_mass;
        const double radius = std::sqrt(
            offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
        radii_and_masses.emplace_back(radius, bodies.masses[i]);
    }
    std::sort(radii_and_masses.begin(), radii_and_masses.end(),
        [](const auto& first, const auto& second) { return first.first < second.first; });

    const auto find_radius = [&radii_and_masses, total_mass](double fraction) {
        const double target_mass = fraction * total_mass;
        double enclosed_mass = 0.0;
        for (const auto& [radius, mass] : radii_and_masses) {
            enclosed_mass += mass;
            if (enclosed_mass >= target_mass) {
                return radius;
            }
        }
        return radii_and_masses.back().first;
    };
    return {find_radius(0.1), find_radius(0.5), find_radius(0.9)};
}

HernquistProfileSummary summarize_hernquist_profile(const Bodies& bodies, double scale_radius) {
    double total_mass = 0.0;
    Vec3 center_of_mass{};
    for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
        total_mass += bodies.masses[i];
        center_of_mass += bodies.positions[i] * bodies.masses[i];
    }
    center_of_mass = center_of_mass / total_mass;
    const auto enclosed_fraction = [&bodies, center_of_mass, total_mass](double radius) {
        double enclosed_mass = 0.0;
        const double radius_squared = radius * radius;
        for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
            const Vec3 offset = bodies.positions[i] - center_of_mass;
            const double distance_squared = offset.x * offset.x
                + offset.y * offset.y + offset.z * offset.z;
            if (distance_squared <= radius_squared) {
                enclosed_mass += bodies.masses[i];
            }
        }
        return enclosed_mass / total_mass;
    };
    return {
        compute_enclosing_radii(bodies),
        enclosed_fraction(scale_radius),
        enclosed_fraction(2.0 * scale_radius),
        enclosed_fraction(5.0 * scale_radius),
    };
}

double total_mass(const Bodies& bodies) {
    double mass = 0.0;
    for (double particle_mass : bodies.masses) {
        mass += particle_mass;
    }
    return mass;
}

Vec3 center_of_mass_position(const Bodies& bodies, std::size_t begin, std::size_t end) {
    double mass = 0.0;
    Vec3 weighted_position{};
    for (std::size_t i = begin; i < end; ++i) {
        mass += bodies.masses[i];
        weighted_position += bodies.positions[i] * bodies.masses[i];
    }
    return weighted_position / mass;
}

ForceSolver parse_force_solver(const char* name) {
    const std::string value(name);
    if (value == "direct") {
        return ForceSolver::Direct;
    }
    if (value == "barnes-hut" || value == "barnes_hut" || value == "bh") {
        return ForceSolver::BarnesHut;
    }
    throw std::invalid_argument("Force solver must be 'direct' or 'barnes-hut'");
}

void write_snapshot(std::ostream& output, const Bodies& bodies, double time) {
    for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
        const Vec3& position = bodies.positions[i];
        const Vec3& velocity = bodies.velocities[i];
        output << time << ',' << i << ','
               << position.x << ',' << position.y << ',' << position.z << ','
               << velocity.x << ',' << velocity.y << ',' << velocity.z << ','
               << bodies.masses[i] << '\n';
    }
}

void write_merger_snapshot(std::ostream& output, const Bodies& bodies,
                           std::size_t particles_per_galaxy, double time) {
    for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
        const Vec3& position = bodies.positions[i];
        const Vec3& velocity = bodies.velocities[i];
        const std::size_t galaxy_id = i < particles_per_galaxy ? 0 : 1;
        output << time << ',' << i << ',' << galaxy_id << ','
               << position.x << ',' << position.y << ',' << position.z << ','
               << velocity.x << ',' << velocity.y << ',' << velocity.z << ','
               << bodies.masses[i] << '\n';
    }
}

double relative_rms_difference(
    const std::vector<Vec3>& first,
    const std::vector<Vec3>& second) {
    double difference_norm_squared = 0.0;
    double reference_norm_squared = 0.0;
    for (std::size_t i = 0; i < first.size(); ++i) {
        const Vec3 difference = first[i] - second[i];
        difference_norm_squared += difference.x * difference.x
            + difference.y * difference.y + difference.z * difference.z;
        reference_norm_squared += second[i].x * second[i].x
            + second[i].y * second[i].y + second[i].z * second[i].z;
    }
    return std::sqrt(difference_norm_squared / reference_norm_squared);
}

void print_force_comparison(std::size_t thread_count) {
    Bodies sample;
    for (int x = 0; x < 4; ++x) {
        for (int y = 0; y < 4; ++y) {
            for (int z = 0; z < 4; ++z) {
                sample.positions.push_back({
                    static_cast<double>(x) - 1.5 + 0.07 * static_cast<double>((y + z) % 2),
                    static_cast<double>(y) - 1.5 + 0.05 * static_cast<double>(x % 2),
                    static_cast<double>(z) - 1.5 + 0.03 * static_cast<double>((x + y) % 3),
                });
                sample.velocities.push_back({});
                sample.masses.push_back(0.8 + 0.1 * static_cast<double>((x + 2 * y + z) % 5));
            }
        }
    }

    constexpr double opening_angles[] = {0.2, 0.5, 0.8};
    compute_direct_accelerations(sample, gravitational_constant, softening_length, 1);
    const std::vector<Vec3> direct_accelerations = sample.accelerations;
    compute_direct_accelerations(sample, gravitational_constant, softening_length, thread_count);
    const double direct_thread_error = relative_rms_difference(
        sample.accelerations, direct_accelerations);

    std::cout << "Barnes–Hut vs direct force (64-particle deterministic sample, threads="
              << thread_count << ")\n"
              << "  direct_single_vs_threaded_relative_rms_difference="
              << std::scientific << std::setprecision(6) << direct_thread_error
              << std::fixed << std::setprecision(10) << '\n';
    for (double theta : opening_angles) {
        const std::vector<Vec3> single_thread_accelerations = compute_barnes_hut_accelerations(
            sample, gravitational_constant, softening_length, theta, 1);
        const std::vector<Vec3> threaded_accelerations = compute_barnes_hut_accelerations(
            sample, gravitational_constant, softening_length, theta, thread_count);
        const double relative_rms_error = relative_rms_difference(
            single_thread_accelerations, direct_accelerations);
        const double tree_thread_error = relative_rms_difference(
            threaded_accelerations, single_thread_accelerations);
        std::cout << "  theta=" << theta << std::scientific << std::setprecision(6)
                  << "  relative_rms_acceleration_error=" << relative_rms_error
                  << "  single_vs_threaded_relative_rms_difference=" << tree_thread_error
                  << std::fixed << std::setprecision(10) << '\n';
    }
}

int run_plummer_mode(int argc, char* argv[]) {
    constexpr std::size_t default_steps = 100;
    constexpr std::size_t default_snapshot_interval = 10;
    constexpr double default_timestep = 0.005;
    if (argc < 4) {
        std::cerr << "Usage: barnes_hut_simulator --plummer PARTICLES SEED [TIMESTEP] [STEPS] "
                     "[SNAPSHOT_INTERVAL] [CSV_PATH] [THREADS] [direct|barnes-hut] [THETA]\n";
        return EXIT_FAILURE;
    }

    const std::size_t particle_count = std::stoull(argv[2]);
    const unsigned long long seed = std::stoull(argv[3]);
    const double timestep = argc > 4 ? std::stod(argv[4]) : default_timestep;
    const std::size_t steps = argc > 5 ? std::stoull(argv[5]) : default_steps;
    const std::size_t snapshot_interval = argc > 6
        ? std::stoull(argv[6]) : default_snapshot_interval;
    const std::string snapshot_path = argc > 7 ? argv[7] : "plummer_snapshots.csv";
    const std::size_t thread_count = argc > 8
        ? std::stoull(argv[8]) : default_thread_count();
    const ForceSolver solver = argc > 9 ? parse_force_solver(argv[9]) : ForceSolver::Direct;
    const double theta = argc > 10 ? std::stod(argv[10]) : 0.5;
    if (timestep <= 0.0 || snapshot_interval == 0 || thread_count == 0) {
        std::cerr << "Timestep, snapshot interval, and thread count must be positive.\n";
        return EXIT_FAILURE;
    }
    if (theta < 0.0) {
        std::cerr << "Theta must be non-negative.\n";
        return EXIT_FAILURE;
    }

    std::ofstream snapshot_file(snapshot_path);
    if (!snapshot_file) {
        std::cerr << "Could not open snapshot file: " << snapshot_path << '\n';
        return EXIT_FAILURE;
    }
    snapshot_file << "time,particle_id,x,y,z,vx,vy,vz,mass\n" << std::setprecision(17);

    Bodies bodies = make_plummer_sphere(particle_count, seed);
    compute_accelerations(bodies, solver, gravitational_constant, softening_length,
                          thread_count, theta);
    const double initial_energy = compute_total_energy(
        bodies, gravitational_constant, softening_length);

    const auto print_diagnostics = [&bodies, initial_energy](double time) {
        const double kinetic = compute_kinetic_energy(bodies);
        const double potential = compute_softened_potential_energy(
            bodies, gravitational_constant, softening_length);
        const double total = kinetic + potential;
        const double virial_ratio = potential != 0.0 ? 2.0 * kinetic / std::abs(potential) : 0.0;
        const double relative_error = initial_energy != 0.0
            ? std::abs(total - initial_energy) / std::abs(initial_energy) : 0.0;
        const Vec3 momentum = compute_total_momentum(bodies);
        const double momentum_magnitude = std::sqrt(
            momentum.x * momentum.x + momentum.y * momentum.y + momentum.z * momentum.z);
        const EnclosingRadii radii = compute_enclosing_radii(bodies);
        std::cout << "t=" << time << "  K=" << kinetic << "  W=" << potential
                  << "  E=" << total << "  relative_energy_error=" << relative_error
                  << "  2K/|W|=" << virial_ratio
                  << "  momentum=(" << momentum.x << ',' << momentum.y << ',' << momentum.z
                  << ")  |P|=" << momentum_magnitude
                  << "  r10=" << radii.r10 << "  r50=" << radii.r50
                  << "  r90=" << radii.r90 << '\n';
    };

    std::cout << std::fixed << std::setprecision(8)
              << "Plummer sphere (N=" << particle_count << ", seed=" << seed
              << ", G=1, M=1, dt=" << timestep << ", steps=" << steps
              << ", epsilon=" << softening_length
              << ", force_solver=" << force_solver_name(solver);
    if (solver == ForceSolver::BarnesHut) {
        std::cout << ", theta=" << theta;
    }
    std::cout << ", snapshot_interval=" << snapshot_interval
              << ", threads=" << thread_count
              << ", snapshots=" << snapshot_path << ")\n";
    write_snapshot(snapshot_file, bodies, 0.0);
    print_diagnostics(0.0);
    for (std::size_t step = 1; step <= steps; ++step) {
        leapfrog_step(bodies, timestep, gravitational_constant, softening_length,
                      thread_count, solver, theta);
        if (step % snapshot_interval == 0 || step == steps) {
            const double time = static_cast<double>(step) * timestep;
            write_snapshot(snapshot_file, bodies, time);
            print_diagnostics(time);
        }
    }
    if (!snapshot_file) {
        std::cerr << "Failed while writing snapshots to: " << snapshot_path << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

double run_energy_error_case(
    const Bodies& initial_bodies,
    double timestep,
    std::size_t steps,
    ForceSolver solver,
    std::size_t thread_count,
    double theta,
    double initial_energy) {
    Bodies bodies = initial_bodies;
    compute_accelerations(bodies, solver, gravitational_constant, softening_length,
                          thread_count, theta);
    for (std::size_t step = 0; step < steps; ++step) {
        leapfrog_step(bodies, timestep, gravitational_constant, softening_length,
                      thread_count, solver, theta);
    }
    const double final_energy = compute_total_energy(
        bodies, gravitational_constant, softening_length);
    return std::abs(final_energy - initial_energy) / std::abs(initial_energy);
}

int run_convergence_mode(int argc, char* argv[]) {
    constexpr std::size_t default_particle_count = 64;
    constexpr unsigned long long default_seed = 42;
    constexpr double default_timestep = 0.005;
    constexpr std::size_t default_steps = 100;
    const std::size_t particle_count = argc > 2
        ? std::stoull(argv[2]) : default_particle_count;
    const unsigned long long seed = argc > 3
        ? std::stoull(argv[3]) : default_seed;
    const double timestep = argc > 4 ? std::stod(argv[4]) : default_timestep;
    const std::size_t steps = argc > 5 ? std::stoull(argv[5]) : default_steps;
    const ForceSolver solver = argc > 6 ? parse_force_solver(argv[6]) : ForceSolver::Direct;
    const std::size_t thread_count = argc > 7
        ? std::stoull(argv[7]) : default_thread_count();
    const double theta = argc > 8 ? std::stod(argv[8]) : 0.5;
    if (particle_count == 0 || timestep <= 0.0 || steps == 0
        || thread_count == 0 || theta < 0.0) {
        std::cerr << "Particle count, timestep, steps, and thread count must be positive; "
                     "theta must be non-negative.\n";
        return EXIT_FAILURE;
    }

    const Bodies initial_bodies = make_plummer_sphere(particle_count, seed);
    const double initial_energy = compute_total_energy(
        initial_bodies, gravitational_constant, softening_length);
    const double coarse_error = run_energy_error_case(
        initial_bodies, timestep, steps, solver, thread_count, theta, initial_energy);
    const double fine_error = run_energy_error_case(
        initial_bodies, timestep * 0.5, steps * 2, solver, thread_count, theta, initial_energy);
    const double error_reduction = fine_error > 0.0 ? coarse_error / fine_error : 0.0;

    std::cout << std::scientific << std::setprecision(8)
              << "Timestep convergence (force_solver=" << force_solver_name(solver);
    if (solver == ForceSolver::BarnesHut) {
        std::cout << ", theta=" << theta;
    }
    std::cout << ", N=" << particle_count << ", seed=" << seed
              << ", total_time=" << timestep * static_cast<double>(steps) << ")\n"
              << "  dt=" << timestep << "  steps=" << steps
              << "  relative_energy_error=" << coarse_error << '\n'
              << "  dt/2=" << timestep * 0.5 << "  steps=" << steps * 2
              << "  relative_energy_error=" << fine_error << '\n'
              << "  coarse_to_fine_error_ratio=" << error_reduction
              << "  (leapfrog expectation is about 4 when integration error dominates)\n";
    return EXIT_SUCCESS;
}

bool validate_isolated_hernquist_halo(
    const Bodies& initial_bodies,
    double model_mass,
    double scale_radius,
    std::size_t thread_count,
    const char* label) {
    constexpr std::size_t steps_per_dynamical_time = 100;
    constexpr double validation_dynamical_times = 5.0;
    const double dynamical_time = std::sqrt(
        scale_radius * scale_radius * scale_radius / model_mass);
    const double timestep = dynamical_time / static_cast<double>(steps_per_dynamical_time);
    const std::size_t steps = static_cast<std::size_t>(
        validation_dynamical_times * static_cast<double>(steps_per_dynamical_time));

    const HernquistProfileSummary initial_profile = summarize_hernquist_profile(
        initial_bodies, scale_radius);
    Bodies isolated_bodies = initial_bodies;
    compute_direct_accelerations(
        isolated_bodies, gravitational_constant, softening_length, thread_count);
    for (std::size_t step = 0; step < steps; ++step) {
        leapfrog_step(isolated_bodies, timestep, gravitational_constant,
                      softening_length, thread_count, ForceSolver::Direct);
    }
    const HernquistProfileSummary final_profile = summarize_hernquist_profile(
        isolated_bodies, scale_radius);

    const double r50_drift = initial_profile.radii.r50 > 0.0
        ? std::abs(final_profile.radii.r50 - initial_profile.radii.r50)
            / initial_profile.radii.r50 : 0.0;
    const double r90_drift = initial_profile.radii.r90 > 0.0
        ? std::abs(final_profile.radii.r90 - initial_profile.radii.r90)
            / initial_profile.radii.r90 : 0.0;
    const double profile_change = std::max({
        std::abs(final_profile.mass_inside_a - initial_profile.mass_inside_a),
        std::abs(final_profile.mass_inside_2a - initial_profile.mass_inside_2a),
        std::abs(final_profile.mass_inside_5a - initial_profile.mass_inside_5a),
    });
    const bool stable = r50_drift < 0.15 && r90_drift < 0.20 && profile_change < 0.10;

    std::cout << "Isolation validation (" << label << ", force_solver=direct, t="
              << validation_dynamical_times << " dynamical times, N="
              << initial_bodies.positions.size() << ")\n"
              << "  r50: " << initial_profile.radii.r50 << " -> "
              << final_profile.radii.r50 << "  r90: " << initial_profile.radii.r90
              << " -> " << final_profile.radii.r90 << '\n'
              << "  M(<a,2a,5a): (" << initial_profile.mass_inside_a << ','
              << initial_profile.mass_inside_2a << ',' << initial_profile.mass_inside_5a
              << ") -> (" << final_profile.mass_inside_a << ','
              << final_profile.mass_inside_2a << ',' << final_profile.mass_inside_5a
              << ")  max_profile_change=" << profile_change
              << "  relative_r50_drift=" << r50_drift
              << "  relative_r90_drift=" << r90_drift
              << "  stable=" << (stable ? "yes" : "no") << '\n';
    return stable;
}

void append_shifted_galaxy(
    Bodies& combined,
    const Bodies& galaxy,
    double center_x,
    double velocity_x) {
    for (std::size_t i = 0; i < galaxy.positions.size(); ++i) {
        Vec3 position = galaxy.positions[i];
        Vec3 velocity = galaxy.velocities[i];
        position.x += center_x;
        velocity.x += velocity_x;
        combined.positions.push_back(position);
        combined.velocities.push_back(velocity);
        combined.masses.push_back(galaxy.masses[i]);
    }
}

int run_merger_mode(int argc, char* argv[], double requested_end_time = -1.0) {
    constexpr std::size_t default_particles_per_galaxy = 128;
    constexpr double default_mass_1 = 1.0;
    constexpr double default_scale_1 = 1.0;
    constexpr double default_mass_2 = 1.0;
    constexpr double default_scale_2 = 1.0;
    constexpr double default_separation = 8.0;
    constexpr double default_approach_speed = 0.45;
    constexpr double default_truncation_scales = 20.0;
    constexpr double default_timestep = 0.01;
    constexpr std::size_t default_steps = 600;
    constexpr double default_theta = 0.5;

    const std::size_t particle_count = argc > 2
        ? std::stoull(argv[2]) : default_particles_per_galaxy;
    const double mass_1 = argc > 3 ? std::stod(argv[3]) : default_mass_1;
    const double scale_1 = argc > 4 ? std::stod(argv[4]) : default_scale_1;
    const double mass_2 = argc > 5 ? std::stod(argv[5]) : default_mass_2;
    const double scale_2 = argc > 6 ? std::stod(argv[6]) : default_scale_2;
    const double separation = argc > 7 ? std::stod(argv[7]) : default_separation;
    const double approach_speed = argc > 8 ? std::stod(argv[8]) : default_approach_speed;
    const double truncation_scales = argc > 9 ? std::stod(argv[9]) : default_truncation_scales;
    const double timestep = argc > 10 ? std::stod(argv[10]) : default_timestep;
    const std::size_t steps = argc > 11 ? std::stoull(argv[11]) : default_steps;
    const std::size_t thread_count = argc > 12
        ? std::stoull(argv[12]) : default_thread_count();
    const ForceSolver solver = argc > 13
        ? parse_force_solver(argv[13]) : ForceSolver::BarnesHut;
    const double theta = argc > 14 ? std::stod(argv[14]) : default_theta;
    const unsigned long long seed = argc > 15 ? std::stoull(argv[15]) : 42ULL;
    const std::size_t snapshot_interval = argc > 16
        ? std::stoull(argv[16]) : std::max<std::size_t>(1, steps / 10);
    const std::string snapshot_path = argc > 17
        ? argv[17] : "results/galaxy_merger_snapshots.csv";

    if (particle_count == 0 || mass_1 <= 0.0 || mass_2 <= 0.0
        || scale_1 <= 0.0 || scale_2 <= 0.0 || separation <= 0.0
        || approach_speed < 0.0 || truncation_scales <= 0.0 || timestep <= 0.0
        || thread_count == 0 || theta < 0.0 || snapshot_interval == 0) {
        std::cerr << "Merger parameters must be positive (approach speed may be zero); "
                     "theta must be non-negative and snapshot interval must be positive.\n";
        return EXIT_FAILURE;
    }

    const Bodies galaxy_1 = make_hernquist_galaxy(
        particle_count, mass_1, scale_1, truncation_scales * scale_1, seed);
    const Bodies galaxy_2 = make_hernquist_galaxy(
        particle_count, mass_2, scale_2, truncation_scales * scale_2, seed + 1ULL);
    std::cout << std::fixed << std::setprecision(6)
              << "Illustrative live Hernquist halo merger; dimensionless N-body units (G=1)\n"
              << "Assumptions: collisionless self-gravitating halos, isotropic equilibrium DF "
                 "velocities, finite outer truncation, no gas or stellar components.\n";
    const bool halo_1_stable = validate_isolated_hernquist_halo(
        galaxy_1, mass_1, scale_1, thread_count, "galaxy 1");
    const bool halo_2_stable = validate_isolated_hernquist_halo(
        galaxy_2, mass_2, scale_2, thread_count, "galaxy 2");
    if (!halo_1_stable || !halo_2_stable) {
        std::cerr << "Isolation validation failed; stopping before combining the halos. "
                     "Increase particle count or revise the truncation and timestep.\n";
        return EXIT_FAILURE;
    }

    const std::filesystem::path snapshot_file_path(snapshot_path);
    std::error_code directory_error;
    if (snapshot_file_path.has_parent_path()) {
        std::filesystem::create_directories(snapshot_file_path.parent_path(), directory_error);
        if (directory_error) {
            std::cerr << "Could not create snapshot directory: "
                      << snapshot_file_path.parent_path().string() << '\n';
            return EXIT_FAILURE;
        }
    }
    std::ofstream snapshot_file(snapshot_file_path);
    if (!snapshot_file) {
        std::cerr << "Could not open merger snapshot file: " << snapshot_path << '\n';
        return EXIT_FAILURE;
    }
    snapshot_file << "time,particle_id,galaxy_id,x,y,z,vx,vy,vz,mass\n"
                  << std::setprecision(17);

    const double represented_mass_1 = total_mass(galaxy_1);
    const double represented_mass_2 = total_mass(galaxy_2);
    const double represented_total_mass = represented_mass_1 + represented_mass_2;
    const double galaxy_1_center_x = -separation * represented_mass_2 / represented_total_mass;
    const double galaxy_2_center_x = separation * represented_mass_1 / represented_total_mass;
    const double galaxy_1_velocity_x = approach_speed * represented_mass_2 / represented_total_mass;
    const double galaxy_2_velocity_x = -approach_speed * represented_mass_1 / represented_total_mass;

    Bodies merger;
    merger.positions.reserve(2 * particle_count);
    merger.velocities.reserve(2 * particle_count);
    merger.masses.reserve(2 * particle_count);
    append_shifted_galaxy(merger, galaxy_1, galaxy_1_center_x, galaxy_1_velocity_x);
    append_shifted_galaxy(merger, galaxy_2, galaxy_2_center_x, galaxy_2_velocity_x);
    compute_accelerations(merger, solver, gravitational_constant, softening_length,
                          thread_count, theta);
    const double initial_energy = compute_total_energy(
        merger, gravitational_constant, softening_length);
    const std::size_t output_interval = std::max<std::size_t>(1, steps / 10);
    const double end_time = requested_end_time > 0.0
        ? requested_end_time : static_cast<double>(steps) * timestep;

    std::cout << "Merger run (N=" << merger.positions.size()
              << ", N_per_galaxy=" << particle_count
              << ", model_masses=" << mass_1 << ',' << mass_2
              << ", represented_masses=" << represented_mass_1 << ',' << represented_mass_2
              << ", scale_radii=" << scale_1 << ',' << scale_2
              << ", truncation_radii=" << truncation_scales * scale_1 << ','
              << truncation_scales * scale_2
              << ", separation=" << separation << ", relative_approach_speed="
              << approach_speed << ", dt=" << timestep << ", steps=" << steps
              << ", force_solver=" << force_solver_name(solver);
    if (solver == ForceSolver::BarnesHut) {
        std::cout << ", theta=" << theta;
    }
    std::cout << ", threads=" << thread_count
              << ", snapshot_interval=" << snapshot_interval
              << ", snapshots=" << snapshot_path << ")\n";

    const auto print_diagnostics = [&merger, initial_energy, particle_count](double time) {
        const double kinetic = compute_kinetic_energy(merger);
        const double potential = compute_softened_potential_energy(
            merger, gravitational_constant, softening_length);
        const double energy = kinetic + potential;
        const double relative_error = std::abs(energy - initial_energy) / std::abs(initial_energy);
        const double virial_ratio = potential != 0.0
            ? 2.0 * kinetic / std::abs(potential) : 0.0;
        const Vec3 momentum = compute_total_momentum(merger);
        const EnclosingRadii radii = compute_enclosing_radii(merger);
        const Vec3 galaxy_1_center = center_of_mass_position(merger, 0, particle_count);
        const Vec3 galaxy_2_center = center_of_mass_position(
            merger, particle_count, merger.positions.size());
        const Vec3 center_offset = galaxy_2_center - galaxy_1_center;
        const double galaxy_center_separation = std::sqrt(
            center_offset.x * center_offset.x + center_offset.y * center_offset.y
            + center_offset.z * center_offset.z);
        std::cout << "t=" << time << " K=" << kinetic << " W=" << potential
                  << " E=" << energy << " relative_energy_error=" << relative_error
                  << " 2K/|W|=" << virial_ratio
                  << " P=(" << momentum.x << ',' << momentum.y << ',' << momentum.z << ')'
                  << " galaxy_center_separation=" << galaxy_center_separation
                  << " r10=" << radii.r10 << " r50=" << radii.r50
                  << " r90=" << radii.r90 << '\n';
    };
    print_diagnostics(0.0);
    write_merger_snapshot(snapshot_file, merger, particle_count, 0.0);
    double elapsed_time = 0.0;
    for (std::size_t step = 1; step <= steps; ++step) {
        const double step_timestep = std::min(timestep, end_time - elapsed_time);
        leapfrog_step(merger, step_timestep, gravitational_constant, softening_length,
                      thread_count, solver, theta);
        elapsed_time += step_timestep;
        if (step % output_interval == 0 || step == steps) {
            print_diagnostics(elapsed_time);
        }
        if (step % snapshot_interval == 0 || step == steps) {
            write_merger_snapshot(snapshot_file, merger, particle_count, elapsed_time);
        }
    }
    if (!snapshot_file) {
        std::cerr << "Failed while writing merger snapshots to: " << snapshot_path << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int run_named_merger_mode(int argc, char* argv[]) {
    double simulation_time = 1.0;
    double timestep = 0.01;
    std::size_t total_particles = 256;
    double separation = 8.0;
    std::string output_path = "results/nbody_run_snapshots.csv";
    std::size_t requested_snapshot_interval = 0;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string option(argv[i]);
            if (option == "--help" || option == "-h") {
                std::cout << "Usage: ./build/nbody [--time T] [--dt DT] "
                             "[--particles TOTAL] [--separation DISTANCE] "
                             "[--snapshot-every STEPS] [--output CSV]\n"
                             "Runs the illustrative two-galaxy merger and saves CSV snapshots.\n";
                return EXIT_SUCCESS;
            }
            if (i + 1 >= argc) {
                std::cerr << "Missing value after " << option << ".\n";
                return EXIT_FAILURE;
            }
            const std::string value(argv[++i]);
            if (option == "--time") {
                simulation_time = std::stod(value);
            } else if (option == "--dt") {
                timestep = std::stod(value);
            } else if (option == "--particles") {
                total_particles = std::stoull(value);
            } else if (option == "--separation") {
                separation = std::stod(value);
            } else if (option == "--snapshot-every") {
                requested_snapshot_interval = std::stoull(value);
                if (requested_snapshot_interval == 0) {
                    throw std::invalid_argument("--snapshot-every must be positive");
                }
            } else if (option == "--output") {
                output_path = value;
            } else {
                std::cerr << "Unknown option: " << option << '\n';
                return EXIT_FAILURE;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Invalid command-line value: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    if (!std::isfinite(simulation_time) || !std::isfinite(timestep)
        || !std::isfinite(separation) || simulation_time <= 0.0 || timestep <= 0.0
        || separation <= 0.0 || total_particles < 2 || total_particles % 2 != 0) {
        std::cerr << "Time, timestep, and separation must be positive; particle count "
                     "must be a positive even total (split equally between the two galaxies).\n";
        return EXIT_FAILURE;
    }

    const double step_ratio = simulation_time / timestep;
    const double rounded_steps = std::ceil(step_ratio);
    if (!std::isfinite(step_ratio) || rounded_steps < 1.0
        || rounded_steps >= static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        std::cerr << "The resulting step count must fit in memory.\n";
        return EXIT_FAILURE;
    }

    const std::size_t steps = static_cast<std::size_t>(rounded_steps);
    const std::size_t particles_per_galaxy = total_particles / 2;
    const std::size_t snapshot_interval = requested_snapshot_interval > 0
        ? requested_snapshot_interval : std::max<std::size_t>(1, steps / 10);
    const std::vector<std::string> arguments = {
        argv[0], "--merger", std::to_string(particles_per_galaxy),
        "1", "1", "1", "1", std::to_string(separation), "0.45", "20",
        std::to_string(timestep), std::to_string(steps),
        std::to_string(default_thread_count()), "barnes-hut", "0.5", "42",
        std::to_string(snapshot_interval), output_path
    };
    std::vector<char*> argument_pointers;
    argument_pointers.reserve(arguments.size());
    for (const std::string& argument : arguments) {
        argument_pointers.push_back(const_cast<char*>(argument.c_str()));
    }
    std::cout << "Custom settings: requested_time=" << simulation_time
              << ", dt=" << timestep << ", steps=" << steps
              << ", total_particles=" << total_particles
              << ", particles_per_galaxy=" << particles_per_galaxy
              << ", separation=" << separation << ", output=" << output_path << '\n';
    return run_merger_mode(static_cast<int>(argument_pointers.size()),
                           argument_pointers.data(), simulation_time);
}

int run_disk_merger_mode(int argc, char* argv[]) {
    std::size_t total_particles = 2048;
    double separation = 11.0;
    double impact_parameter = 2.4;
    double approach_speed = 0.78;
    double timestep = 0.01;
    std::size_t steps = 2400;
    std::size_t snapshot_interval = 15;
    std::string output_path = "results/disk_merger_snapshots.csv";
    double theta = 0.5;
    std::size_t thread_count = default_thread_count();

    for (int i = 2; i < argc; ++i) {
        const std::string option = argv[i];
        if (i + 1 < argc) {
            const std::string value = argv[i + 1];
            if (option == "--particles") {
                total_particles = std::stoull(value);
                ++i;
            } else if (option == "--separation") {
                separation = std::stod(value);
                ++i;
            } else if (option == "--impact") {
                impact_parameter = std::stod(value);
                ++i;
            } else if (option == "--speed") {
                approach_speed = std::stod(value);
                ++i;
            } else if (option == "--dt") {
                timestep = std::stod(value);
                ++i;
            } else if (option == "--steps") {
                steps = std::stoull(value);
                ++i;
            } else if (option == "--snapshot-every") {
                snapshot_interval = std::stoull(value);
                ++i;
            } else if (option == "--output") {
                output_path = value;
                ++i;
            } else if (option == "--theta") {
                theta = std::stod(value);
                ++i;
            } else if (option == "--threads") {
                thread_count = std::stoull(value);
                ++i;
            }
        }
    }

    if (total_particles < 8) {
        total_particles = 8;
    }
    const std::size_t particles_per_galaxy = total_particles / 2;
    const std::size_t bh_parts = 1;
    const std::size_t remaining = particles_per_galaxy - bh_parts;
    const std::size_t disk_parts = (remaining * 55) / 100;
    const std::size_t bulge_parts = (remaining * 15) / 100;
    const std::size_t halo_parts = remaining - disk_parts - bulge_parts;

    DiskGalaxyConfig config1;
    config1.include_black_hole = true;
    config1.disk_particles = disk_parts;
    config1.bulge_particles = bulge_parts;
    config1.halo_particles = halo_parts;
    config1.disk_mass = 0.75;
    config1.bulge_mass = 0.22;
    config1.halo_mass = 2.60;
    config1.black_hole_mass = 0.03;
    config1.disk_scale_radius = 1.0;
    config1.disk_scale_height = 0.10;
    config1.bulge_scale_radius = 0.22;
    config1.halo_scale_radius = 3.0;
    config1.inclination_deg = 28.0;
    config1.argument_of_periapsis_deg = 0.0;
    config1.seed = 42;

    DiskGalaxyConfig config2;
    config2.include_black_hole = true;
    config2.disk_particles = disk_parts;
    config2.bulge_particles = bulge_parts;
    config2.halo_particles = halo_parts;
    config2.disk_mass = 0.75;
    config2.bulge_mass = 0.22;
    config2.halo_mass = 2.60;
    config2.black_hole_mass = 0.03;
    config2.disk_scale_radius = 1.0;
    config2.disk_scale_height = 0.10;
    config2.bulge_scale_radius = 0.22;
    config2.halo_scale_radius = 3.0;
    config2.inclination_deg = -42.0;
    config2.argument_of_periapsis_deg = 55.0;
    config2.seed = 101;

    const Bodies galaxy1 = make_disk_galaxy(config1);
    const Bodies galaxy2 = make_disk_galaxy(config2);

    Bodies merger;
    merger.positions.reserve(particles_per_galaxy * 2);
    merger.velocities.reserve(particles_per_galaxy * 2);
    merger.masses.reserve(particles_per_galaxy * 2);
    merger.components.reserve(particles_per_galaxy * 2);

    const double half_sep = 0.5 * separation;
    const double half_impact = 0.5 * impact_parameter;
    const double half_speed = 0.5 * approach_speed;

    // Shift galaxy 1
    for (std::size_t i = 0; i < galaxy1.positions.size(); ++i) {
        Vec3 pos = galaxy1.positions[i];
        Vec3 vel = galaxy1.velocities[i];
        pos.x -= half_sep;
        pos.y -= half_impact;
        vel.x += half_speed;
        merger.positions.push_back(pos);
        merger.velocities.push_back(vel);
        merger.masses.push_back(galaxy1.masses[i]);
        merger.components.push_back(galaxy1.components[i]);
    }

    // Shift galaxy 2
    for (std::size_t i = 0; i < galaxy2.positions.size(); ++i) {
        Vec3 pos = galaxy2.positions[i];
        Vec3 vel = galaxy2.velocities[i];
        pos.x += half_sep;
        pos.y += half_impact;
        vel.x -= half_speed;
        merger.positions.push_back(pos);
        merger.velocities.push_back(vel);
        merger.masses.push_back(galaxy2.masses[i]);
        merger.components.push_back(galaxy2.components[i]);
    }

    std::filesystem::path path(output_path);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream snapshot_file(output_path);
    if (!snapshot_file) {
        std::cerr << "Could not open output snapshot file: " << output_path << '\n';
        return EXIT_FAILURE;
    }
    snapshot_file << "time,particle_id,galaxy_id,component,x,y,z,vx,vy,vz,mass\n";

    auto write_disk_snapshot = [&](double time) {
        for (std::size_t i = 0; i < merger.positions.size(); ++i) {
            const Vec3& position = merger.positions[i];
            const Vec3& velocity = merger.velocities[i];
            const std::size_t galaxy_id = i < particles_per_galaxy ? 0 : 1;
            const int comp = i < merger.components.size() ? merger.components[i] : 0;
            snapshot_file << time << ',' << i << ',' << galaxy_id << ',' << comp << ','
                          << position.x << ',' << position.y << ',' << position.z << ','
                          << velocity.x << ',' << velocity.y << ',' << velocity.z << ','
                          << merger.masses[i] << '\n';
        }
    };

    // Track true galactic nucleus using Central SMBH + inner bound bulge particles
    auto nuclear_core_position = [&](std::size_t begin, std::size_t end) -> Vec3 {
        // First locate the SMBH particle if present
        Vec3 bh_pos{};
        bool has_bh = false;
        for (std::size_t i = begin; i < end; ++i) {
            if (i < merger.components.size()
                && merger.components[i] == static_cast<int>(ComponentType::BlackHole)) {
                bh_pos = merger.positions[i];
                has_bh = true;
                break;
            }
        }
        double mass = 0.0;
        Vec3 weighted_pos{};
        for (std::size_t i = begin; i < end; ++i) {
            const int comp = i < merger.components.size() ? merger.components[i] : 0;
            if (comp == static_cast<int>(ComponentType::BlackHole)
                || comp == static_cast<int>(ComponentType::Bulge)) {
                if (has_bh) {
                    const Vec3 d = merger.positions[i] - bh_pos;
                    if (d.x * d.x + d.y * d.y + d.z * d.z > 2.25) {
                        continue; // exclude tidally stripped outer bulge outliers
                    }
                }
                mass += merger.masses[i];
                weighted_pos += merger.positions[i] * merger.masses[i];
            }
        }
        return mass > 0.0 ? (weighted_pos / mass) : center_of_mass_position(merger, begin, end);
    };

    constexpr double disk_softening = 0.04;
    std::cout << "Starting Multi-Component Spiral Galaxy Merger (Disk + Bulge + Live DM Halo + Central SMBH)\n"
              << "  Total particles: " << merger.positions.size() << " (" << particles_per_galaxy
              << " per galaxy: " << disk_parts << " disk, " << bulge_parts << " bulge, "
              << halo_parts << " dark matter halo, " << bh_parts << " central SMBH)\n"
              << "  Separation: " << separation << ", Impact parameter: " << impact_parameter
              << ", Approach speed: " << approach_speed << '\n'
              << "  Timestep dt: " << timestep << ", Steps: " << steps
              << " (Total time: " << timestep * static_cast<double>(steps) << ")\n"
              << "  Force solver: Barnes-Hut (theta=" << theta << "), Threads: " << thread_count << '\n'
              << "  Output: " << output_path << '\n';

    compute_accelerations(merger, ForceSolver::BarnesHut, gravitational_constant, disk_softening, thread_count, theta);
    const double initial_energy = compute_total_energy(merger, gravitational_constant, disk_softening, thread_count);

    write_disk_snapshot(0.0);

    const std::size_t output_interval = std::max<std::size_t>(1, steps / 10);
    double elapsed_time = 0.0;

    for (std::size_t step = 1; step <= steps; ++step) {
        leapfrog_step(merger, timestep, gravitational_constant, disk_softening, thread_count, ForceSolver::BarnesHut, theta);
        elapsed_time += timestep;

        if (step % output_interval == 0 || step == steps) {
            const double current_energy = compute_total_energy(merger, gravitational_constant, disk_softening, thread_count);
            const double rel_err = std::abs(current_energy - initial_energy) / std::abs(initial_energy);
            const Vec3 c1 = nuclear_core_position(0, particles_per_galaxy);
            const Vec3 c2 = nuclear_core_position(particles_per_galaxy, merger.positions.size());
            const Vec3 dist = c2 - c1;
            const double separation_now = std::sqrt(dist.x * dist.x + dist.y * dist.y + dist.z * dist.z);
            std::cout << "t=" << std::setw(6) << std::fixed << std::setprecision(2) << elapsed_time
                      << "  E=" << std::setprecision(6) << current_energy
                      << "  rel_err=" << std::scientific << rel_err << std::fixed
                      << "  core_separation=" << std::setprecision(3) << separation_now << '\n';
        }

        if (step % snapshot_interval == 0 || step == steps) {
            write_disk_snapshot(elapsed_time);
        }
    }

    std::cout << "Spiral galaxy merger finished successfully. Saved snapshots to " << output_path << "\n";
    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    constexpr double default_timestep = 0.01;
    constexpr std::size_t default_steps = 1000;
    constexpr std::size_t output_interval = 100;
    if (argc > 1 && (std::string(argv[1]) == "--disk"
                     || std::string(argv[1]) == "--disk-merger"
                     || std::string(argv[1]) == "--spiral")) {
        return run_disk_merger_mode(argc, argv);
    }
    if (argc > 1 && (std::string(argv[1]) == "--time"
                     || std::string(argv[1]) == "--dt"
                     || std::string(argv[1]) == "--particles"
                     || std::string(argv[1]) == "--separation"
                     || std::string(argv[1]) == "--snapshot-every"
                     || std::string(argv[1]) == "--output"
                     || std::string(argv[1]) == "--help"
                     || std::string(argv[1]) == "-h")) {
        return run_named_merger_mode(argc, argv);
    }
    if (argc > 1 && std::string(argv[1]) == "--plummer") {
        std::cout << std::fixed << std::setprecision(8);
        return run_plummer_mode(argc, argv);
    }
    if (argc > 1 && std::string(argv[1]) == "--convergence") {
        return run_convergence_mode(argc, argv);
    }
    if (argc > 1 && std::string(argv[1]) == "--merger") {
        return run_merger_mode(argc, argv);
    }

    const double timestep = argc > 1 ? std::stod(argv[1]) : default_timestep;
    const std::size_t steps = argc > 2 ? std::stoull(argv[2]) : default_steps;
    const std::size_t thread_count = argc > 3
        ? std::stoull(argv[3]) : default_thread_count();
    if (timestep <= 0.0 || thread_count == 0) {
        std::cerr << "Timestep and thread count must be positive.\n";
        return EXIT_FAILURE;
    }

    std::cout << std::fixed << std::setprecision(10);
    print_force_comparison(thread_count);

    Bodies bodies{
        {{-0.5, 0.0, 0.0}, {0.5, 0.0, 0.0}},
        {{0.0, -0.7071, 0.0}, {0.0, 0.7071, 0.0}},
        {},
        {1.0, 1.0},
        {},
    };
    compute_direct_accelerations(bodies, gravitational_constant, softening_length, thread_count);
    const double initial_energy = compute_total_energy(
        bodies, gravitational_constant, softening_length);

    const auto print_diagnostics = [&bodies, initial_energy](double time) {
        const double total_energy = compute_total_energy(
            bodies, gravitational_constant, softening_length);
        const double relative_energy_error = std::abs(total_energy - initial_energy)
            / std::abs(initial_energy);
        std::cout << "t=" << time;
        for (std::size_t i = 0; i < bodies.positions.size(); ++i) {
            const Vec3& position = bodies.positions[i];
            std::cout << "  particle " << i << "=("
                      << position.x << ", " << position.y << ", " << position.z << ")";
        }
        std::cout << "  energy=" << total_energy
                  << std::scientific << std::setprecision(6)
                  << "  relative_energy_error=" << relative_energy_error
                  << std::fixed << std::setprecision(10) << '\n';
    };

    std::cout << "Two-body orbit (dt=" << timestep << ", steps=" << steps << ")\n";
    print_diagnostics(0.0);
    for (std::size_t step = 1; step <= steps; ++step) {
        leapfrog_step(bodies, timestep, gravitational_constant, softening_length, thread_count);
        if (step % output_interval == 0 || step == steps) {
            print_diagnostics(static_cast<double>(step) * timestep);
        }
    }
    return 0;
}
