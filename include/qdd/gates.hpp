#pragma once

#include "qdd/dd.hpp"

#include <numbers>

namespace qdd {

namespace gates {

// Standard 2x2 unitary matrices
Mat2 I();
Mat2 X();
Mat2 Y();
Mat2 Z();
Mat2 H();
Mat2 S();
Mat2 Sdg();
Mat2 T();
Mat2 Tdg();
Mat2 SX();
Mat2 RX(double theta);
Mat2 RY(double theta);
Mat2 RZ(double theta);
Mat2 P(double lambda);
Mat2 U1(double lambda);
Mat2 U2(double phi, double lambda);
Mat2 U3(double theta, double phi, double lambda);

} // namespace gates

} // namespace qdd
