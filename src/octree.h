#ifndef BARNES_HUT_OCTREE_H
#define BARNES_HUT_OCTREE_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

#include "bodies.h"
#include "parallel.h"

class Octree {
public:
    struct Node {
        Vec3 center{};
        double half_width{};
        double mass{};
        Vec3 center_of_mass{};
        std::array<int, 8> children{{-1, -1, -1, -1, -1, -1, -1, -1}};
        std::vector<std::size_t> particle_indices;

        Node() = default;

        Node(const Vec3& cube_center, double cube_half_width)
            : center(cube_center), half_width(cube_half_width) {}

        bool is_leaf() const {
            for (int child : children) {
                if (child != -1) {
                    return false;
                }
            }
            return true;
        }
    };

    explicit Octree(const Bodies& bodies) : bodies_(bodies) {
        if (bodies.positions.size() != bodies.masses.size()) {
            throw std::invalid_argument("Bodies positions and masses must have equal sizes");
        }
        if (bodies.positions.empty()) {
            return;
        }

        Vec3 minimum = bodies.positions.front();
        Vec3 maximum = bodies.positions.front();
        for (const Vec3& position : bodies.positions) {
            minimum.x = std::min(minimum.x, position.x);
            minimum.y = std::min(minimum.y, position.y);
            minimum.z = std::min(minimum.z, position.z);
            maximum.x = std::max(maximum.x, position.x);
            maximum.y = std::max(maximum.y, position.y);
            maximum.z = std::max(maximum.z, position.z);
        }

        const Vec3 root_center = (minimum + maximum) * 0.5;
        const double largest_extent = std::max({maximum.x - minimum.x,
                                                maximum.y - minimum.y,
                                                maximum.z - minimum.z});
        const double root_half_width = largest_extent > 0.0
            ? largest_extent * 0.500001
            : 0.5;
        nodes_.reserve(std::max<std::size_t>(64, bodies.positions.size() * 2));
        nodes_.push_back(Node{root_center, root_half_width});

        for (std::size_t particle = 0; particle < bodies.positions.size(); ++particle) {
            insert(0, particle, 0);
        }
        calculate_mass(0);
    }

    const std::vector<Node>& nodes() const {
        return nodes_;
    }

    std::vector<Vec3> accelerations(
        double gravitational_constant,
        double softening_length,
        double theta,
        std::size_t thread_count = default_thread_count()) const {
        if (softening_length < 0.0 || theta < 0.0) {
            throw std::invalid_argument("Softening length and theta must be non-negative");
        }
        std::vector<Vec3> result(bodies_.positions.size());
        if (nodes_.empty()) {
            return result;
        }
        const double softening_squared = softening_length * softening_length;
        const double theta_squared = theta * theta;

        parallel_for_indices(bodies_.positions.size(), thread_count, [&](std::size_t target) {
            result[target] = acceleration_for_target(
                target, gravitational_constant, softening_squared, theta_squared);
        });
        return result;
    }

private:
    static constexpr std::size_t maximum_depth = 64;

    const Bodies& bodies_;
    std::vector<Node> nodes_;

    static std::size_t octant_for(const Vec3& position, const Vec3& center) {
        return (position.x >= center.x ? 1U : 0U)
            | (position.y >= center.y ? 2U : 0U)
            | (position.z >= center.z ? 4U : 0U);
    }

    int create_child(std::size_t parent_index, std::size_t octant) {
        const Vec3 parent_center = nodes_[parent_index].center;
        const double child_half_width = nodes_[parent_index].half_width * 0.5;
        const Vec3 child_center{
            parent_center.x + ((octant & 1U) ? child_half_width : -child_half_width),
            parent_center.y + ((octant & 2U) ? child_half_width : -child_half_width),
            parent_center.z + ((octant & 4U) ? child_half_width : -child_half_width),
        };
        const int child_index = static_cast<int>(nodes_.size());
        nodes_.push_back(Node{child_center, child_half_width});
        nodes_[parent_index].children[octant] = child_index;
        return child_index;
    }

    void insert(std::size_t node_index, std::size_t particle, std::size_t depth) {
        const bool leaf = nodes_[node_index].is_leaf();
        if (leaf) {
            if (nodes_[node_index].particle_indices.empty()) {
                nodes_[node_index].particle_indices.push_back(particle);
                return;
            }
            if (depth >= maximum_depth || nodes_[node_index].half_width <=
                    std::numeric_limits<double>::epsilon()) {
                nodes_[node_index].particle_indices.push_back(particle);
                return;
            }

            std::vector<std::size_t> existing_particles = std::move(nodes_[node_index].particle_indices);
            nodes_[node_index].particle_indices.clear();
            for (std::size_t existing_particle : existing_particles) {
                const std::size_t octant = octant_for(
                    bodies_.positions[existing_particle], nodes_[node_index].center);
                int child_index = nodes_[node_index].children[octant];
                if (child_index == -1) {
                    child_index = create_child(node_index, octant);
                }
                insert(static_cast<std::size_t>(child_index), existing_particle, depth + 1);
            }
        }

        const std::size_t octant = octant_for(bodies_.positions[particle], nodes_[node_index].center);
        int child_index = nodes_[node_index].children[octant];
        if (child_index == -1) {
            child_index = create_child(node_index, octant);
        }
        insert(static_cast<std::size_t>(child_index), particle, depth + 1);
    }

    double calculate_mass(std::size_t node_index) {
        if (nodes_[node_index].is_leaf()) {
            double mass = 0.0;
            Vec3 weighted_position{};
            for (std::size_t particle : nodes_[node_index].particle_indices) {
                const double particle_mass = bodies_.masses[particle];
                mass += particle_mass;
                weighted_position += bodies_.positions[particle] * particle_mass;
            }
            nodes_[node_index].mass = mass;
            nodes_[node_index].center_of_mass = mass != 0.0
                ? weighted_position / mass
                : nodes_[node_index].center;
            return mass;
        }

        double mass = 0.0;
        Vec3 weighted_position{};
        for (int child_index : nodes_[node_index].children) {
            if (child_index == -1) {
                continue;
            }
            const std::size_t child = static_cast<std::size_t>(child_index);
            const double child_mass = calculate_mass(child);
            mass += child_mass;
            weighted_position += nodes_[child].center_of_mass * child_mass;
        }
        nodes_[node_index].mass = mass;
        nodes_[node_index].center_of_mass = mass != 0.0
            ? weighted_position / mass
            : nodes_[node_index].center;
        return mass;
    }

    Vec3 acceleration_for_target(
        std::size_t target,
        double gravitational_constant,
        double softening_squared,
        double theta_squared) const {
        const Vec3 target_position = bodies_.positions[target];
        Vec3 acceleration{};

        struct StackEntry {
            int node_index;
            bool contains_target;
        };

        std::array<StackEntry, 256> stack;
        int stack_top = 0;
        stack[stack_top++] = {0, true};

        while (stack_top > 0) {
            const StackEntry current = stack[--stack_top];
            const Node& node = nodes_[static_cast<std::size_t>(current.node_index)];
            if (node.mass == 0.0) {
                continue;
            }

            if (node.is_leaf()) {
                for (std::size_t source : node.particle_indices) {
                    if (source == target) {
                        continue;
                    }
                    const Vec3 displacement = bodies_.positions[source] - target_position;
                    const double distance_squared = displacement.x * displacement.x
                        + displacement.y * displacement.y
                        + displacement.z * displacement.z
                        + softening_squared;
                    const double inverse_distance_cubed = 1.0
                        / (distance_squared * std::sqrt(distance_squared));
                    acceleration += displacement
                        * (gravitational_constant * bodies_.masses[source] * inverse_distance_cubed);
                }
                continue;
            }

            const Vec3 displacement = node.center_of_mass - target_position;
            const double distance_squared = displacement.x * displacement.x
                + displacement.y * displacement.y
                + displacement.z * displacement.z;
            const double cell_width = 2.0 * node.half_width;

            // Multipole acceptance criterion using squared distances:
            // cell_width / distance < theta <=> (cell_width * cell_width) < (theta * theta * distance_squared)
            // Completely avoids std::sqrt when opening nodes.
            if (!current.contains_target && distance_squared > 0.0
                && (cell_width * cell_width < theta_squared * distance_squared)) {
                const double softened_distance_squared = distance_squared + softening_squared;
                const double inverse_distance_cubed = 1.0
                    / (softened_distance_squared * std::sqrt(softened_distance_squared));
                acceleration += displacement * (gravitational_constant * node.mass * inverse_distance_cubed);
                continue;
            }

            const std::size_t target_octant = octant_for(target_position, node.center);
            for (std::size_t octant = 0; octant < 8; ++octant) {
                const int child_index = node.children[octant];
                if (child_index != -1 && stack_top < static_cast<int>(stack.size())) {
                    stack[stack_top++] = {
                        child_index,
                        current.contains_target && (octant == target_octant)
                    };
                }
            }
        }
        return acceleration;
    }

    Vec3 acceleration_from_node(
        [[maybe_unused]] std::size_t node_index,
        std::size_t target,
        [[maybe_unused]] bool contains_target,
        double gravitational_constant,
        double softening_squared,
        double theta) const {
        return acceleration_for_target(
            target, gravitational_constant, softening_squared, theta * theta);
    }
};

inline std::vector<Vec3> compute_barnes_hut_accelerations(
    const Bodies& bodies,
    double gravitational_constant,
    double softening_length,
    double theta,
    std::size_t thread_count = default_thread_count()) {
    const Octree tree(bodies);
    return tree.accelerations(gravitational_constant, softening_length, theta, thread_count);
}

#endif
