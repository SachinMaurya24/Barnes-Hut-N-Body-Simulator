#ifndef BARNES_HUT_VEC3_H
#define BARNES_HUT_VEC3_H

struct Vec3 {
    double x{};
    double y{};
    double z{};

    constexpr Vec3 operator+(const Vec3& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    constexpr Vec3 operator-(const Vec3& other) const {
        return {x - other.x, y - other.y, z - other.z};
    }

    constexpr Vec3 operator*(double scalar) const {
        return {x * scalar, y * scalar, z * scalar};
    }

    constexpr Vec3 operator/(double scalar) const {
        return {x / scalar, y / scalar, z / scalar};
    }

    constexpr Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
};

constexpr Vec3 operator*(double scalar, const Vec3& vector) {
    return vector * scalar;
}

#endif
