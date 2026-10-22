#include "qasm_lexer.hpp"
#include "qdd/circuit.hpp"

#include <fstream>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace qdd {

class Parser {
public:
    explicit Parser(std::string_view src) : lexer_(src) {
        advance();
    }

    Circuit parse() {
        Circuit circuit;

        while (curr_.type != TokenType::Eof) {
            if (curr_.type == TokenType::Semicolon) {
                advance();
                continue;
            }

            if (curr_.type != TokenType::Identifier) {
                throw std::runtime_error("Expected identifier at line " + std::to_string(curr_.line));
            }

            std::string id = curr_.text;
            advance();

            if (id == "OPENQASM") {
                consume(TokenType::Number, "Expected OpenQASM version number");
                consume(TokenType::Semicolon, "Expected ';' after OPENQASM statement");
            } else if (id == "include") {
                consume(TokenType::String, "Expected string after include");
                consume(TokenType::Semicolon, "Expected ';' after include statement");
            } else if (id == "qreg") {
                std::string reg_name = curr_.text;
                consume(TokenType::Identifier, "Expected qreg name");
                consume(TokenType::LBracket, "Expected '[' after qreg name");
                auto size = static_cast<std::size_t>(curr_.number_value);
                consume(TokenType::Number, "Expected qreg size");
                consume(TokenType::RBracket, "Expected ']' after qreg size");
                consume(TokenType::Semicolon, "Expected ';' after qreg declaration");

                circuit.qregs.emplace_back(reg_name, size);
                circuit.nqubits += size;
            } else if (id == "creg") {
                std::string reg_name = curr_.text;
                consume(TokenType::Identifier, "Expected creg name");
                consume(TokenType::LBracket, "Expected '[' after creg name");
                auto size = static_cast<std::size_t>(curr_.number_value);
                consume(TokenType::Number, "Expected creg size");
                consume(TokenType::RBracket, "Expected ']' after creg size");
                consume(TokenType::Semicolon, "Expected ';' after creg declaration");

                circuit.cregs.emplace_back(reg_name, size);
            } else if (id == "barrier") {
                // Ignore barrier until semicolon
                while (curr_.type != TokenType::Semicolon && curr_.type != TokenType::Eof) {
                    advance();
                }
                consume(TokenType::Semicolon, "Expected ';' after barrier");
            } else if (id == "gate") {
                // Skip composite gate definition: gate name args... { body... }
                while (curr_.type != TokenType::LBrace && curr_.type != TokenType::Eof) {
                    advance();
                }
                if (curr_.type == TokenType::LBrace) {
                    advance();
                    int depth = 1;
                    while (depth > 0 && curr_.type != TokenType::Eof) {
                        if (curr_.type == TokenType::LBrace) depth++;
                        else if (curr_.type == TokenType::RBrace) depth--;
                        advance();
                    }
                }
            } else if (id == "measure") {
                auto q = parse_arg(circuit);
                consume(TokenType::Arrow, "Expected '->' in measure statement");
                // consume target classical register
                if (curr_.type == TokenType::Identifier) {
                    advance();
                    if (curr_.type == TokenType::LBracket) {
                        advance();
                        consume(TokenType::Number, "Expected index in creg");
                        consume(TokenType::RBracket, "Expected ']' in creg");
                    }
                }
                consume(TokenType::Semicolon, "Expected ';' after measure statement");
                GateOp op;
                op.type = GateType::Measure;
                op.targets = {q};
                circuit.ops.push_back(op);
            } else {
                // Gate application
                std::vector<double> params;
                if (curr_.type == TokenType::LParen) {
                    advance();
                    while (curr_.type != TokenType::RParen && curr_.type != TokenType::Eof) {
                        params.push_back(parse_expr());
                        if (curr_.type == TokenType::Comma) {
                            advance();
                        } else {
                            break;
                        }
                    }
                    consume(TokenType::RParen, "Expected ')' after gate parameters");
                }

                std::vector<std::uint16_t> args;
                while (curr_.type != TokenType::Semicolon && curr_.type != TokenType::Eof) {
                    args.push_back(parse_arg(circuit));
                    if (curr_.type == TokenType::Comma) {
                        advance();
                    } else {
                        break;
                    }
                }
                consume(TokenType::Semicolon, "Expected ';' after gate application");

                GateOp op = make_gate_op(id, params, args);
                circuit.ops.push_back(op);
            }
        }

        return circuit;
    }

private:
    Lexer lexer_;
    Token curr_;

    void advance() {
        curr_ = lexer_.next_token();
    }

    void consume(TokenType type, const std::string& err) {
        if (curr_.type != type) {
            throw std::runtime_error(err + " at line " + std::to_string(curr_.line) +
                                     ", got '" + curr_.text + "'");
        }
        advance();
    }

    std::uint16_t parse_arg(const Circuit& circuit) {
        std::string reg = curr_.text;
        consume(TokenType::Identifier, "Expected register identifier");
        consume(TokenType::LBracket, "Expected '[' in register access");
        auto idx = static_cast<std::size_t>(curr_.number_value);
        consume(TokenType::Number, "Expected index in register access");
        consume(TokenType::RBracket, "Expected ']' in register access");
        return circuit.get_qubit_index(reg, idx);
    }

    double parse_expr() {
        return parse_additive();
    }

    double parse_additive() {
        double left = parse_multiplicative();
        while (curr_.type == TokenType::Plus || curr_.type == TokenType::Minus) {
            TokenType op = curr_.type;
            advance();
            double right = parse_multiplicative();
            if (op == TokenType::Plus) left += right;
            else left -= right;
        }
        return left;
    }

    double parse_multiplicative() {
        double left = parse_unary();
        while (curr_.type == TokenType::Star || curr_.type == TokenType::Slash) {
            TokenType op = curr_.type;
            advance();
            double right = parse_unary();
            if (op == TokenType::Star) left *= right;
            else left /= right;
        }
        return left;
    }

    double parse_unary() {
        if (curr_.type == TokenType::Minus) {
            advance();
            return -parse_unary();
        }
        if (curr_.type == TokenType::Plus) {
            advance();
            return parse_unary();
        }
        return parse_primary();
    }

    double parse_primary() {
        if (curr_.type == TokenType::Number) {
            double v = curr_.number_value;
            advance();
            return v;
        }
        if (curr_.type == TokenType::Identifier) {
            if (curr_.text == "pi") {
                advance();
                return std::numbers::pi;
            }
            throw std::runtime_error("Unknown identifier in math expression: " + curr_.text);
        }
        if (curr_.type == TokenType::LParen) {
            advance();
            double val = parse_expr();
            consume(TokenType::RParen, "Expected ')'");
            return val;
        }
        throw std::runtime_error("Unexpected token in math expression: " + curr_.text);
    }

    GateOp make_gate_op(const std::string& name, const std::vector<double>& params,
                        const std::vector<std::uint16_t>& args) {
        GateOp op;
        op.params = params;

        if (name == "x") {
            op.type = GateType::X; op.targets = {args[0]};
        } else if (name == "y") {
            op.type = GateType::Y; op.targets = {args[0]};
        } else if (name == "z") {
            op.type = GateType::Z; op.targets = {args[0]};
        } else if (name == "h") {
            op.type = GateType::H; op.targets = {args[0]};
        } else if (name == "s") {
            op.type = GateType::S; op.targets = {args[0]};
        } else if (name == "sdg") {
            op.type = GateType::Sdg; op.targets = {args[0]};
        } else if (name == "t") {
            op.type = GateType::T; op.targets = {args[0]};
        } else if (name == "tdg") {
            op.type = GateType::Tdg; op.targets = {args[0]};
        } else if (name == "sx") {
            op.type = GateType::SX; op.targets = {args[0]};
        } else if (name == "rx") {
            op.type = GateType::RX; op.targets = {args[0]};
        } else if (name == "ry") {
            op.type = GateType::RY; op.targets = {args[0]};
        } else if (name == "rz") {
            op.type = GateType::RZ; op.targets = {args[0]};
        } else if (name == "p" || name == "u1") {
            op.type = GateType::P; op.targets = {args[0]};
        } else if (name == "u2") {
            op.type = GateType::U2; op.targets = {args[0]};
        } else if (name == "u3" || name == "u") {
            op.type = GateType::U3; op.targets = {args[0]};
        } else if (name == "cx") {
            op.type = GateType::CX;
            op.controls = {Control{args[0], true}};
            op.targets = {args[1]};
        } else if (name == "cy") {
            op.type = GateType::CY;
            op.controls = {Control{args[0], true}};
            op.targets = {args[1]};
        } else if (name == "cz") {
            op.type = GateType::CZ;
            op.controls = {Control{args[0], true}};
            op.targets = {args[1]};
        } else if (name == "ch") {
            op.type = GateType::CH;
            op.controls = {Control{args[0], true}};
            op.targets = {args[1]};
        } else if (name == "cp" || name == "cu1") {
            op.type = GateType::CP;
            op.controls = {Control{args[0], true}};
            op.targets = {args[1]};
        } else if (name == "crx") {
            op.type = GateType::CRX;
            op.controls = {Control{args[0], true}};
            op.targets = {args[1]};
        } else if (name == "cry") {
            op.type = GateType::CRY;
            op.controls = {Control{args[0], true}};
            op.targets = {args[1]};
        } else if (name == "crz") {
            op.type = GateType::CRZ;
            op.controls = {Control{args[0], true}};
            op.targets = {args[1]};
        } else if (name == "swap") {
            op.type = GateType::SWAP;
            op.targets = {args[0], args[1]};
        } else if (name == "ccx") {
            op.type = GateType::CCX;
            op.controls = {Control{args[0], true}, Control{args[1], true}};
            op.targets = {args[2]};
        } else if (name == "cswap") {
            op.type = GateType::CSWAP;
            op.controls = {Control{args[0], true}};
            op.targets = {args[1], args[2]};
        } else if (name == "mcx") {
            op.type = GateType::MCX;
            for (std::size_t i = 0; i < args.size() - 1; ++i) {
                op.controls.push_back(Control{args[i], true});
            }
            op.targets = {args.back()};
        } else {
            throw std::runtime_error("Unsupported gate in OpenQASM: " + name);
        }

        return op;
    }
};

Circuit parse_qasm_string(const std::string& source) {
    Parser parser(source);
    return parser.parse();
}

Circuit parse_qasm_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_qasm_string(buffer.str());
}

} // namespace qdd
