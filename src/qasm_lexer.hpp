#pragma once

#include "qdd/qasm.hpp"

#include <cctype>
#include <string>
#include <string_view>
#include <stdexcept>

namespace qdd {

enum class TokenType {
    Identifier,
    Number,
    String,
    Semicolon,
    Comma,
    LBracket,
    RBracket,
    LParen,
    RParen,
    Plus,
    Minus,
    Star,
    Slash,
    LBrace,
    RBrace,
    Arrow,
    Eof
};

struct Token {
    TokenType type = TokenType::Eof;
    std::string text;
    double number_value = 0.0;
    std::size_t line = 1;
    std::size_t col = 1;
};

class Lexer {
public:
    explicit Lexer(std::string_view src) : src_(src) {
        if (src_.size() >= 3 &&
            static_cast<unsigned char>(src_[0]) == 0xEF &&
            static_cast<unsigned char>(src_[1]) == 0xBB &&
            static_cast<unsigned char>(src_[2]) == 0xBF) {
            pos_ = 3;
        }
    }

    Token next_token() {
        skip_whitespace_and_comments();
        if (pos_ >= src_.size()) {
            return Token{TokenType::Eof, "", 0.0, line_, col_};
        }

        std::size_t tok_line = line_;
        std::size_t tok_col = col_;
        char c = src_[pos_];

        if (c == ';') { advance(); return Token{TokenType::Semicolon, ";", 0.0, tok_line, tok_col}; }
        if (c == ',') { advance(); return Token{TokenType::Comma, ",", 0.0, tok_line, tok_col}; }
        if (c == '[') { advance(); return Token{TokenType::LBracket, "[", 0.0, tok_line, tok_col}; }
        if (c == ']') { advance(); return Token{TokenType::RBracket, "]", 0.0, tok_line, tok_col}; }
        if (c == '(') { advance(); return Token{TokenType::LParen, "(", 0.0, tok_line, tok_col}; }
        if (c == ')') { advance(); return Token{TokenType::RParen, ")", 0.0, tok_line, tok_col}; }
        if (c == '+') { advance(); return Token{TokenType::Plus, "+", 0.0, tok_line, tok_col}; }
        if (c == '*') { advance(); return Token{TokenType::Star, "*", 0.0, tok_line, tok_col}; }
        if (c == '/') { advance(); return Token{TokenType::Slash, "/", 0.0, tok_line, tok_col}; }
        if (c == '{') { advance(); return Token{TokenType::LBrace, "{", 0.0, tok_line, tok_col}; }
        if (c == '}') { advance(); return Token{TokenType::RBrace, "}", 0.0, tok_line, tok_col}; }

        if (c == '-') {
            advance();
            if (pos_ < src_.size() && src_[pos_] == '>') {
                advance();
                return Token{TokenType::Arrow, "->", 0.0, tok_line, tok_col};
            }
            return Token{TokenType::Minus, "-", 0.0, tok_line, tok_col};
        }

        if (c == '"') {
            advance();
            std::size_t start = pos_;
            while (pos_ < src_.size() && src_[pos_] != '"') {
                advance();
            }
            std::string text(src_.substr(start, pos_ - start));
            if (pos_ < src_.size() && src_[pos_] == '"') {
                advance();
            }
            return Token{TokenType::String, text, 0.0, tok_line, tok_col};
        }

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::size_t start = pos_;
            while (pos_ < src_.size() && (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
                advance();
            }
            std::string text(src_.substr(start, pos_ - start));
            return Token{TokenType::Identifier, text, 0.0, tok_line, tok_col};
        }

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            std::size_t start = pos_;
            bool has_dot = (c == '.');
            advance();
            while (pos_ < src_.size()) {
                char ch = src_[pos_];
                if (std::isdigit(static_cast<unsigned char>(ch))) {
                    advance();
                } else if (ch == '.' && !has_dot) {
                    has_dot = true;
                    advance();
                } else if (ch == 'e' || ch == 'E') {
                    advance();
                    if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) {
                        advance();
                    }
                } else {
                    break;
                }
            }
            std::string text(src_.substr(start, pos_ - start));
            double val = std::stod(text);
            return Token{TokenType::Number, text, val, tok_line, tok_col};
        }

        throw std::runtime_error("Unexpected character '" + std::string(1, c) + "' at line " +
                                 std::to_string(tok_line) + ":" + std::to_string(tok_col));
    }

private:
    std::string_view src_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
    std::size_t col_ = 1;

    void advance() {
        if (pos_ < src_.size()) {
            if (src_[pos_] == '\n') {
                line_++;
                col_ = 1;
            } else {
                col_++;
            }
            pos_++;
        }
    }

    void skip_whitespace_and_comments() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (std::isspace(static_cast<unsigned char>(c))) {
                advance();
            } else if (c == '/' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '/') {
                while (pos_ < src_.size() && src_[pos_] != '\n') {
                    advance();
                }
            } else {
                break;
            }
        }
    }
};

} // namespace qdd
