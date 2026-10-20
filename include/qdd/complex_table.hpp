#pragma once

#include <complex>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cmath>

namespace qdd {

using CIdx = std::uint32_t;

inline constexpr CIdx kC0 = 0;              // 0
inline constexpr CIdx kC1 = 1;              // 1
inline constexpr CIdx kCMinus1 = 2;         // -1
inline constexpr CIdx kCI = 3;              // i
inline constexpr CIdx kCMinusI = 4;         // -i
inline constexpr CIdx kCInvSqrt2 = 5;       // 1/sqrt(2)
inline constexpr CIdx kCMinusInvSqrt2 = 6;  // -1/sqrt(2)
inline constexpr CIdx kCInvSqrt2I = 7;      // i/sqrt(2)
inline constexpr CIdx kCMinusInvSqrt2I = 8; // -i/sqrt(2)

class ComplexTable {
public:
    static constexpr double kDefaultTol = 1e-13;

    explicit ComplexTable(double tol = kDefaultTol);

    CIdx intern(std::complex<double> val);
    std::complex<double> value(CIdx idx) const {
        return values_[idx];
    }

    CIdx mul(CIdx a, CIdx b);
    CIdx add(CIdx a, CIdx b);
    CIdx sub(CIdx a, CIdx b);
    CIdx div(CIdx a, CIdx b);
    CIdx conj(CIdx a);

    double mag_sq(CIdx a) const {
        const auto& v = values_[a];
        return v.real() * v.real() + v.imag() * v.imag();
    }

    double mag(CIdx a) const {
        return std::sqrt(mag_sq(a));
    }

    bool is_zero(CIdx a) const { return a == kC0; }
    bool is_one(CIdx a) const { return a == kC1; }
    bool approx_eq(CIdx a, CIdx b) const;

    std::size_t size() const { return values_.size(); }
    double tolerance() const { return tol_; }

private:
    struct BucketKey {
        std::int64_t x;
        std::int64_t y;

        bool operator==(const BucketKey& other) const {
            return x == other.x && y == other.y;
        }
    };

    struct BucketKeyHash {
        std::size_t operator()(const BucketKey& k) const noexcept {
            // Splitmix64 combine
            std::uint64_t h1 = static_cast<std::uint64_t>(k.x) * 0x9e3779b97f4a7c15ULL;
            std::uint64_t h2 = static_cast<std::uint64_t>(k.y) * 0xbf58476d1ce4e5b9ULL;
            return static_cast<std::size_t>(h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2)));
        }
    };

    double tol_;
    double bucket_step_;
    std::vector<std::complex<double>> values_;
    std::unordered_map<BucketKey, std::vector<CIdx>, BucketKeyHash> buckets_;

    void insert_into_bucket(CIdx idx, const std::complex<double>& v);
};

} // namespace qdd
