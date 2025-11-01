#include "lexer.h"
#include <iostream>

int main() {
    std::string code = "var x : integer; begin x := 5; write(x); end";
    auto tokens = tokenize(code);

    for (const auto& token : tokens) {
        std::cout << "Line " << token.line << ": " << token.lexeme << std::endl;
    }

    return 0;
}
