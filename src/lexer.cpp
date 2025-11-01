#include "lexer.h"
#include <cctype>
#include <unordered_map>

static std::unordered_map<std::string, TokenType> keywords = {
    {"var", TokenType::VAR},
    {"begin", TokenType::BEGIN},
    {"end", TokenType::END},
    {"read", TokenType::READ},
    {"write", TokenType::WRITE},
    {"if", TokenType::IF},
    {"then", TokenType::THEN},
    {"while", TokenType::WHILE},
    {"do", TokenType::DO}
};

std::vector<Token> tokenize(const std::string& source) {
    std::vector<Token> tokens;
    size_t i = 0;
    int line = 1;

    while (i < source.size()) {
        char c = source[i];

        if (isspace(c)) {
            if (c == '\n') line++;
            i++;
            continue;
        }

        if (isalpha(c)) {
            size_t start = i;
            while (isalnum(source[i])) i++;
            std::string word = source.substr(start, i - start);
            TokenType type = keywords.count(word) ? keywords[word] : TokenType::IDENTIFIER;
            tokens.emplace_back(type, word, line);
        }
        else if (isdigit(c)) {
            size_t start = i;
            while (isdigit(source[i])) i++;
            tokens.emplace_back(TokenType::INTEGER_LITERAL, source.substr(start, i - start), line);
        }
        else {
            switch (c) {
                case ':':
                    if (source[i + 1] == '=') {
                        tokens.emplace_back(TokenType::ASSIGN, ":=", line);
                        i += 2;
                    } else {
                        tokens.emplace_back(TokenType::UNKNOWN, ":", line);
                        i++;
                    }
                    break;
                case '+': tokens.emplace_back(TokenType::PLUS, "+", line); i++; break;
                case '-': tokens.emplace_back(TokenType::MINUS, "-", line); i++; break;
                case '*': tokens.emplace_back(TokenType::MULT, "*", line); i++; break;
                case '/': tokens.emplace_back(TokenType::DIV, "/", line); i++; break;
                case '(': tokens.emplace_back(TokenType::LPAREN, "(", line); i++; break;
                case ')': tokens.emplace_back(TokenType::RPAREN, ")", line); i++; break;
                case ';': tokens.emplace_back(TokenType::SEMICOLON, ";", line); i++; break;
                case '=': tokens.emplace_back(TokenType::EQUAL, "=", line); i++; break;
                case '<': tokens.emplace_back(TokenType::LT, "<", line); i++; break;
                case '>': tokens.emplace_back(TokenType::GT, ">", line); i++; break;
                default:
                    tokens.emplace_back(TokenType::UNKNOWN, std::string(1, c), line);
                    i++;
            }
        }
    }

    tokens.emplace_back(TokenType::END_OF_FILE, "", line);
    return tokens;
}
