#ifndef BARNES_HUT_DISK_H
#define BARNES_HUT_DISK_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>
#include <stdexcept>
#include <vector>

#include "bodies.h"
#include "vec3.h"

struct DiskGalaxyConfig {
    std::size_t disk_particles{320};
    std::size_t bulge_particles{96};
    std::size_t halo_particles{191};
    bool include_black_hole{true};
    double disk_mass{0.75};
    double bulge_mass{0.22};
    double halo_mass{2.60};
    double black_hole_mass{0.03};
    double disk_scale_radius{1.0};
    double disk_scale_height{0.10};
    double bulge_scale_radius{0.22};
    double halo_scale_radius{3.0};
    double disk_truncation_radius{5.5};
    double halo_truncation_radius{10.0};
    double inclination_deg{35.0};
    double argument_of_periapsis_deg{0.0};
    unsigned long long seed{42};
};

// Invert exponential disk cumulative radial mass fraction:
// F(x) = 1 - (1 + x) * exp(-x) with x = R / R_d.
inline double sample_exponential_disk_radius(
    double scale_radius,
    double truncation_radius,
    std::mt19937_64& random_engine,
    std::uniform_real_distribution<double>& uniform) {
    const double x_max = truncation_radius / scale_radius;
    const double f_max = 1.0 - (1.0 + x_max) * std::exp(-x_max);
    const double u = uniform(random_engine) * f_max;

    // Initial guess from series expansion around x=0: 1 - (1+x)e^-x ~ x^2 / 2 => x ~ sqrt(2u)
    double x = std::sqrt(std::max(0.0, 2.0 * u));
    for (int iter = 0; iter < 10; ++iter) {
        const double exp_neg_x = std::exp(-x);
        const double f = 1.0 - (1.0 + x) * exp_neg_x - u;
        const double f_prime = x * exp_neg_x;
        if (std::abs(f_prime) < 1.0e-12) {
            break;
        }
        const double delta = f / f_prime;
        x -= delta;
        if (std::abs(delta) < 1.0e-8) {
            break;
        }
    }
    x = std::clamp(x, 0.0, x_max);
    return x * scale_radius;
}

inline Bodies make_disk_galaxy(const DiskGalaxyConfig& config) {
    const std::size_t bh_particles = config.include_black_hole ? 1 : 0;
    const std::size_t total_particles = config.disk_particles + config.bulge_particles
        + config.halo_particles + bh_particles;
    if (total_particles == 0) {
        throw std::invalid_argument("Total particles in disk galaxy must be positive");
    }

    const double disk_particle_mass = config.disk_particles > 0
        ? config.disk_mass / static_cast<double>(config.disk_particles) : 0.0;
    const double bulge_particle_mass = config.bulge_particles > 0
        ? config.bulge_mass / static_cast<double>(config.bulge_particles) : 0.0;

    // Retained Hernquist dark matter halo mass inside halo_truncation_radius
    const double halo_trunc_ratio = config.halo_truncation_radius
        / (config.halo_truncation_radius + config.halo_scale_radius);
    const double retained_halo_fraction = halo_trunc_ratio * halo_trunc_ratio;
    const double represented_halo_mass = config.halo_mass * retained_halo_fraction;
    const double halo_particle_mass = config.halo_particles > 0
        ? represented_halo_mass / static_cast<double>(config.halo_particles) : 0.0;
    const double bh_mass = config.include_black_hole ? config.black_hole_mass : 0.0;

    std::mt19937_64 random_engine(config.seed);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    std::normal_distribution<double> normal(0.0, 1.0);

    Bodies galaxy;
    galaxy.positions.reserve(total_particles);
    galaxy.velocities.reserve(total_particles);
    galaxy.masses.reserve(total_particles);
    galaxy.components.reserve(total_particles);

    constexpr double gravitational_constant = 1.0;
    constexpr double softening = 0.04;

    // Helper for total enclosed mass M(<r) across all 4 components (Disk + Bulge + Dark Matter Halo + SMBH)
    auto enclosed_total_mass = [&](double r) -> double {
        const double x_scaled = r / config.disk_scale_radius;
        const double disk_enc = config.disk_mass * (1.0 - (1.0 + x_scaled) * std::exp(-x_scaled));
        const double bulge_ratio = r / (r + config.bulge_scale_radius);
        const double bulge_enc = config.bulge_mass * (bulge_ratio * bulge_ratio);
        const double halo_ratio = r / (r + config.halo_scale_radius);
        const double halo_enc = config.halo_mass * (halo_ratio * halo_ratio);
        return disk_enc + bulge_enc + halo_enc + bh_mass;
    };

    // 0. Central Supermassive Black Hole (SMBH) anchor particle at the galactic nucleus
    if (config.include_black_hole) {
        galaxy.positions.push_back({0.0, 0.0, 0.0});
        galaxy.velocities.push_back({0.0, 0.0, 0.0});
        galaxy.masses.push_back(bh_mass);
        galaxy.components.push_back(static_cast<int>(ComponentType::BlackHole));
    }

    // 1. Generate Exponential Stellar Disk Particles (with flat DM-supported rotation curve & subtle 2-arm spiral structure)
    for (std::size_t i = 0; i < config.disk_particles; ++i) {
        const double radius = sample_exponential_disk_radius(
            config.disk_scale_radius, config.disk_truncation_radius, random_engine, uniform);

        // 55% of disk stars carry a gentle 2-arm logarithmic spiral phase concentration
        double azimuth = 2.0 * std::numbers::pi * uniform(random_engine);
        if (uniform(random_engine) < 0.55 && radius > 0.25 * config.disk_scale_radius) {
            const double arm_offset = (i % 2 == 0) ? 0.0 : std::numbers::pi;
            constexpr double pitch_cot = 2.6; // ~21 deg spiral pitch angle
            const double spiral_angle = arm_offset + pitch_cot * std::log(radius / config.disk_scale_radius);
            azimuth = spiral_angle + 0.38 * normal(random_engine);
        }

        // Vertical sech^2 profile: z = z_0 * atanh(2u - 1)
        const double u_z = std::clamp(uniform(random_engine), 0.001, 0.999);
        const double z = config.disk_scale_height * std::atanh(2.0 * u_z - 1.0);

        const double x = radius * std::cos(azimuth);
        const double y = radius * std::sin(azimuth);

        // Flat rotation curve supported by Disk + Bulge + Dark Matter Halo + SMBH
        const double total_enclosed = enclosed_total_mass(radius);
        const double v_circ = std::sqrt(
            gravitational_constant * total_enclosed * radius
            / (radius * radius + softening * softening + 1.0e-6));

        // Velocity dispersion for Toomre stability
        const double sigma_r = 0.11 * v_circ * std::exp(-radius / (2.5 * config.disk_scale_radius));
        const double sigma_phi = 0.08 * v_circ;
        const double sigma_z = 0.06 * v_circ;

        const double v_rad = sigma_r * normal(random_engine);
        const double v_phi = v_circ + sigma_phi * normal(random_engine);
        const double v_vert = sigma_z * normal(random_engine);

        const double vx = v_rad * std::cos(azimuth) - v_phi * std::sin(azimuth);
        const double vy = v_rad * std::sin(azimuth) + v_phi * std::cos(azimuth);
        const double vz = v_vert;

        galaxy.positions.push_back({x, y, z});
        galaxy.velocities.push_back({vx, vy, vz});
        galaxy.masses.push_back(disk_particle_mass);
        galaxy.components.push_back(static_cast<int>(ComponentType::Disk));
    }

    // 2. Generate Central Stellar Bulge Particles (Hernquist profile in combined potential)
    for (std::size_t i = 0; i < config.bulge_particles; ++i) {
        const double u = std::clamp(uniform(random_engine), 1.0e-5, 0.92);
        const double root_u = std::sqrt(u);
        const double radius = config.bulge_scale_radius * root_u / (1.0 - root_u);
        const double cos_theta = 2.0 * uniform(random_engine) - 1.0;
        const double sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));
        const double phi = 2.0 * std::numbers::pi * uniform(random_engine);

        const double x = radius * sin_theta * std::cos(phi);
        const double y = radius * sin_theta * std::sin(phi);
        const double z = radius * cos_theta;

        const double m_enc = enclosed_total_mass(radius);
        const double sigma_1d = std::sqrt(
            gravitational_constant * m_enc / (3.0 * (radius + config.bulge_scale_radius)));
        const double vx = sigma_1d * normal(random_engine);
        const double vy = sigma_1d * normal(random_engine);
        const double vz = sigma_1d * normal(random_engine);

        galaxy.positions.push_back({x, y, z});
        galaxy.velocities.push_back({vx, vy, vz});
        galaxy.masses.push_back(bulge_particle_mass);
        galaxy.components.push_back(static_cast<int>(ComponentType::Bulge));
    }

    // 3. Generate Live Extended Dark Matter Halo Particles (Hernquist profile)
    for (std::size_t i = 0; i < config.halo_particles; ++i) {
        const double u = std::clamp(uniform(random_engine) * retained_halo_fraction, 1.0e-6, 0.999);
        const double root_u = std::sqrt(u);
        const double radius = config.halo_scale_radius * root_u / (1.0 - root_u);
        const double cos_theta = 2.0 * uniform(random_engine) - 1.0;
        const double sin_theta = std::sqrt(std::max(0.0, 1.0 - cos_theta * cos_theta));
        const double phi = 2.0 * std::numbers::pi * uniform(random_engine);

        const double x = radius * sin_theta * std::cos(phi);
        const double y = radius * sin_theta * std::sin(phi);
        const double z = radius * cos_theta;

        const double m_enc = enclosed_total_mass(radius);
        const double sigma_1d = std::sqrt(
            gravitational_constant * m_enc / (3.0 * (radius + 0.5 * config.halo_scale_radius)));
        const double max_speed = 0.85 * std::sqrt(
            2.0 * gravitational_constant * m_enc / (radius + 0.2 * config.halo_scale_radius));

        Vec3 vel{
            sigma_1d * normal(random_engine),
            sigma_1d * normal(random_engine),
            sigma_1d * normal(random_engine),
        };
        const double speed = std::sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z);
        if (speed > max_speed && speed > 0.0) {
            vel = vel * (max_speed / speed);
        }

        galaxy.positions.push_back({x, y, z});
        galaxy.velocities.push_back(vel);
        galaxy.masses.push_back(halo_particle_mass);
        galaxy.components.push_back(static_cast<int>(ComponentType::Halo));
    }

    // 4. Apply 3D Rotation (Inclination and Argument of Periapsis)
    const double inc_rad = config.inclination_deg * (std::numbers::pi / 180.0);
    const double arg_rad = config.argument_of_periapsis_deg * (std::numbers::pi / 180.0);
    const double cos_i = std::cos(inc_rad);
    const double sin_i = std::sin(inc_rad);
    const double cos_w = std::cos(arg_rad);
    const double sin_w = std::sin(arg_rad);

    for (std::size_t i = 0; i < total_particles; ++i) {
        auto rotate = [&](Vec3 v) -> Vec3 {
            // Rotate around X axis by inclination
            const double y1 = v.y * cos_i - v.z * sin_i;
            const double z1 = v.y * sin_i + v.z * cos_i;
            // Rotate around Z axis by argument of periapsis
            const double x2 = v.x * cos_w - y1 * sin_w;
            const double y2 = v.x * sin_w + y1 * cos_w;
            return {x2, y2, z1};
        };

        galaxy.positions[i] = rotate(galaxy.positions[i]);
        galaxy.velocities[i] = rotate(galaxy.velocities[i]);
    }

    // 5. Recenter Baryonic/Total Bulk Velocity and keep SMBH anchored at origin
    Vec3 com_pos{};
    Vec3 com_vel{};
    double total_mass = 0.0;
    for (std::size_t i = 0; i < total_particles; ++i) {
        total_mass += galaxy.masses[i];
        com_pos += galaxy.positions[i] * galaxy.masses[i];
        com_vel += galaxy.velocities[i] * galaxy.masses[i];
    }
    if (total_mass > 0.0) {
        com_pos = com_pos / total_mass;
        com_vel = com_vel / total_mass;
        for (std::size_t i = 0; i < total_particles; ++i) {
            galaxy.positions[i] = galaxy.positions[i] - com_pos;
            galaxy.velocities[i] = galaxy.velocities[i] - com_vel;
        }
    }

    return galaxy;
}

#endif
