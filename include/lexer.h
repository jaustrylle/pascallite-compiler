#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>

enum class TokenType {
    VAR, BEGIN, END, READ, WRITE,
    IF, THEN, WHILE, DO,
    IDENTIFIER, INTEGER_LITERAL,
    ASSIGN, PLUS, MINUS, MULT, DIV,
    LPAREN, RPAREN, SEMICOLON,
    EQUAL, LT, GT,
    END_OF_FILE,
    UNKNOWN
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;

    Token(TokenType t, const std::string& l, int ln)
        : type(t), lexeme(l), line(ln) {}
};

std::vector<Token> tokenize(const std::string& source);

#endif
