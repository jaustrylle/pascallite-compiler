// Serena Reese and Amiran Fields - CS 4301 - Stage 1

/*
stage1.cpp
- Completes the implementation of Stage 1 for the CS4301 Pascallite compiler project
- Follows the header and pseudocode supplied in our course's materials
- Uses registers A (eax) and D (edx)
- D serves for mod division remainders
- Each register assigned at most one operand at a time
*/

#include <stage1.h> // iostream, fstream, string, map, namespace, SymbolTable

#include <iomanip>
#include <cctype>
#include <sstream>
#include <cstdlib>      // for exit

#include <set>
#include <vector>
#include <chrono>       // for time
#include <ctime>
#include <algorithm>    // for std::find_if, std::remove_if, std::isspace

/////////////////////////////////////////////////////////////////////////////

// --- Global State Definitions ---
// Missing private members in Compiler class
static std::set<std::string> keywords;
static std::set<char> specialSymbols;
static uint I_count = 0;
static uint B_count = 0;
static bool begChar = true;

// String rep of END_OF_FILE char
const std::string END_FILE_TOKEN = std::string(1, END_OF_FILE);

/////////////////////////////////////////////////////////////////////////////
// --- Global Helper Function Implementations ---

std::string getTime() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char buf[64];
    // Use std::strftime to format the local time
    if (std::strftime(buf, sizeof(buf), "%c", std::localtime(&now))) {
        return std::string(buf);
    }
    return std::string();
}

static inline std::string trim(const std::string &s) {
    auto front = std::find_if_not(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); });
    auto back = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char c){ return std::isspace(c); }).base();
    if (front >= back) return std::string();
    return std::string(front, back);
}

std::vector<std::string> splitNames(std::string names) {
    std::vector<std::string> result;
    std::stringstream ss(names);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // Trim whitespace
        std::string t = trim(item);
        if (!t.empty()) result.push_back(t);
    }
    return result;
}

// Global version for use in whichType/whichValue
bool isBooleanLiteral(std::string s) {
    return s == "true" || s == "false";
}

// Global version for use in whichType/whichValue
bool isIntegerLiteral(std::string s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') {
        i = 1;
        if (s.size() == 1) return false; // only sign, no digits
    }
    bool hasDigit = false;
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        hasDigit = true;
    }
    return hasDigit;
}

/////////////////////////////////////////////////////////////////////////////

/* ------------------------------------------------------
    Compiler class declared in stage1.h, now define its funcs
    ------------------------------------------------------ */

Compiler::Compiler(char **argv){        // constructor
    // Initialize runtime state before opening files
    token = "";
    ch = ' ';
    errorCount = 0;
    lineNo = 1;
    currentTempNo = -1;
    maxTempNo = -1;
    contentsOfAReg = "";

    // Open files (argv indices assumed valid by main)
    sourceFile.open(argv[1]);
    listingFile.open(argv[2]);
    objectFile.open(argv[3]);

    // Initialize static global sets
    keywords = {"program", "const", "var", "begin", "end", "integer", "boolean", "true", "false", "not", "read", "write"};
    // single-character special symbols; lexer must still handle multi-char tokens like ":=" or "<="
    specialSymbols = {':', ',', ';', '=', '+', '-', '.', '(', ')', '{', '}', '*', '/', '%', '<', '>'};

    // Check file openings and report errors
    if (!sourceFile.is_open()) {
        processError(std::string("Unable to open source file: ") + argv[1]);
    }
    if (!listingFile.is_open()) {
        processError(std::string("Unable to open listing file: ") + argv[2]);
    }
    if (!objectFile.is_open()) {
        processError(std::string("Unable to open object file: ") + argv[3]);
    }
}

Compiler::~Compiler(){  // destructor
    if (sourceFile.is_open()) sourceFile.close();
    if (listingFile.is_open()) listingFile.close();
    if (objectFile.is_open()) objectFile.close();
}

void Compiler::createListingHeader(){
    std::string timeStr = getTime();

    // Listing header output to listingFile, not console
    listingFile << "STAGE1:\tSERENA REESE, AMIRAN FIELDS\t\t" << timeStr << "\n\n";

    listingFile << std::left << "LINE NO."
                << std::setw(23 - std::string("LINE NO.").length()) << " "
                << "SOURCE STATEMENT\n";

    listingFile << "\n";
    lineNo = 1;
}

void Compiler::parser(){
    // Ensure ch is initialized to first character of source file
    ch = nextChar(); // nextChar is expected to be implemented elsewhere
    token = nextToken(); // prime the first token

    if(token != "program"){
        processError("keyword \"program\" expected");
        // attempt to continue parsing anyway
    }

    prog();     // parser implements grammar rules, calling 1st rule
}

void Compiler::createListingTrailer() {
    std::string errorWord = (errorCount == 1) ? "ERROR" : "ERRORS";

    // Output to console
    std::cout << "COMPILATION TERMINATED\t\t"
              << errorCount << " " << errorWord << " ENCOUNTERED"
              << std::endl;

    // Output to listing file
    if (listingFile.is_open()) {
        listingFile << "\n" << "COMPILATION TERMINATED\t\t"
                    << errorCount << " " << errorWord << " ENCOUNTERED"
                    << std::endl;
    }
}

/* ------------------------------------------------------
    Methods implementing grammar prods
    ------------------------------------------------------ */

void Compiler::prog(){  // stage0, prod 1
    // Expect token to be "program" on entry
    if (token != "program") {
        processError("keyword \"program\" expected");
        // try to recover: attempt to find program token
    } else {
        token = nextToken(); // consume "program" and advance to program name
    }

    progStmt();            // parse program header (program name, semicolon)

    // optional const and var blocks (order: const* var*)
    if (token == "const") {
        consts();
    }
    if (token == "var") {
        vars();
    }

    if (token != "begin") {
        processError("keyword \"begin\" expected");
        // Recovery for Stage 1: continue attempting to parse body
    }

    beginEndStmt();        // parse begin ... end .

    // After program, expect EOF token
    if (token != END_FILE_TOKEN) {
        processError("no text may follow \"end\"");
    }
}

void Compiler::progStmt(){      // stage 0, prod 2
    // On entry token should be program name (already consumed "program")
    std::string x;

    if (!isNonKeyId(token)) {
        processError("program name expected");
        // try to recover: skip token
        x = token;
    } else {
        x = token;
    }

    token = nextToken();        // expect semicolon
    if (token != ";") {
        processError("semicolon expected");
        // attempt to continue
    }

    token = nextToken();        // advance to next token after semicolon
    // Insert program name into symbol table
    insert(x, PROG_NAME, CONSTANT, x, NO, 0);
    code("program", x);
}

void Compiler::consts(){        // stage0, prod 3
    // token is "const" on entry
    token = nextToken();        // advance to first identifier (or error)
    if (!isNonKeyId(token)) {
        processError("non-keyword identifier must follow \"const\"");
        // attempt to continue: return to caller
        return;
    }

    // Parse one or more const declarations
    while (isNonKeyId(token)) {
        constStmts();
    }
}

void Compiler::vars(){  // stage 0, prod 4
    // token is "var" on entry
    token = nextToken();        // advance to first identifier (or error)
    if (!isNonKeyId(token)) {
        processError("non-keyword identifier must follow \"var\"");
        // attempt to continue
        return;
    }

    // Parse one or more var declarations
    while (isNonKeyId(token)) {
        varStmts();
    }
}

void Compiler::beginEndStmt(){  // stage 1, prod 5
    // token is "begin" on entry
    token = nextToken();        // move to first token inside the block

    // Actually parse the executable statements now
    execStmts();

    // When execStmts returns, token should be "end" (or we complain)
    if (token != "end") {
        processError("keyword \"end\" expected");
    } else {
        token = nextToken();    // consume "end"
    }

    if (token != ".") {
        processError("period expected");
    } else {
        token = nextToken();    // consume '.' and advance (should be EOF)
    }

    code("end", ".");           // emit epilogue + storage
}

void Compiler::constStmts(){    // stage 0, prod 6
    // On entry token is identifier for the const declaration
    std::string x, y;
    storeTypes type = UNKNOWN;
    std::string val; // actual value to store

    if (!isNonKeyId(token)) {
        processError("non-keyword identifier expected");
        // try to recover
        token = nextToken();
        return;
    }

    x = token;                 // identifier name
    token = nextToken();       // should be '='
    if (token != "=") {
        processError("\"=\" expected");
        // attempt to continue
    }

    token = nextToken();       // token on the right of '='
    y = token;

    // 1. Check for unary operator (+, -, not)
    if (y == "+" || y == "-") {
        // Case 1: Signed Integer
        std::string sign = y;
        token = nextToken();
        if (!isInteger(token)) {
            processError("integer expected after sign");
            // attempt to continue
        } else {
            val = sign + token; // e.g., "-1"
            type = INTEGER;
        }
        token = nextToken(); // advance past the integer literal
    }
    else if (y == "not") {
        // Case 2: NOT Boolean
        token = nextToken();
        if (!isBoolean(token)) {
            processError("boolean expected after \"not\"");
        } else {
            val = (token == "true") ? "false" : "true"; // Flip the value
            type = BOOLEAN;
        }
        token = nextToken(); // advance past the boolean literal
    }
    else {
        // Case 3: Simple Literal (0, true) OR Non-Key-Id (existing const)
        if (isInteger(y) || isBoolean(y)) {
            // Subcase 3a: Literal (e.g., "0", "true")
            type = isInteger(y) ? INTEGER : BOOLEAN;
            val = y;
            token = nextToken(); // advance past literal
        } else if (isNonKeyId(y)) {
            // Subcase 3b: Existing Constant Name (e.g., "big")
            type = whichType(y);
            val = whichValue(y);
            token = nextToken(); // advance past identifier
        } else {
            processError("token to right of \"=\" illegal");
            token = nextToken(); // try to continue
        }
    }

    // 4. Expect and process semicolon
    if (token != ";") {
        processError("semicolon expected");
        // attempt to continue
    } else {
        token = nextToken(); // consume ';' and advance
    }

    // 5. Final Type check
    if (type != INTEGER && type != BOOLEAN) {
        processError("data type of token on the right-hand side must be INTEGER or BOOLEAN");
    }

    // 6. Insert into Symbol Table as a constant
    insert(x, type, CONSTANT, val, YES, 1);

    // 7. If next token is another identifier, continue parsing consts (handled by caller loop)
    // (caller of consts() loops while isNonKeyId(token))
}

void Compiler::varStmts(){      // stage 0, prod 7
    // On entry token is identifier (first in a comma-separated list)
    if (!isNonKeyId(token)) {
        processError("non-keyword identifier expected");
        token = nextToken();
        return;
    }

    // Parse identifier list and leave token at the token after the list
    std::string idlist = ids(); // ids() will advance token appropriately

    // Expect colon
    if (token != ":") {
        processError("\":\" expected");
        // attempt to continue
    } else {
        token = nextToken(); // consume ':' and advance to type
    }

    if (token != "integer" && token != "boolean") {
        processError("illegal type follows \":\"");
    }

    storeTypes varType = (token == "integer") ? INTEGER : BOOLEAN;

    token = nextToken(); // advance past type
    if (token != ";") {
        processError("semicolon expected");
    } else {
        token = nextToken(); // consume ';' and advance
    }

    // Insert each identifier from the comma-separated list into the symbol table
    std::vector<std::string> names = splitNames(idlist);
    for (const auto &name : names) {
        if (!name.empty()) {
            insert(name, varType, VARIABLE, "", YES, 1);
        }
    }

    // If next token is another identifier, caller loop will continue parsing varStmts
}

string Compiler::ids(){         // stage 0, prod 8
    // On entry token is an identifier
    if (!isNonKeyId(token)) {
        processError("non-keyword identifier expected");
        // try to recover: return empty and advance
        std::string bad = token;
        token = nextToken();
        return bad;
    }

    std::string tempString = token; // first identifier
    token = nextToken();            // advance to next token after identifier

    if (token == ",") {
        token = nextToken();        // advance past comma to next identifier
        if (!isNonKeyId(token)) {
            processError("non-keyword identifier expected");
        } else {
            // recursive call returns the rest of the list (as comma-separated string)
            std::string rest = ids();
            tempString = tempString + "," + rest;
        }
    }

    // When returning, token is left at the token after the identifier list (either ":" or other)
    return tempString;
}

//////////////////// EXPANDED IN STAGE 1

void Compiler::execStmts(){     // stage 1, prod 2
    // Parse zero or more executable statements until 'end' or EOF or '.'
    while (true) {
        // Stop if we reached end of block
        if (token == "end" || (token.size() == 1 && token[0] == END_OF_FILE) || token == ".") break;

        // Parse a single statement
        execStmt();

        // Statements in Pascal are typically separated by semicolons.
        if (token == ";") {
            token = nextToken(); // consume semicolon and continue
            continue;
        }

        // If next token begins another statement, continue; otherwise break
        if (token == "end" || token == "." || (token.size() == 1 && token[0] == END_OF_FILE)) break;

        // If token looks like start of another statement, continue loop
        if (isNonKeyId(token) || token == "read" || token == "write") {
            continue;
        }

        // Unexpected token: try to recover by advancing
        processError("unexpected token in statement list");
        token = nextToken();
    }
}

void Compiler::execStmt(){      // stage 1, prod 3
    // Decide which kind of statement based on current token
    if (isNonKeyId(token)) {
        assignStmt();
    }
    else if (token == "read") {
        readStmt();
    }
    else if (token == "write") {
        writeStmt();
    }
    else {
        processError("executable statement expected");
        // Attempt to skip token and continue
        token = nextToken();
    }
}

void Compiler::assignStmt(){    // stage 1, prod 4
    // Syntax: <id> := <expression>
    std::string lhs = token;
    if (!isNonKeyId(lhs)) {
        processError("assignment target must be an identifier");
        token = nextToken();
        return;
    }

    token = nextToken(); // consume identifier, advance to ':='
    if (token != ":=") {
        processError("':=' expected in assignment");
        // attempt to continue
    } else {
        token = nextToken(); // consume ':=' and advance to expression
    }

    // Parse RHS expression; express() will push result onto operand stack
    express();

    // After express, top of operand stack holds result
    std::string rhs = popOperand();
    if (rhs.empty()) {
        processError("missing expression in assignment");
        return;
    }

    // Emit assignment: lhs := rhs
    // If rhs is a temporary, we can store it directly into lhs
    // If rhs is a literal or variable, emitAssignCode will handle it
    emitAssignCode(rhs, lhs);

    // If rhs was a temporary, free it now (value moved to lhs)
    if (isTemporary(rhs)) freeTemp();

    // token is left at the token after the expression (express() leaves it there)
}

void Compiler::readStmt(){      // stage 1, prod 5
    // Syntax: read ( id {, id} )
    token = nextToken(); // consume 'read' and advance to '(' or identifier

    if (token != "(") {
        processError("'(' expected after read");
        // attempt to continue
    } else {
        token = nextToken(); // consume '('
    }

    // Read one or more identifiers separated by commas
    while (true) {
        if (!isNonKeyId(token)) {
            processError("identifier expected in read");
            // try to recover
            token = nextToken();
            if (token == ")") break;
        } else {
            // Emit read code for this identifier
            emitReadCode(token);
            token = nextToken(); // consume identifier
        }

        if (token == ",") {
            token = nextToken(); // consume comma and continue
            continue;
        }
        break;
    }

    if (token != ")") {
        processError("')' expected after read list");
    } else {
        token = nextToken(); // consume ')'
    }
}

void Compiler::writeStmt(){     // stage 1, prod 7
    // Syntax: write ( <expression> {, <expression>} )
    token = nextToken(); // consume 'write' and advance to '('

    if (token != "(") {
        processError("'(' expected after write");
        // attempt to continue
    } else {
        token = nextToken(); // consume '('
    }

    // One or more expressions separated by commas
    while (true) {
        // Parse expression and emit write for its result
        express();
        std::string val = popOperand();
        if (val.empty()) {
            processError("missing expression in write");
        } else {
            emitWriteCode(val);
            if (isTemporary(val)) freeTemp();
        }

        // token is at next token after expression
        if (token == ",") {
            token = nextToken(); // consume comma and continue
            continue;
        }
        break;
    }

    if (token != ")") {
        processError("')' expected after write list");
    } else {
        token = nextToken(); // consume ')'
    }
}

void Compiler::express(){       // stage 1, prod 9
    // express -> term expresses
    term();
    expresses();
    // After reduction, top of operand stack holds the expression result
}

void Compiler::expresses(){      // stage 1, prod 10
    // handles additive and logical-or operators: +, -, or
    while (token == "+" || token == "-" || token == "or" || token == "||") {
        std::string op = token;
        token = nextToken(); // consume operator
        term();              // parse right-hand term

        // Pop operands: right then left
        std::string right = popOperand();
        std::string left  = popOperand();

        if (left.empty() || right.empty()) {
            // ... (error handling) ...
            return;
        }

        // Apply operator: Emitter must calculate 'left op right', leave result in EAX,
        // and crucially, set contentsOfAReg = left.
        if (op == "+") {
            emitAdditionCode(left, right);
        } else if (op == "-") {
            emitSubtractionCode(left, right);
        } else if (op == "or" || op == "||") {
            emitOrCode(left, right);
        } else {
            processError("unknown additive/logical operator: " + op);
        }

        // --- OPTIMIZED STAGE 1 LOGIC START ---
        // 1. The accumulated result is in EAX, tracked by the name 'left'.
        // 2. Free the right operand temporary if necessary.
        if (isTemporary(right)) freeTemp();

        // 3. Push the left operand's name back onto the stack to represent the new result.
        // The value in EAX is now associated with the name 'left'.
        pushOperand(left);
        // --- OPTIMIZED STAGE 1 LOGIC END ---
    }
}

void Compiler::term(){          // stage 1, prod 11
    // term -> factor terms
    factor();
    terms();
}

void Compiler::terms(){          // stage 1, prod 12
    // handles multiplicative and logical-and operators: *, /, %, and
    while (token == "*" || token == "/" || token == "%" || token == "and" || token == "&&") {
        std::string op = token;
        token = nextToken(); // consume operator
        factor();            // parse right-hand factor

        // Pop operands: right then left
        std::string right = popOperand();
        std::string left  = popOperand();

        if (left.empty() || right.empty()) {
            // ... (error handling) ...
            return;
        }

        // Apply operator: Emitter must calculate 'left op right', leave result in EAX,
        // and crucially, set contentsOfAReg = left.
        if (op == "*") {
            emitMultiplicationCode(left, right);
        } else if (op == "/") {
            emitDivisionCode(left, right);
        } else if (op == "%") {
            emitModuloCode(left, right);
        } else if (op == "and" || op == "&&") {
            emitAndCode(left, right);
        } else {
            processError("unknown multiplicative/logical operator: " + op);
        }

        // --- OPTIMIZED STAGE 1 LOGIC START ---
        // 1. The accumulated result is in EAX, tracked by the name 'left'.
        // 2. Free the right operand temporary if necessary.
        if (isTemporary(right)) freeTemp();

        // 3. Push the left operand's name back onto the stack to represent the new result.
        // The value in EAX is now associated with the name 'left'.
        pushOperand(left);
        // --- OPTIMIZED STAGE 1 LOGIC END ---
    }
}

void Compiler::factor(){         // stage 1, prod 13
    // factor -> [ unary-op ] part
    if (token == "+" || token == "-" || token == "not") {
        std::string unary = token;
        token = nextToken(); // consume unary operator
        part();              // parse the operand
        std::string opnd = popOperand();
        if (opnd.empty()) {
            processError("operand expected after unary operator");
            return;
        }

        // --- Special Case: Unary Plus is a no-op ---
        if (unary == "+") {
            // The result is just the operand itself. No code generation needed.
            // Push the operand back onto the stack to be the result of the factor.
            pushOperand(opnd);

        } else {
            // --- Unary Minus and NOT (Code Generating) ---

            // Apply unary op: Emitter calculates the operation, leaves result in EAX, and clears contentsOfAReg.
            if (unary == "-") {
                emitNegationCode(opnd);
            } else if (unary == "not") {
                emitNotCode(opnd);
            } else {
                processError("unknown unary operator: " + unary);
                return;
            }

            // EAX now holds the result. We need to save it to a new temporary.
            std::string dest = getTemp();
            emit("", "mov", "[" + symbolTable.at(dest).getInternalName() + "], eax", "; store expression result into " + dest);

            // Free the temporary used for the operand (which has now been consumed)
            if (isTemporary(opnd)) freeTemp();

            // Update A register tracking and push the result
            contentsOfAReg = dest;
            pushOperand(dest);
        }

    } else {
        // No unary operator; just parse part
        part();
    }

    // After factor, allow further factor-level processing if grammar requires
    factors();
}

void Compiler::factors(){       // stage 1, prod 14
    // This implementation does not define additional postfix operators,
    // so factors is a no-op placeholder to match grammar shape.
    // If you later add exponentiation or other postfix operators, implement here.
    (void)0;
}

void Compiler::part(){          // stage 1, prod 15
    // part -> identifier | literal | ( express )
    if (token == "(") {
        token = nextToken(); // consume '('
        express();
        if (token != ")") {
            processError("')' expected");
        } else {
            token = nextToken(); // consume ')'
        }
        return;
    }

    // Identifier
    if (isNonKeyId(token)) {
        // Push the identifier name as operand (external name used in emit)
        pushOperand(token);
        token = nextToken(); // consume identifier
        return;
    }

    // Literal (integer or boolean)
    if (isLiteral(token) || isInteger(token) || isBoolean(token)) {
        // Push literal token directly; emit routines will accept literals
        pushOperand(token);
        token = nextToken(); // consume literal
        return;
    }

    // Unexpected token
    processError("literal, identifier, or '(' expected");
    token = nextToken(); // try to recover
}

/* ------------------------------------------------------
    Helper funcs for Pascallite lexicon
    ------------------------------------------------------ */

bool Compiler::isKeyword(string s) const {       // is s a keyword?
    return keywords.count(s) > 0;
}

bool Compiler::isSpecialSymbol(char c) const {  // is c a spec symb?
    return specialSymbols.count(c) > 0;
}

bool Compiler::isNonKeyId(string s) const {      // is s a non_key_id?
    if (s.empty()) return false;
    // first character must be lowercase letter
    if (!std::islower(static_cast<unsigned char>(s[0]))) return false;

    // remaining characters may be lowercase letters, digits, or underscore
    for (char ch : s) {
        if (!(std::islower(static_cast<unsigned char>(ch)) ||
              std::isdigit(static_cast<unsigned char>(ch)) ||
              ch == '_')) {
            return false;
        }
    }

    // must not be a keyword
    return !isKeyword(s);
}

bool Compiler::isInteger(string s) const { // is s an int?
    if (s.empty()) return false;

    size_t i = 0;
    // optional leading sign
    if (s[0] == '+' || s[0] == '-') {
        i = 1;
        // lone '+' or '-' is not an integer
        if (s.size() == 1) return false;
    }

    bool hasDigit = false;
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
        hasDigit = true;
    }

    return hasDigit;
}

bool Compiler::isBoolean(string s) const {      // is s a bool?
    return s == "true" || s == "false";
}

bool Compiler::isLiteral(string s) const {      // is s a lit?
    return isInteger(s) || isBoolean(s);
}

/* ------------------------------------------------------
    Action routines
    ------------------------------------------------------ */

void Compiler::insert(string externalName, storeTypes inType, modes inMode, string inValue, allocation inAlloc, int inUnits){
    std::vector<std::string> names = splitNames(externalName);

    for(const std::string& name : names){
        if (name.empty()) {
            processError("empty identifier in insert()");
            continue;
        }
        if(symbolTable.count(name)){
            processError("symbol " + name + " is multiply defined");
        } else if (isKeyword(name)) {
            processError("illegal use of keyword: " + name);
        } else {
            std::string internalName;
            if (!name.empty() && std::isupper(static_cast<unsigned char>(name[0]))) {
                internalName = name;
            } else {
                internalName = genInternalName(inType);
            }
            // Use the SymbolTableEntry constructor and insert into map
            symbolTable.emplace(name, SymbolTableEntry(internalName, inType, inMode, inValue, inAlloc, inUnits));
        }
    }
}

storeTypes Compiler::whichType(string name){     // which data type does name have?
    // Use global helpers and SymbolTableEntry getter
    if(::isBooleanLiteral(name)){
        return BOOLEAN;
    } else if(::isIntegerLiteral(name)){
        return INTEGER;
    } else if(symbolTable.count(name)){
        return symbolTable.at(name).getDataType();
    } else{     // name ident, const too hopefully
        processError("reference to undefined constant: " + name);
        return INTEGER;         // fallback to avoid compiler warning
    }
}

string Compiler::whichValue(string name){        // which value does name have?
    // Use global helpers and SymbolTableEntry getter
    if(::isBooleanLiteral(name) || ::isIntegerLiteral(name)){
        return name;
    } else if(symbolTable.count(name)){
        std::string val = symbolTable.at(name).getValue();
        if(!val.empty()){
             return val;
        } else {
            // This is likely a variable, not a constant
             processError("reference to variable or missing value for constant: " + name);
             return "";
        }
    } else{
        processError("reference to undefined constant: " + name);
        return "";      // fallback
    }
}

//////////////////// EXPANDED IN STAGE 1

void Compiler::code(string op, string operand1, string operand2){       // generates the code
    if(op == "program"){
        emitPrologue(operand1);
    } else if(op == "end"){
        emitEpilogue();
    } else if(op == "read"){
        emitReadCode(operand1);
    } else if(op == "write"){
        emitWriteCode(operand1);
    } else if(op == "+"){       // binary +
        emitAdditionCode(operand1, operand2);
    } else if(op == "-"){       // binary -
        emitSubtractionCode(operand1, operand2);
    } else if(op == "neg"){     // unary -
        emitNegationCode(operand1, operand2);
    } else if(op == "not"){
        emitNotCode(operand1, operand2);
    } else if(op == "*"){
        emitMultiplicationCode(operand1, operand2);
    } else if(op == "div" || op == "/"){
        emitDivisionCode(operand1, operand2);
    } else if(op == "mod" || op == "%"){
        emitModuloCode(operand1, operand2);
    } else if(op == "and" || op == "&&"){
        emitAndCode(operand1, operand2);
    } else if(op == "or" || op == "||"){
        emitOrCode(operand1, operand2);
    } else if(op == "=="){
        emitEqualityCode(operand1, operand2);
    } else if(op == "!="){
        emitInequalityCode(operand1, operand2);
    } else if(op == "<"){
        emitLessThanCode(operand1, operand2);
    } else if(op == "<="){
        emitLessThanOrEqualToCode(operand1, operand2);
    } else if(op == ">"){
        emitGreaterThanCode(operand1, operand2);
    } else if(op == ">="){
        emitGreaterThanOrEqualToCode(operand1, operand2);
    } else if(op == ":="){
        emitAssignCode(operand1, operand2);
    } else {
        processError("compiler error: illegal arguments to code(): " + op);
    }
}

// Stack helpers and emit functions

void Compiler::pushOperator(string name){         // push name onto operatorStk
    operatorStk.push(name);
}

string Compiler::popOperator(){   // pop name from operatorStk
    if (operatorStk.empty()) {
        processError("compiler error: operator stack underflow");
        return string();
    }
    string top = operatorStk.top();
    operatorStk.pop();
    return top;
}

void Compiler::pushOperand(string name){          // push name onto operandStk
    // If name is a literal and not already in the symbol table, create an entry
    if (isLiteral(name) || isInteger(name) || isBoolean(name)) {
        if (symbolTable.count(name) == 0) {
            // Determine literal type
            storeTypes t = whichType(name);
            // Insert literal as a constant with allocation so it can be emitted in storage
            insert(name, t, CONSTANT, name, YES, 1);
        }
    }
    operandStk.push(name);
}

string Compiler::popOperand(){    // pop name from operandStk
    if (operandStk.empty()) {
        processError("compiler error: operand stack underflow");
        return string();
    }
    string top = operandStk.top();
    operandStk.pop();
    return top;
}

/* ------------------------------------------------------
    Emit funcs
    ------------------------------------------------------ */

void Compiler::emit(string label, string instruction, string operands, string comment)
{
    objectFile << std::left                      // required
               << std::setw(8)  << label        // label width 8
               << std::setw(8)  << instruction  // instruction width 8
               << std::setw(24) << operands     // operands width 24
               << comment                        // comment directly after
               << "\n";
}

void Compiler::emitPrologue(string progName, string operand2)
{
    std::string timeStr = getTime();
    objectFile << "; SERENA REESE, AMIRAN FIELDS\t\t" << timeStr << "\n";

    // Include directives
    objectFile << "%INCLUDE \"Along32.inc\"\n";
    objectFile << "%INCLUDE \"Macros_Along.inc\"\n";

    objectFile << "\n"; // blank line

    emit("SECTION", ".text");
    emit("global", "_start", "", "; program " + progName);

    objectFile << "\n"; // another blank line

    emit("_start:");
}

void Compiler::emitEpilogue(string operand1, string operand2){
    emit("", "Exit", "{0}");
    objectFile << "\n"; // next blank line
    emitStorage();
}

void Compiler::emitStorage(){
    emit("SECTION", ".data");
    // Iterate using const auto& pair : symbolTable

    for(const auto& pair : symbolTable){
        const std::string& name = pair.first;
        const SymbolTableEntry& entry = pair.second;

        if (entry.getAlloc() == YES && entry.getMode() == CONSTANT){
            std::string value = entry.getValue();

            // Convert boolean constants
            if (value == "false" || value == "FALSE")
                value = "0";
            else if (value == "true" || value == "TRUE")
                value = "-1";
            else if (value.empty())
                value = "0";  // fallback

            emit(entry.getInternalName(), "dd", value, "; " + name);
        }
    }

    objectFile << "\n"; // blank line before next section

    emit("SECTION", ".bss");

    for(const auto& pair : symbolTable){
        const std::string& name = pair.first;
        const SymbolTableEntry& entry = pair.second;

        if(entry.getAlloc() == YES && entry.getMode() == VARIABLE){
            emit(entry.getInternalName(), "resd", std::to_string(entry.getUnits()), "; " + name);
        }
    }
}

//////////////////// EXPANDED DURING STAGE 1

void Compiler::emitReadCode(string operand, string /*operand2*/){
    // Expect operand to be a single identifier (readStmt already handles lists)
    std::string name = operand;

    if (name.empty()) {
        processError("internal error: empty operand to emitReadCode");
        return;
    }

    // Must be defined
    if (!symbolTable.count(name)) {
        processError("reference to undefined symbol: " + name);
        return;
    }

    const SymbolTableEntry &entry = symbolTable.at(name);

    // Only INTEGER variables may be read
    if (entry.getDataType() != INTEGER) {
        processError("can't read variables of this type: " + name);
        return;
    }

    // Must be a variable (not a constant)
    if (entry.getMode() != VARIABLE) {
        processError("attempting to read to a read-only location: " + name);
        return;
    }

    // Call the runtime ReadInt routine (assumes it returns value in eax)
    emit("", "call", "ReadInt", "; read int; value placed in eax");

    // Store eax into the variable's storage (use internal name)
    emit("", "mov", "[" + entry.getInternalName() + "], eax", "; store eax at " + name);

    // Track that A register (eax) no longer holds a useful named value;
    // but per the spec we set contentsOfAReg to the variable that now contains the value
    contentsOfAReg = name;
}

void Compiler::emitWriteCode(string operand, string /*operand2*/){
    // Expect operand to be a single operand (identifier, literal, or temp)
    std::string name = operand;

    if (name.empty()) {
        processError("internal error: empty operand to emitWriteCode");
        return;
    }

    // If it's a literal that was pushed earlier, it should exist in symbol table.
    if (!symbolTable.count(name)) {
        // If it's a literal, insert it so we can reference its internal name
        if (isLiteral(name) || isInteger(name) || isBoolean(name)) {
            insert(name, whichType(name), CONSTANT, name, YES, 1);
        } else {
            processError("reference to undefined symbol: " + name);
            return;
        }
    }

    const SymbolTableEntry &entry = symbolTable.at(name);

    // Ensure the value is in A (eax). If not, load it.
    if (contentsOfAReg != name) {
        // Load the value into eax from the symbol's internal storage
        emit("", "mov", "eax, [" + entry.getInternalName() + "]", "; load " + name + " into eax");
        contentsOfAReg = name;
    }

    // For INTEGER or BOOLEAN, call WriteInt (assumes value in eax)
    if (entry.getDataType() == INTEGER || entry.getDataType() == BOOLEAN) {
        emit("", "call", "WriteInt", "; write eax");
        // Print newline / CRLF
        emit("", "call", "Crlf", "; newline");
    } else {
        processError("cannot write value of this type: " + name);
    }
}

void Compiler::emitAssignCode(string operand1, string operand2){        // op2 = op1
    if (operand1.empty() || operand2.empty()) {
        processError("internal error: empty operand in emitAssignCode");
        return;
    }

    // operand2 must be a defined symbol (target)
    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol on left-hand side: " + operand2);
        return;
    }

    // Ensure operand1 exists in symbol table
    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1)){
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        }
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    contentsOfAReg.clear();
    }

    const SymbolTableEntry &srcEntry = symbolTable.at(operand1);
    const SymbolTableEntry &destEntry = symbolTable.at(operand2);


    // A. Register Spill Management: Deassign/Spill EAX if it holds a temporary *other than* operand1
    if (!contentsOfAReg.empty() && contentsOfAReg != operand1) {
        if (isTemporary(contentsOfAReg) && symbolTable.count(contentsOfAReg)) {
            // Spill the current contents of EAX to its memory location
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax",
                 "; spill A reg (" + contentsOfAReg + ")");
        }
        contentsOfAReg.clear(); // EAX is now clear
    }

    // B. Load Right Operand (operand1) into EAX if it's not already there
    if (contentsOfAReg != operand1) {
        if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
            // Use immediate MOV for integer literals
            emit("", "mov", "eax, " + srcEntry.getValue(), "; load immediate literal " + srcEntry.getValue());
        } else {
            // Load from memory for variables or temporaries
            emit("", "mov", "eax, [" + srcEntry.getInternalName() + "]", "; load " + operand1 + " into eax");
        }
        contentsOfAReg = operand1; // EAX now holds the value of operand1
    }

    // C. Perform Assignment: EAX -> Destination Memory
    // EAX holds the value of operand1, store it into operand2's memory location.
    emit("", "mov", "[" + destEntry.getInternalName() + "], eax", "; store eax into " + operand2);

    // D. Final State Cleanup
    // EAX still holds the value. Since we just assigned the value of operand1 to operand2,
    // EAX now also represents operand2. This is key for optimization of subsequent expressions.
    contentsOfAReg = operand2;
}

// Arithmetic / logical emit implementations

void Compiler::emitAdditionCode(string operand1, string operand2){       // op2 + op1
    // op1: right operand; op2: left operand (Accumulator)
    
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in addition (integers required)");
        return;
    }

    // A. Register Spill Management: Only spill if contentsOfAReg is a Temporary.
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg) && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", 
                 "; spill A reg (" + contentsOfAReg + ")");
        }
        // contentsOfAReg is NOT cleared here, relying on the load logic below to handle it.
    }

    const auto &op1Entry = symbolTable.at(operand1);
    const auto &op2Entry = symbolTable.at(operand2);

    // B. Load Left Operand (operand2) into EAX if not already there
    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + op2Entry.getInternalName() + "]", "; load " + operand2 + " in eax");
        
        // CRITICAL FIX: Only track if the loaded operand is a temporary.
        if (isTemporary(operand2)) {
            contentsOfAReg = operand2;
        } else {
            // If we load a constant/variable, we clear tracking. The result in EAX
            // is now an intermediate value not yet associated with a symbol name.
            contentsOfAReg.clear(); 
        }
    }
    // If contentsOfAReg == operand2, EAX is already set.

    // C. Perform Operation: Add operand1 to EAX
    if (op1Entry.getMode() == CONSTANT && isInteger(op1Entry.getValue())) {
        emit("", "add", "eax, " + op1Entry.getValue(), "; eax += " + op1Entry.getValue());
    } else {
        emit("", "add", "eax, [" + op1Entry.getInternalName() + "]", "; eax += " + operand1);
    }
    
    // D. Final Tracking Adjustment (Safety for chains like (1+2)+3):
    // After the calculation, if EAX was holding a non-temporary accumulator (constant/variable), 
    // we must ensure it's not tracked by that name, as the value is now different.
    if (!isTemporary(operand2)) {
        contentsOfAReg.clear();
    }
    // If operand2 was a Temporary, we leave contentsOfAReg as operand2 
    // to maintain the optimization chain (e.g., T1 + T2 = T1).
}

void Compiler::emitSubtractionCode(string operand1, string operand2){   // op2 - op1
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in subtraction (integers required)");
        return;
    }

    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    contentsOfAReg.clear();
    }

    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol (destination): " + operand2);
        return;
    }

    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
        contentsOfAReg.clear();
    }

    if (contentsOfAReg != operand2) {
        const auto &destEntry = symbolTable.at(operand2);
        emit("", "mov", "eax, [" + destEntry.getInternalName() + "]", "; load " + operand2 + " into eax");
        contentsOfAReg = operand2;
    }

    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "sub", "eax, " + srcEntry.getValue(), "; eax -= " + srcEntry.getValue());
    } else {
        emit("", "sub", "eax, " + srcEntry.getInternalName(), "; eax -= " + operand1);
    }

    const auto &destEntry = symbolTable.at(operand2);
    emit("", "mov", "[" + destEntry.getInternalName() + "], eax", "; store result into " + operand2);
    contentsOfAReg = operand2;

    if (isTemporary(operand1)) freeTemp();
}

void Compiler::emitMultiplicationCode(string operand1, string operand2){      // op2 * op1
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in multiplication (integers required)");
        return;
    }

    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
        contentsOfAReg.clear();
    }

// B. Load Left Operand (operand2) into EAX if not already there
    if (contentsOfAReg != operand2) {
        const auto &destEntry = symbolTable.at(operand2);
        emit("", "mov", "eax, [" + destEntry.getInternalName() + "]", "; load " + operand2 + " into eax");
        
        // CRITICAL FIX: Only track if the loaded operand is a temporary.
        if (isTemporary(operand2)) {
            contentsOfAReg = operand2;
        } else {
            contentsOfAReg.clear();
        }
    }

    // C. Perform Operation:
    const auto &op1Entry = symbolTable.at(operand1);
    if (op1Entry.getMode() == CONSTANT && isInteger(op1Entry.getValue())) {
        emit("", "imul", "eax, " + op1Entry.getValue(), "; eax *= " + op1Entry.getValue());
    } else {
        emit("", "imul", op1Entry.getInternalName(), "; eax *= " + operand1);
    }

    // D. Final Tracking Adjustment:
    // After the calculation, if EAX was holding a non-temporary accumulator (constant/variable), 
    // we must ensure it's not tracked by that name.
    if (!isTemporary(operand2)) {
        contentsOfAReg.clear();
    }
}

void Compiler::emitDivisionCode(string operand1, string operand2){       // op2 / op1
    // op2 is dividend (left), operand1 is divisor (right)
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in division (integers required)");
        return;
    }

    // Ensure operand1 exists in symbol table
    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    }

    // Ensure operand2 exists in symbol table
    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol (dividend): " + operand2);
        return;
    }

    // Declare symbol table entries once
    const auto &divisorEntry = symbolTable.at(operand1);
    const auto &dividendEntry = symbolTable.at(operand2);

    // Spill unrelated A reg content
    if (!contentsOfAReg.empty() && contentsOfAReg != operand2 && contentsOfAReg != operand1) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
        contentsOfAReg.clear();
    }

    // Load dividend (operand2) into eax if not already there
    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + dividendEntry.getInternalName() + "]", "; load dividend " + operand2 + " into eax");
        contentsOfAReg = operand2;
    }

    // Sign-extend eax into edx:eax
    emit("", "cdq", "", "; sign-extend eax into edx:eax");

    // Perform idiv by divisor (operand1). IDIV only accepts a memory or register operand.
    // The previous complex immediate check is replaced by relying on the internal name.
    emit("", "idiv", "dword [" + divisorEntry.getInternalName() + "]", "; idiv by " + operand1);
    // After IDIV, quotient is in eax. EAX now holds the new temporary result.

    // Free temporary operands
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

void Compiler::emitModuloCode(string operand1, string operand2){        // op2 % op1
    // op2 is dividend, operand1 is divisor; result should be remainder
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in modulo (integers required)");
        return;
    }

    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    contentsOfAReg.clear();
    }
    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol (dividend): " + operand2);
        return;
    }

    // Spill unrelated A reg content (This logic is correct for spilling)
    if (!contentsOfAReg.empty() && contentsOfAReg != operand2 && contentsOfAReg != operand1) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
    contentsOfAReg.clear();
    }

    // Load dividend (operand2) into eax if not already there
    if (contentsOfAReg != operand2) {
        const auto &dividendEntry = symbolTable.at(operand2);
        emit("", "mov", "eax, [" + dividendEntry.getInternalName() + "]", "; load dividend " + operand2 + " into eax");
        contentsOfAReg = operand2;
    }

    emit("", "cdq", "", "; sign-extend eax into edx:eax for idiv");

    const auto &divisorEntry = symbolTable.at(operand1);
    emit("", "idiv", divisorEntry.getInternalName(), "; idiv by " + operand1);

    // After IDIV: Quotient in EAX, Remainder in EDX.

    // Move remainder (EDX) into EAX (the accumulator for the result)
    emit("", "mov", "eax, edx", "; move remainder (edx) to accumulator (eax)");

    // Free temporary operands
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

void Compiler::emitNegationCode(string operand1, string /*operand2*/){      // -op1 (operand1 is destination temp)
    // operand1 is expected to be the destination (temp) that already contains the operand value
    if (!symbolTable.count(operand1)) {
        processError("reference to undefined symbol in negation: " + operand1);
        return;
    }
    if (whichType(operand1) != INTEGER) {
        processError("illegal type in negation (integer required)");

        return;
    }
    // Ensure value is in eax
    if (contentsOfAReg != operand1) {
        emit("", "mov", "eax, [" + symbolTable.at(operand1).getInternalName() + "]", "; load " + operand1 + " into eax for negation"$
    }

    emit("", "neg", "eax", "; negate eax");
}

void Compiler::emitNotCode(string operand1, string /*operand2*/){           // !op1 (operand1 is destination temp)
    if (!symbolTable.count(operand1)) {
        processError("reference to undefined symbol in not: " + operand1);
        return;
    }
    if (whichType(operand1) != BOOLEAN) {
        processError("illegal type in not (boolean required)");
        return;
    }

    // Ensure value is in eax
    if (contentsOfAReg != operand1) {
        emit("", "mov", "eax, [" + symbolTable.at(operand1).getInternalName() + "]", "; load " + operand1 + " into eax for not");
    }

    emit("", "not", "eax", "; bitwise not eax");
}

// 2 funcs above don't really need temp

void Compiler::emitAndCode(string operand1, string operand2){           // op2 && op1
    // operand2 is destination (left), operand1 is right operand
    if (whichType(operand1) != BOOLEAN || whichType(operand2) != BOOLEAN) {
        processError("illegal type in and (booleans required)");
        return;
    }

    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    contentsOfAReg.clear();
    }

    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol (destination): " + operand2);
        return;
    }

    // Spill unrelated A reg content
    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
    contentsOfAReg.clear();
    }

    // Load left operand (operand2) into eax if not already there
    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]", "; load " + operand2 + " into eax");
        contentsOfAReg = operand2;
    }

    // AND with operand1 (Correct)
    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "and", "eax, " + srcEntry.getValue(), "; eax &= " + srcEntry.getValue());
    } else {
        // Must use memory reference for non-immediate values
        emit("", "and", "eax, [" + srcEntry.getInternalName() + "]", "; eax &= " + operand1);
    }

    // Free temporary source operands
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

// Comparison and logical-or emit implementations

void Compiler::emitOrCode(string operand1, string operand2){            // op2 || op1
    // operand2 is destination (left), operand1 is right
    if (whichType(operand1) != BOOLEAN || whichType(operand2) != BOOLEAN) {
        processError("illegal type in or (booleans required)");
        return;
    }

    // Ensure operands exist in symbol table (literals may be inserted)
    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    contentsOfAReg.clear();
    }

    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol (destination): " + operand2);
        return;
    }

    // Spill unrelated A reg content (Correct)
    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
    contentsOfAReg.clear();
    }

    // Load left operand (operand2) into eax if not already there
    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]", "; load " + operand2 + " into eax");
        contentsOfAReg = operand2;
    }

    // OR with operand1
    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        // Immediate OR for constants
        emit("", "or", "eax, " + srcEntry.getValue(), "; eax |= " + srcEntry.getValue());
    } else {
        // Memory OR for variables/temporaries
        emit("", "or", "eax, [" + srcEntry.getInternalName() + "]", "; eax |= " + operand1);
    }

    // Free temporary source operands
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

void Compiler::emitEqualityCode(string operand1, string operand2){      // op2 == op1
    // Types must match
    storeTypes t1 = whichType(operand1);
    storeTypes t2 = whichType(operand2);
    if (t1 != t2) {
        processError("incompatible types in equality comparison");
        return;
    }

    // Ensure operands exist (insert literals if needed)
    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, t1, CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    }
    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol: " + operand2);
        return;
    }

    // Spill unrelated A reg content (Correct)
    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
    }

    // Load operand2 into eax if not already there
    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]", "; load " + operand2 + " into eax");
    }

    // Compare eax with operand1
    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getValue(), "; compare with " + srcEntry.getValue());
    } else {
        emit("", "cmp", "eax, [" + srcEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    // --- Generate Boolean Result in EAX ---

    // Prepare labels
    string Ltrue = getLabel();
    string Lend  = getLabel();

    // 1. Jump if equal to Ltrue
    emit("", "JE", Ltrue, "; jump if equal");

    // 2. Not equal (False path)
    // Load FALSE (0) into eax
    emit("", "mov", "eax, 0", "; load FALSE (0)");
    // Jump to end
    emit("", "jmp", Lend, "; jump to end");

    // 3. Label Ltrue (True path)
    emit(Ltrue + ":");
    // Load TRUE (-1) into eax
    emit("", "mov", "eax, -1", "; load TRUE (-1)");

    // 4. Label Lend:
    emit(Lend + ":");

    // After this block, EAX holds the boolean result (0 or -1).

    // Deassign/free temporaries used as operands
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

void Compiler::emitInequalityCode(string operand1, string operand2){    // op2 != op1
    // Reuse equality pattern but invert jump
    storeTypes t1 = whichType(operand1);
    storeTypes t2 = whichType(operand2);
    if (t1 != t2) {
        processError("incompatible types in inequality comparison");
        return;
    }

    // --- (Symbol Table checks omitted for brevity, assume they are correct) ---
    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, t1, CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    }
    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol: " + operand2);
        return;
    }

    // Spill unrelated A reg content (Correct)
    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
    contentsOfAReg.clear();
    }

    // Load operand2 into eax if not already there
    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]", "; load " + operand2 + " into eax");
    }

    // Compare eax with operand1
    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getValue(), "; compare with " + srcEntry.getValue());
    } else {
        // Use memory reference for non-immediate values
        emit("", "cmp", "eax, [" + srcEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    string Ltrue = getLabel();
    string Lend  = getLabel();

    // Jump if not equal to Ltrue
    emit("", "JNE", Ltrue, "; jump if not equal");

    // FALSE PATH
    //   Remove symbol table checks for "false" constant
    emit("", "mov", "eax, 0", "; load FALSE");
    emit("", "jmp", Lend, "; jump to end");

    // Ltrue: TRUE PATH
    emit(Ltrue + ":");
    //   Remove symbol table checks for "true" constant
    emit("", "mov", "eax, -1", "; load TRUE");

    emit(Lend + ":");

    // Deassign/free temporaries
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();

    //   REMOVE the redundant temporary management at the end:
    // string dest = getTemp();
    // symbolTable.at(dest).setDataType(BOOLEAN);
    // emit("", "mov", "[" + symbolTable.at(dest).getInternalName() + "], eax", "; store comparison result into " + dest);
    // contentsOfAReg = dest; // WRONG. Should be cleared.
    // pushOperand(dest);
}

void Compiler::emitLessThanCode(string operand1, string operand2){      // op2 < op1
    // op2 < op1  (operand2 is left, operand1 is right)
    if (whichType(operand1) != whichType(operand2)) {
        processError("incompatible types in less-than comparison");
        return;
    }

    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
     contentsOfAReg.clear();
    }

    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol: " + operand2);
        return;
    }

    // Spill unrelated A reg content (Correct)
    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
    contentsOfAReg.clear();
    }

    // Load operand2 into eax if not already there
    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]", "; load " + operand2 + " into eax");
    }

    // Compare eax with operand1
    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getValue(), "; compare with " + srcEntry.getValue());
    } else {
        // Use memory reference for non-immediate values
        emit("", "cmp", "eax, [" + srcEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    string Ltrue = getLabel();
    string Lend  = getLabel();

    // Jump if less (signed)
    emit("", "JL", Ltrue, "; jump if less");

    // FALSE PATH
    //   Remove symbol table checks for "false" constant
    emit("", "mov", "eax, 0", "; load FALSE");
    emit("", "jmp", Lend, "; jump to end");

    // Ltrue: TRUE PATH
    emit(Ltrue + ":");
    //   Remove symbol table checks for "true" constant
    emit("", "mov", "eax, -1", "; load TRUE");

    emit(Lend + ":");

    // Deassign/free temporaries
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

void Compiler::emitLessThanOrEqualToCode(string operand1, string operand2){     // op2 <= op1
    if (whichType(operand1) != whichType(operand2)) {
        processError("incompatible types in less-than-or-equal comparison");
        return;
    }

    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    }
    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol: " + operand2);
        return;
    }

    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
    contentsOfAReg.clear();
    }

    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]", "; load " + operand2 + " into eax");
    }

    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getValue(), "; compare with " + srcEntry.getValue());
    } else {
        // Use memory reference
        emit("", "cmp", "eax, [" + srcEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    string Ltrue = getLabel();
    string Lend  = getLabel();

    // Jump if less or equal (signed)
    emit("", "JLE", Ltrue, "; jump if less or equal");

    // FALSE PATH
    //   Remove symbol table checks
    emit("", "mov", "eax, 0", "; load FALSE (0)");
    emit("", "jmp", Lend, "; jump to end");

    // Ltrue: TRUE PATH
    emit(Ltrue + ":");
    //   Remove symbol table checks
    emit("", "mov", "eax, -1", "; load TRUE (-1)");

    emit(Lend + ":");

    // Deassign/free temporaries
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

void Compiler::emitGreaterThanCode(string operand1, string operand2){           // op2 > op1
    if (whichType(operand1) != whichType(operand2)) {
        processError("incompatible types in greater-than comparison");
        return;
    }

    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    }
    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol: " + operand2);
        return;
    }

    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
    }

    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]", "; load " + operand2 + " into eax");
    }

    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getValue(), "; compare with " + srcEntry.getValue());
    } else {
        // Use memory reference
        emit("", "cmp", "eax, [" + srcEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    string Ltrue = getLabel();
    string Lend  = getLabel();

    // Jump if greater (signed)
    emit("", "JG", Ltrue, "; jump if greater");


    // FALSE PATH
    //   Remove symbol table checks
    emit("", "mov", "eax, 0", "; load FALSE (0)");
    emit("", "jmp", Lend, "; jump to end");

    // Ltrue: TRUE PATH
    emit(Ltrue + ":");
    //   Remove symbol table checks
    emit("", "mov", "eax, -1", "; load TRUE (-1)");

    emit(Lend + ":");

    // Deassign/free temporaries
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

void Compiler::emitGreaterThanOrEqualToCode(string operand1, string operand2){  // op2 >= op1
    if (whichType(operand1) != whichType(operand2)) {
        processError("incompatible types in greater-than-or-equal comparison");
        return;
    }

    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1))
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    }
    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol: " + operand2);
        return;
    }

    if (!contentsOfAReg.empty() && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        if (symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + $
            symbolTable.at(contentsOfAReg).setAlloc(YES);
        }
    }

    if (contentsOfAReg != operand2) {
        emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]", "; load " + operand2 + " into eax");
    }

    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getValue(), "; compare with " + srcEntry.getValue());
    } else {
        // Use memory reference
        emit("", "cmp", "eax, [" + srcEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    string Ltrue = getLabel();
    string Lend  = getLabel();

    // Jump if greater or equal (signed)
    emit("", "JGE", Ltrue, "; jump if greater or equal");

    // FALSE PATH
    //   Remove symbol table checks
    emit("", "mov", "eax, 0", "; load FALSE (0)");
    emit("", "jmp", Lend, "; jump to end");

    // Ltrue: TRUE PATH
    emit(Ltrue + ":");
    //   Remove symbol table checks
    emit("", "mov", "eax, -1", "; load TRUE (-1)");

    emit(Lend + ":");

    // Deassign/free temporaries
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

/* ------------------------------------------------------
    Lexical routines
    ------------------------------------------------------ */

char Compiler::nextChar(){       // returns next char or END_OF_FILE marker
    char next;

    // Try to read next char
    if (!sourceFile.get(next)) {
        ch = END_OF_FILE;
        listingFile << "\n";
        return ch;
    }

    ch = next;

    // Print line number for the *first* line if nothing has printed yet
    if (begChar) {
        listingFile << std::right << std::setw(5) << lineNo << "|";
        begChar = false;
    }

    // Print the character
    listingFile << ch;

    // If we hit a newline, set flag for next line
    if (ch == '\n') {
        int peeked = sourceFile.peek();
        lineNo++;

        if (peeked == EOF) {
            begChar = false;
        } else {
            begChar = true;
        }
    }

    return ch;
}

string Compiler::nextToken(){   // returns next tok or END_OF_FILE marker
    token.clear();

    // Skip whitespace/comments until we produce a token or hit EOF
    while (true) {
        // If we've reached EOF character, return EOF token
        if (ch == END_OF_FILE) {
            return END_FILE_TOKEN;
        }

        // Skip whitespace
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            ch = nextChar();
            continue;
        }

        // Skip comments { ... }
        if (ch == '{') {
            // consume '{'
            ch = nextChar();
            while (ch != END_OF_FILE && ch != '}') {
                ch = nextChar();
            }
            if (ch == END_OF_FILE) {
                processError("unexpected end of file in comment");
                return END_FILE_TOKEN;
            }
            // consume closing '}'
            ch = nextChar();
            continue;
        }

        // Illegal '}' at token start
        if (ch == '}') {
            processError("'}' cannot begin token");
            ch = nextChar();
            continue;
        }

        // Special symbols (handle two-character tokens)
        if (isSpecialSymbol(ch)) {
            char first = ch;
            char saved = ch;
            ch = nextChar(); // advance to possibly form two-char token

            // Two-char combinations
            if (first == ':' && ch == '=') {
                token = ":=";
                ch = nextChar();
                return token;
            }
            if (first == '<' && ch == '=') {
                token = "<=";
                ch = nextChar();
                return token;
            }
            if (first == '>' && ch == '=') {
                token = ">=";
                ch = nextChar();
                return token;
            }
            if (first == '!' && ch == '=') {
                token = "!=";
                ch = nextChar();
                return token;
            }

            // Otherwise single-character special symbol
            token = std::string(1, saved);
            return token;
        }

        // Identifier or keyword: starts with lowercase letter
        if (std::islower(static_cast<unsigned char>(ch))) {
            token.push_back(ch);
            ch = nextChar();
            while (ch != END_OF_FILE && (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
                token.push_back(ch);
                ch = nextChar();
            }
            // normalize to lowercase already ensured by checks
            return token;
        }
        
        // Number literal (integer)
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            // Only check for digits; '+' and '-' must be parsed as separate operators
            while (ch != END_OF_FILE && std::isdigit(static_cast<unsigned char>(ch))) {
                token.push_back(ch);
                ch = nextChar();
            }
            return token;
        }

        // EOF check (redundant but safe)
        if (ch == END_OF_FILE) {
            return END_FILE_TOKEN;
        }

        // Anything else is illegal
        {
            std::string bad(1, ch);
            processError("illegal symbol '" + bad + "'");
            ch = nextChar();
            // continue scanning for next valid token
        }
    }
}

/* ------------------------------------------------------
    Other routines
    ------------------------------------------------------ */

string Compiler::genInternalName(storeTypes stype) const{
    // **FIXED: Use static global counters**
    if (stype == INTEGER) {
        return "I" + std::to_string(I_count++);
    } else if (stype == BOOLEAN) {
        return "B" + std::to_string(B_count++);
    } else {
        return "X";     // fallback for unknown types
    }
}

void Compiler::processError(string err){
    ++errorCount;

    std::cerr << "ERROR: " << err << " on line " << lineNo << std::endl;

    if (listingFile.is_open()) {
        listingFile << "\n";
        listingFile << "Error: Line " << lineNo << ": " << err << "\n" << std::endl;
    }

    // Flush object file so .asm contains header
    if (objectFile.is_open()) {
        objectFile.flush();
    }

    // For this assignment we stop on the first error (matches earlier behavior).
    // createListingTrailer prints summary and closes listing; guard against recursive calls.
    createListingTrailer();
    std::exit(EXIT_FAILURE);
}

//////////////////// EXPANDED DURING STAGE 1

void Compiler::freeTemp(){
    // Only decrement if we actually have a temp allocated
    if (currentTempNo > -1) {
        --currentTempNo;
    }
    
}

string Compiler::getTemp(){
    // Allocate a new temporary external name "Tn"
    ++currentTempNo;
    if (currentTempNo > maxTempNo) {
        maxTempNo = currentTempNo;
    }
    std::string temp = "T" + std::to_string(currentTempNo);

    // If this temp is new, insert into symbol table as an INTEGER variable by default.
    // (Type may be adjusted later by code generation routines.)
    if (symbolTable.count(temp) == 0) {
        insert(temp, INTEGER, VARIABLE, "", YES, 1);
    }

    return temp;
}

string Compiler::getLabel(){
    static int labelNo = 0;
    std::ostringstream oss;
    oss << "L" << labelNo++;
    return oss.str();
}

bool Compiler::isTemporary(string s) const{       // determines if s represents a temporary
    if (s.size() < 2) return false;
    if (s[0] != 'T') return false;
    for (size_t i = 1; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////

