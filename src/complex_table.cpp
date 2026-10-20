#include "qdd/complex_table.hpp"

#include <numbers>

namespace qdd {

ComplexTable::ComplexTable(double tol)
    : tol_(tol), bucket_step_(2.0 * tol) {
    values_.reserve(1024);
    // Pre-seed constants
    const double inv_sqrt2 = 1.0 / std::sqrt(2.0);

    values_.push_back(std::complex<double>(0.0, 0.0));           // kC0 = 0
    values_.push_back(std::complex<double>(1.0, 0.0));           // kC1 = 1
    values_.push_back(std::complex<double>(-1.0, 0.0));          // kCMinus1 = 2
    values_.push_back(std::complex<double>(0.0, 1.0));           // kCI = 3
    values_.push_back(std::complex<double>(0.0, -1.0));          // kCMinusI = 4
    values_.push_back(std::complex<double>(inv_sqrt2, 0.0));     // kCInvSqrt2 = 5
    values_.push_back(std::complex<double>(-inv_sqrt2, 0.0));    // kCMinusInvSqrt2 = 6
    values_.push_back(std::complex<double>(0.0, inv_sqrt2));     // kCInvSqrt2I = 7
    values_.push_back(std::complex<double>(0.0, -inv_sqrt2));    // kCMinusInvSqrt2I = 8

    for (CIdx i = 0; i < values_.size(); ++i) {
        insert_into_bucket(i, values_[i]);
    }
}

void ComplexTable::insert_into_bucket(CIdx idx, const std::complex<double>& v) {
    auto kx = static_cast<std::int64_t>(std::floor(v.real() / bucket_step_));
    auto ky = static_cast<std::int64_t>(std::floor(v.imag() / bucket_step_));
    buckets_[BucketKey{kx, ky}].push_back(idx);
}

CIdx ComplexTable::intern(std::complex<double> val) {
    // Fast checks for common values
    double r = val.real();
    double im = val.imag();

    if (std::abs(r) <= tol_ && std::abs(im) <= tol_) {
        return kC0;
    }
    if (std::abs(r - 1.0) <= tol_ && std::abs(im) <= tol_) {
        return kC1;
    }
    if (std::abs(r + 1.0) <= tol_ && std::abs(im) <= tol_) {
        return kCMinus1;
    }
    if (std::abs(r) <= tol_ && std::abs(im - 1.0) <= tol_) {
        return kCI;
    }
    if (std::abs(r) <= tol_ && std::abs(im + 1.0) <= tol_) {
        return kCMinusI;
    }

    auto kx = static_cast<std::int64_t>(std::floor(r / bucket_step_));
    auto ky = static_cast<std::int64_t>(std::floor(im / bucket_step_));

    for (std::int64_t dx = -1; dx <= 1; ++dx) {
        for (std::int64_t dy = -1; dy <= 1; ++dy) {
            auto it = buckets_.find(BucketKey{kx + dx, ky + dy});
            if (it != buckets_.end()) {
                for (CIdx cand_idx : it->second) {
                    const auto& cand = values_[cand_idx];
                    if (std::abs(cand.real() - r) <= tol_ &&
                        std::abs(cand.imag() - im) <= tol_) {
                        return cand_idx;
                    }
                }
            }
        }
    }

    auto new_idx = static_cast<CIdx>(values_.size());
    values_.push_back(val);
    insert_into_bucket(new_idx, val);
    return new_idx;
}

CIdx ComplexTable::mul(CIdx a, CIdx b) {
    if (a == kC0 || b == kC0) return kC0;
    if (a == kC1) return b;
    if (b == kC1) return a;
    if (a == kCMinus1) return intern(-values_[b]);
    if (b == kCMinus1) return intern(-values_[a]);
    return intern(values_[a] * values_[b]);
}

CIdx ComplexTable::add(CIdx a, CIdx b) {
    if (a == kC0) return b;
    if (b == kC0) return a;
    return intern(values_[a] + values_[b]);
}

CIdx ComplexTable::sub(CIdx a, CIdx b) {
    if (b == kC0) return a;
    if (a == kC0) return mul(b, kCMinus1);
    return intern(values_[a] - values_[b]);
}

CIdx ComplexTable::div(CIdx a, CIdx b) {
    if (a == kC0) return kC0;
    if (b == kC1) return a;
    if (b == kCMinus1) return mul(a, kCMinus1);
    return intern(values_[a] / values_[b]);
}

CIdx ComplexTable::conj(CIdx a) {
    if (a <= kCMinus1 || a == kCInvSqrt2 || a == kCMinusInvSqrt2) {
        return a;
    }
    if (a == kCI) return kCMinusI;
    if (a == kCMinusI) return kCI;
    if (a == kCInvSqrt2I) return kCMinusInvSqrt2I;
    if (a == kCMinusInvSqrt2I) return kCInvSqrt2I;
    return intern(std::conj(values_[a]));
}

bool ComplexTable::approx_eq(CIdx a, CIdx b) const {
    if (a == b) return true;
    const auto& va = values_[a];
    const auto& vb = values_[b];
    return std::abs(va.real() - vb.real()) <= tol_ &&
           std::abs(va.imag() - vb.imag()) <= tol_;
}

} // namespace qdd
