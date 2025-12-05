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
    listingFile << "STAGE0:\tSERENA REESE, AMIRAN FIELDS\t\t" << timeStr << "\n\n";
    listingFile << std::left << std::setw(8) << "LINE" << std::setw(3) << " " << std::setw(23) << "SOURCE STATEMENT" << "\n";
    listingFile << std::string(60, '-') << "\n";
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

void Compiler::beginEndStmt(){  // stage 0, prod 5
    // token is "begin" on entry
    token = nextToken();        // advance to first token inside begin...end

    // In Stage 1 this is where execStmts() would be invoked.
    // For now, skip tokens until we find "end" (or EOF) while allowing nested handling later.
    while (token != "end" && !(token.size() == 1 && token[0] == END_OF_FILE)) {
        // If you implement execStmts later, call execStmts() here instead of skipping.
        token = nextToken();
    }

    if (token != "end") {
        processError("keyword \"end\" expected");
        // attempt to continue
    } else {
        token = nextToken();    // consume "end"
    }

    if (token != ".") {
        processError("period expected");
        // attempt to continue
    } else {
        token = nextToken();    // consume '.' and advance (should be EOF)
    }

    code("end", ".");
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

void Compiler::expresses(){     // stage 1, prod 10
    // handles additive and logical-or operators: +, -, or
    while (token == "+" || token == "-" || token == "or" || token == "||") {
        std::string op = token;
        token = nextToken(); // consume operator
        term();              // parse right-hand term

        // Pop operands: right then left
        std::string right = popOperand();
        std::string left  = popOperand();

        if (left.empty() || right.empty()) {
            processError("operand missing for binary operator");
            // push back what we have and return
            if (!left.empty()) pushOperand(left);
            if (!right.empty()) pushOperand(right);
            return;
        }

        // Create destination temporary and compute dest = left op right
        std::string dest = getTemp();
        // Copy left into dest
        emitAssignCode(left, dest);

        // Apply operator using dest as left operand
        if (op == "+") {
            emitAdditionCode(right, dest);
        } else if (op == "-") {
            emitSubtractionCode(right, dest);
        } else if (op == "or" || op == "||") {
            emitOrCode(right, dest);
        } else {
            processError("unknown additive/logical operator: " + op);
        }

        // Free temporaries used for left/right if they were temps
        if (isTemporary(left)) freeTemp();
        if (isTemporary(right)) freeTemp();

        // Push result temp
        pushOperand(dest);
    }
}

void Compiler::term(){          // stage 1, prod 11
    // term -> factor terms
    factor();
    terms();
}

void Compiler::terms(){         // stage 1, prod 12
    // handles multiplicative and logical-and operators: *, /, %, and
    while (token == "*" || token == "/" || token == "%" || token == "and" || token == "&&") {
        std::string op = token;
        token = nextToken(); // consume operator
        factor();            // parse right-hand factor

        // Pop operands: right then left
        std::string right = popOperand();
        std::string left  = popOperand();

        if (left.empty() || right.empty()) {
            processError("operand missing for multiplicative operator");
            if (!left.empty()) pushOperand(left);
            if (!right.empty()) pushOperand(right);
            return;
        }

        // Create destination temporary and compute dest = left op right
        std::string dest = getTemp();
        // Copy left into dest
        emitAssignCode(left, dest);

        // Apply operator using dest as left operand
        if (op == "*") {
            emitMultiplicationCode(right, dest);
        } else if (op == "/") {
            emitDivisionCode(right, dest);
        } else if (op == "%") {
            emitModuloCode(right, dest);
        } else if (op == "and" || op == "&&") {
            emitAndCode(right, dest);
        } else {
            processError("unknown multiplicative/logical operator: " + op);
        }

        // Free temporaries used for left/right if they were temps
        if (isTemporary(left)) freeTemp();
        if (isTemporary(right)) freeTemp();

        // Push result temp
        pushOperand(dest);
    }
}

void Compiler::factor(){        // stage 1, prod 13
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

        // Create destination temp and apply unary op
        std::string dest = getTemp();
        // Copy operand into dest
        emitAssignCode(opnd, dest);

        if (unary == "-") {
            emitNegationCode(dest);
        } else if (unary == "+") {
            // unary plus is a no-op (value already in dest)
        } else if (unary == "not") {
            emitNotCode(dest);
        } else {
            processError("unknown unary operator: " + unary);
        }

        if (isTemporary(opnd)) freeTemp();
        pushOperand(dest);
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

bool Compiler::isKeyword(const string &s) const {       // is s a keyword?
    return keywords.count(s) > 0;
}

bool Compiler::isSpecialSymbol(char c) const {  // is c a spec symb?
    return specialSymbols.count(c) > 0;
}

bool Compiler::isNonKeyId(const string &s) const {      // is s a non_key_id?
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

bool Compiler::isInteger(const string &s) const { // is s an int?
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

bool Compiler::isBoolean(const string &s) const {      // is s a bool?
    return s == "true" || s == "false";
}

bool Compiler::isLiteral(const string &s) const {      // is s a lit?
    return isInteger(s) || isBoolean(s);
}

/* ------------------------------------------------------
    Action routines
    ------------------------------------------------------ */

void Compiler::insert(string externalName, storeTypes inType, modes inMode, string inValue, allocation inAlloc, int inUnits){
    std::vector<std::string> names = splitNames(externalName);

    for(const std::string& name : names){
        if(symbolTable.count(name)){
            processError("symbol " + name + " is multiply defined);
        } else if (isKeyword(name)) {
            processError("illegal use of keyword: " + name);
        } else {
            std::string internalName = (std::isupper(static_cast<unsigned char>(name[0])) ? name : genInternalName(inType));
            // Use the SymbolTableEntry constructor
            symbolTable.insert({name, SymbolTableEntry(internalName, inType, inMode, inValue, inAlloc, inUnits)});
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
        return INTEGER;         // unreachable, avoids compiler warn
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
        return "";      // unreachable, avoids compiler warn
    }
}
//////////////////// EXPANDED IN STAGE 1

void Compiler::code(string op, string operand1, string operand2){       // generates the code
    if(op == "program"){
        emitPrologue(operand1);
    } else if(op == "end"){
        emitEpilogue();
    } else if(op == "read"){
        emit readCode();
    } else if(op == "write"){
        emit writeCode();
    } else if(op == "+"){       // must be binary +
        emit additionCode();
    } else if(op == "-"){       // must be binary -
        emit subtractionCode();
    } else if(op == "neg"){     // must be unary -
        emit negationCode();
    } else if(op == "not"){
        emit notCode();
    } else if(op == "*"){
        emit multiplicationCode();
    } else if(op == "div"){
        emit divisionCode();
    } else if(op == "mod"){
        emit moduloCode();
    } else if(op == "and"){
        emit andCode();
    // ...
    } else if(op == "="){
        emit equalityCode();
    } else if(op == ":="){
        emit assignmentCode();
    } else{
        processError("compiler error: illegal arguments to code()");
    }
}

//////////////////// EXPANDED IN STAGE 1

void pushOperator(string name){         // push name onto operatorStk
    // push name onto stack;
}

string popOperator(){   // pop name from operatorStk
    // if op stack != empty
    // return top element removed from Stck
    // else
    // processError (compiler error; operator stack underflow)
}

void pushOperand(string name){          // push name onto operandStk
    // if name is lit, also create symbol table entry for it
    // if name = lit & no symb table entry
        // insert symb table entry, call whichType to find data type of lit
    // push name onto stack
}
string popOperand(){    // pop name from operandStk
    // if operandStk != empty
    // return top element removed from stack
    // else
    // processError(compiler error; operand stack underflow)
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

void Compiler::emitReadCode(string operand, string operand2){
    string name
while (name is broken from list (operand) and put in name != "")
{
if name is not in symbol table
processError(reference to undefined symbol)
if data type of name is not INTEGER
processError(can't read variables of this type)
if storage mode of name is not VARIABLE
processError(attempting to read to a read-only location)
emit code to call the Irvine ReadInt function
emit code to store the contents of the A register at name
set the contentsOfAReg = name
}
void Compiler::emitWriteCode(string operand, string operand2){
string name
static bool definedStorage = false
while (name is broken from list (operand) and put in name != "")
{
if name is not in symbol table
processError(reference to undefined symbol)
if name is not in the A register
emit the code to load name in the A register
set the contentsOfAReg = name
if data type of name is INTEGER or BOOLEAN
emit code to call the Irvine WriteInt function
emit code to call the Irvine Crlf function
} // end while
}

void Compiler::emitAssignCode(string operand1, string operand2){        // op2=op1
if types of operands are not the same
processError(incompatible types)
if storage mode of operand2 is not VARIABLE
processError(symbol on left-hand side of assignment must have a storage mode of VARIABLE)
if operand1 = operand2 return
if operand1 is not in the A register then
emit code to load operand1 into the A register
emit code to store the contents of that register into the memory location pointed to by
operand2
set the contentsOfAReg = operand2
if operand1 is a temp then free its name for reuse
//operand2 can never be a temporary since it is to the left of ':='
}

void Compiler::emitAdditionCode(string operand1, string operand2){      // op2+op1
    // if type either operand is not int, processError(illegal type)
    // if A reg. holds temp not operand1 or operand2 then emit code store that temp mem
    // change alloc entry for temp in symb table to yes, deassign it
    // if A reg holds non-temp not operand1 or operand2, deassign
    // if neither operand in A reg., emit code perform reg.-mem add
    // deassign all temp in add and free names
    // A reg. next avail. temp name change type symb table entry int, push name of result onto operandStk
}

void Compiler::emitSubtractionCode(string operand1, string operand2){   //op2-op1
...
}

void Compiler::emitMultiplicationCode(string operand1, string operand2){        //op2*op1
...
}

void Compiler::emitDivisionCode(string operand1, string operand2){      //op2/op1
    // if type either operand not int, processError(illegal type)
    // if A reg. holds temp not op2 then emit code store temp to mem
    // change alloc entry for symb tab to yes, deassign
    // if A reg non-gemp not operand2 deassign
    // if op2 not in A reg., emit innst do reg.-mem load op2 into A
    // emit code exte sign div from A reg to edx:eax, emit code reg-mem div
    // deassign all temp invov free
    // A reg next avail temp name, change type symb tab to int, push name on operandStk
}

void Compiler::emitModuloCode(string operand1, string operand2){        //op2%op1
...
}

void Compiler::emitNegationCode(string operand1, string operand2){      //-op1
...
}

void Compiler::emitNotCode(string operand1, string operand2){           //!op1
...
}

void Compiler::emitAndCode(string operand1, string operand2){           //op2&&op1
if type of either operand is not boolean
processError(illegal type)
if the A Register holds a temp not operand1 nor operand2 then
emit code to store that temp into memory
change the allocate entry for the temp in the symbol table to yes
deassign it
if the A register holds a non-temp not operand1 nor operand2 then deassign it
if neither operand is in the A register then
emit code to load operand2 into the A register
emit code to perform register-memory and
deassign all temporaries involved in the and operation and free those names for reuse
A Register = next available temporary name and change type of its symbol table entry to boolean
push the name of the result onto operandStk
}

void Compiler::emitOrCode(string operand1, string operand2){            //op2||op1
...
}

void Compiler::emitEqualityCode(string operand1, string operand2){      //op2==op1
    if types of operands are not the same
processError(incompatible types)
if the A Register holds a temp not operand1 nor operand2 then
emit code to store that temp into memory
change the allocate entry for it in the symbol table to yes
deassign it
if the A register holds a non-temp not operand2 nor operand1 then deassign it
if neither operand is in the A register then
emit code to load operand2 into the A register
emit code to perform a register-memory compare
emit code to jump if equal to the next available Ln (call getLabel)
emit code to load FALSE into the A register
insert FALSE in symbol table with value 0 and external name false
emit code to perform an unconditional jump to the next label (call getLabel should be L(n+1))
emit code to label the next instruction with the first acquired label Ln
emit code to load TRUE into A register
insert TRUE in symbol table with value -1 and external name true
emit code to label the next instruction with the second acquired label L(n+1)
deassign all temporaries involved and free those names for reuse
A Register = next available temporary name and change type of its symbol table entry to boolean
push the name of the result onto operandStk
}

void Compiler::emitInequalityCode(string operand1, string operand2){    //op2!=op1
...
}

void Compiler::emitLessThanCode(string operand1, string operand2){      //op2<op1
...
}

void Compiler::emitLessThanOrEqualToCode(string operand1, string operand2){     //op2<=op1
...
}

void Compiler::emitGreaterThanCode(string operand1, string operand2){           //op2>op1
...
}

void Compiler::emitGreaterThanOrEqualToCode(string operand1, string operand2){  //op2>=op1
...
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

        if (peeked == EOF){
            begChar = false;
        } else{
            begChar = true;
        }
    }

    return ch;
}
string Compiler::nextToken(){   // returns next tok or END_OF_FILE marker
    token = "";

    while(token.empty()){
        switch(ch){
            case '{':   // skip comment
                while(nextChar() != '}' && std::string(1, ch) != END_FILE_TOKEN){}
                if(std::string(1, ch) == END_FILE_TOKEN){
                    processError("unexpected end of file in comment");
                } else{
                    nextChar();         // skip closing '}'
                }
                break;

            case '}':
                processError("/'}' cannot begin token");
                break;

            case ' ':
            case '\t':
            case '\n':
            case '\r':
                nextChar();     // skip whitespace
                break;
            default:
                if(isSpecialSymbol(ch)){
                    token += ch;
                    nextChar();
                } else if(std::islower(static_cast<unsigned char>(ch))){
                    token += ch;
                    while(std::isalnum(static_cast<unsigned char>(nextChar())) || ch == '_'){
                        token += ch;
                    }
                    if(std::string(1, ch) == END_FILE_TOKEN){
                         // If EOF, token is complete, don't error here, let parser handle it
                    }
                } else if(std::isdigit(static_cast<unsigned char>(ch))){
                    token += ch;
                    while(std::isdigit(static_cast<unsigned char>(nextChar()))){
                        token += ch;
                    }
                    if(std::string(1, ch) == END_FILE_TOKEN){
                        // EOF, let parser handle
                    }
                } else if(std::string(1, ch) == END_FILE_TOKEN){
                    token = END_OF_FILE;
                } else{         // default
                    processError("illegal symbol /'" + std::string(1, ch) + "\'");
                }
                break;
        }
    }

    return token;
}
/* ------------------------------------------------------
    Other routines
    ------------------------------------------------------ */

string Compiler::genInternalName(storeTypes stype) const{
    // **FIXED: Use static global counters**
    if(stype == INTEGER){
        return "I" + std::to_string(I_count++);
    } else if(stype == BOOLEAN){
        return "B" + std::to_string(B_count++);
    } else{
        return "X";     // fallback for unknown types
    }
}

void Compiler::processError(string err){
    ++errorCount;

    std::cerr << "ERROR: " << err << " on line " << lineNo << std::endl;

    if(listingFile.is_open()){
        listingFile << "\n";
        listingFile << "Error: Line " << lineNo << ": " << err << "\n" << std::endl;
    }

    // Flush object file so .asm contains header
    if(objectFile.is_open()){
        objectFile.flush();
    }

    if(errorCount > 0){
        createListingTrailer();
        std::exit(EXIT_FAILURE);
    }
}
//////////////////// EXPANDED DURING STAGE 1

void freeTemp(){
    currentTempNo--;
if (currentTempNo < -1)
processError(compiler error, currentTempNo should be ≥ –1)
}

string getTemp(){
    string temp;
currentTempNo++;
temp = "T" + currentTempNo;
if (currentTempNo > maxTempNo)
insert(temp, UNKNOWN, VARIABLE, "", NO, 1)
maxTempNo++
return temp
}

string getLabel(){
...
}

bool isTemporary(string s) const{       // determines if s rep. temp
...
}

/////////////////////////////////////////////////////////////////////////////
