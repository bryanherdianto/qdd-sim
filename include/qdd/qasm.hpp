#pragma once

#include "qdd/circuit.hpp"
#include <string>

namespace qdd {

Circuit parse_qasm_file(const std::string& filepath);
Circuit parse_qasm_string(const std::string& source);

} // namespace qdd
