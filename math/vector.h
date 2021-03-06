#pragma once
#ifndef MEINEKRAFT_VECTOR_H
#define MEINEKRAFT_VECTOR_H

#include <iostream>
#include <vector>

/************ Forward declarations ************/
template<typename T>
struct Vec2;

template<typename T>
struct Vec3;

template<typename T>
struct Vec4;

template<typename T>
struct Vec4 {
    T x, y, z, w;

    Vec4(T x, T y, T z, T w): x(x), y(y), z(z), w(w) { };
    Vec4(T x, T y, T z): x(x), y(y), z(z), w(0.0f) { };
    Vec4(): x(0), y(0), z(0), w(0) { };
    explicit Vec4(T val): x(val), y(val), z(val), w(val) {};
    explicit Vec4(Vec3<T> vec): x(vec.x), y(vec.y), z(vec.z), w(0.0) { };

    /************ Operators ************/
    /// Returns the members x, y, z, w in index order (invalid indexes returns w)
    T& operator[] (const int index) {
        switch (index) { // Should be a jump table when optimised
            case 0:
                return x;
            case 1:
                return y;
            case 2:
                return z;
            case 3:
                return w;
            default:
                return x;
        }
    }

    /// Returns the members x, y, z, w in index order (invalid indexes returns w)
    const T& operator[] (const int index) const {
      switch (index) { // Should be a jump table when optimised
      case 0:
        return x;
      case 1:
        return y;
      case 2:
        return z;
      case 3:
        return w;
      default:
        return x;
      }
    }
  
    void operator=(const Vec3<T>& rhs) {
      x = rhs.x; y = rhs.y; z = rhs.z; w = 0.0f;
    }

    bool operator==(const Vec4 &rhs) {
        return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
    }

    friend std::ostream &operator<<(std::ostream &os, const Vec4 &vec) {
        return os << "(x:" << vec.x << " y:" << vec.y << " z:" << vec.z << " w:" << vec.w << ")";
    }
};

template<typename T>
struct Vec3 {
    T x, y, z;

    constexpr Vec3(T x, T y, T z): x(x), y(y), z(z) {};
    constexpr explicit Vec3(const Vec4<T>& v): x(v.x), y(v.y), z(v.z) {};
    constexpr explicit Vec3(T val): x(val), y(val), z(val) {};
    constexpr Vec3(): x{}, y{}, z{} {};

    inline static Vec3 zero() { return Vec3(0.0, 0.0, 0.0); }

    /// Unit vector along x-axis
    inline static Vec3 X() { return Vec3(1.0, 0.0, 0.0); }

    /// Unit vector along y-axis
    inline static Vec3 Y() { return Vec3(0.0, 1.0, 0.0); }

    /// Unit vector along z-axis
    inline static Vec3 Z() { return Vec3(0.0, 0.0, 1.0); }

    /// Length of the vector
    inline float length() const { return std::sqrt(std::pow(x, 2) + std::pow(y, 2) + std::pow(z, 2)); }

    /// Normalizes a copy of this vector and returns it
    inline Vec3<T> normalize() const {
        const float length = this->length();
        Vec3<T> result;
        result.x = x / length;
        result.y = y / length;
        result.z = z / length;
        return result;
    }

    /// Sum of the components of the vector
    inline T sum() const { return x + y + z; }

    /// Floors the components
    Vec3<T> floor() const { return {std::floor(x), std::floor(y), std::floor(z)}; }

    /// Result = v x u
    inline Vec3<T> cross(Vec3<T> u) const {
        Vec3<T> result;
        result.x = y * u.z - z * u.y;
        result.y = z * u.x - x * u.z;
        result.z = x * u.y - y * u.x;
        return result;
    }

    /// Dot product of this and the vector u
    inline T dot(Vec3<T> u) const { return x * u.x + y * u.y + z * u.z; }

    /************ Operators ************/
    // Hashing operator
    inline size_t operator()() {
      size_t seed = 0;
      hash_combine(seed, x);
      hash_combine(seed, y);
      hash_combine(seed, z);
      return seed;
    }
    
    inline bool operator<(const Vec3& rhs) const {
        return (x < rhs.x) && (y < rhs.y) && (z < rhs.z);
    }
    
    inline Vec3<T> operator+(const Vec3 &rhs) const {
        return Vec3<T>{x + rhs.x, y + rhs.y, z + rhs.z};
    }

    inline Vec3<T> operator+(const double rhs) const {
        return Vec3<T>{x + rhs, y + rhs, z + rhs};
    }

    inline Vec3<T> operator*(const Vec3 &rhs) const {
        return Vec3<T>{x * rhs.x, y * rhs.y, z * rhs.z};
    }

    inline Vec3<T> operator*(const T s) const {
        return Vec3<T>{x * s, y * s, z * s};
    }
  
    inline Vec3<T> operator*=(const T rhs) const {
      return Vec3<T>{x * rhs, y * rhs, z * rhs};
    }

    inline bool operator==(const Vec3 &rhs) const {
        return (x == rhs.x) && (y == rhs.y) && (z == rhs.z);
    }

    friend std::ostream &operator<<(std::ostream& os, const Vec3 &vec) {
        return os << "(x:" << vec.x << " y:" << vec.y << " z:" << vec.z << ")";
    }

    inline Vec3 operator-(const Vec3 &rhs) const {
        return Vec3{x - rhs.x, y - rhs.y, z - rhs.z};
    }

    inline Vec3 operator-() const {
        return Vec3{-x, -y, -z};
    }

private:
  void hash_combine(size_t &seed, const size_t hash) const {
    seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
};

template<typename T>
inline Vec3<T> operator*(const T s, const Vec3<T>& v) {
    return Vec3<T>{v.x * s, v.y * s, v.z * s};
}

template<typename T>
struct Vec2 {
    T x, y = 0.0f;
    Vec2(T x, T y): x(x), y(y) {};
    Vec2(): x(0), y(0) {};

    /// Sum of the components of the vector
    inline T sum() const { return x + y; }

    /// Floors the components and returns a copy
    inline Vec2<T> floor() const { return {std::floor(x), std::floor(y)}; }

    /// Dot product
    inline T dot(Vec2<T> u) const { return x * u.x + y * u.y; }

    /************ Operators ************/
    Vec2<T> operator+(const Vec2 &rhs) const { return {x + rhs.x, y + rhs.y}; }

    Vec2<T> operator-(const Vec2 &rhs) const { return {x - rhs.x, y - rhs.y}; }

    bool operator==(const Vec2 &rhs) const { return x == rhs.x && y == rhs.y; }

    /// Returns a copy of this vector normalized
    inline Vec2<T> normalize() const {
        double length = this->length();
        Vec2<T> result;
        result.x = x / length;
        result.y = y / length;
        return result;
    }

    /// Length of the vector
    inline double length() const { return std::sqrt(std::pow(x, 2) + std::pow(y, 2)); }

    friend std::ostream &operator<<(std::ostream& os, const Vec2& v) {
      return os << "(x: " << v.x << ", y: " << v.y << std::endl;
    }
};

template<typename T>
struct Mat4 {
private:
    Vec4<T> rows[4];

public:
    inline T *data() { return &rows[0][0]; }

    /// Identity matrix by default
    Mat4<T>() {
        rows[0] = Vec4<T>{1.0f, 0.0f, 0.0f, 0.0f};
        rows[1] = Vec4<T>{0.0f, 1.0f, 0.0f, 0.0f};
        rows[2] = Vec4<T>{0.0f, 0.0f, 1.0f, 0.0f};
        rows[3] = Vec4<T>{0.0f, 0.0f, 0.0f, 1.0f};
    }

    /// Translation - positions the matrix projection in space ...
    inline Mat4<T> set_translation(const Vec3<T>& vec) const {
        Mat4<T> matrix;
        matrix[0] = { 1.0f, 0.0f, 0.0f, 0.0f };
        matrix[1] = { 0.0f, 1.0f, 0.0f, 0.0f };
        matrix[2] = { 0.0f, 0.0f, 1.0f, 0.0f };
        matrix[3] = { vec.x, vec.y, vec.z, 1.0f };
        return matrix;
    }

    /// Gets the translation vector (if that makes sense depends on the matrix) from the matrix
    inline Vec3<T> get_translation() const {
      return Vec3<T>{rows[3][0], rows[3][1], rows[3][2]};
    }

    /// Translation - moves the matrix projection in space ...
    inline Mat4<T> translate(const Vec3<T>& vec) const {
        Mat4<T> matrix;
        matrix[0] = { 1.0f, 0.0f, 0.0f, 0.0f };
        matrix[1] = { 0.0f, 1.0f, 0.0f, 0.0f };
        matrix[2] = { 0.0f, 0.0f, 1.0f, 0.0f };
        matrix[3] = { vec.x, vec.y, vec.z, 1.0f };
        return *this * matrix;
    }

    /// Scales the matrix the same over all axis except w
    inline Mat4<T> scale(const T scale) const {
        Mat4<T> matrix;
        matrix[0] = {scale, 0.0f, 0.0f, 0.0f};
        matrix[1] = {0.0f, scale, 0.0f, 0.0f};
        matrix[2] = {0.0f, 0.0f, scale, 0.0f};
        matrix[3] = {0.0f, 0.0f,  0.0f, 1.0f};
        return *this * matrix;
    }

    /// Transposes the current matrix and returns that matrix
    inline Mat4<T> transpose() {
        Mat4<T> mat;
        mat[0][0] = rows[0][0];
        mat[1][1] = rows[1][1];
        mat[2][2] = rows[2][2];
        mat[3][3] = rows[3][3];

        mat[1][0] = rows[0][1];
        mat[2][0] = rows[0][2];
        mat[3][0] = rows[0][3];
        mat[0][1] = rows[1][0];
        mat[0][2] = rows[2][0];
        mat[0][3] = rows[3][0];

        mat[2][1] = rows[1][2];
        mat[3][1] = rows[1][3];
        mat[1][2] = rows[2][1];
        mat[1][3] = rows[3][1];

        mat[2][3] = rows[3][2];
        mat[3][2] = rows[2][3];
        return mat;
    }

    /************ Operators ************/
    /// Standard matrix multiplication row-column wise; *this * mat
    inline Mat4<T> operator*(const Mat4<T>& mat) const {
        Mat4<T> matrix;
        for (uint8_t i = 0; i < 4; ++i) {
            const auto row = rows[i];
            for (uint8_t j = 0; j < 4; ++j) {
                const Vec4<T> column = Vec4<T>{mat[0][j], mat[1][j], mat[2][j], mat[3][j]};
                matrix.rows[i][j] = row[0]*column[0] + row[1]*column[1] + row[2]*column[2] + row[3]*column[3];
            }
        }
        return matrix;
    }

    /// A * v = b
    inline Vec4<T> operator*(Vec4<T> rhs) const {
        Vec4<T> result;
        for (int i = 0; i < 4; i++) {
            result[i] = rows[i].x * rhs.x + rows[i].y * rhs.y + rows[i].z * rhs.z + rows[i].w * rhs.w;
        }
        return result;
    }

    /// matrix[row_i][colum_j]
    inline Vec4<T>& operator[](const int index) { return rows[index]; }
    inline const Vec4<T>& operator[](const int index) const { return rows[index]; }

    friend std::ostream &operator<<(std::ostream& os, const Mat4 &mat) {
        return os << "\n { \n" << mat.rows[0] << "), \n" << mat.rows[1] << "), \n" << mat.rows[2] << "), \n" << mat.rows[3] << ")\n }";
    }
};

/// Convenience type declarations
using Vec2i = Vec2<int32_t>;
using Vec3i = Vec3<int32_t>;
using Vec4i = Vec4<int32_t>;

using Vec2u = Vec2<uint32_t>;
using Vec3u = Vec3<uint32_t>;
using Vec4u = Vec4<uint32_t>;

using Vec2f = Vec2<float>;
using Vec3f = Vec3<float>;
using Vec4f = Vec4<float>;
using Mat4f = Mat4<float>;

using Vec2d = Vec2<double>;
using Vec3d = Vec3<double>;
using Vec4d = Vec4<double>;
using Mat4d = Mat4<double>;

#endif // MEINEKRAFT_VECTOR_H
