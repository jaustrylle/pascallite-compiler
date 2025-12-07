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

// Define this near the top of stage1.cpp.
#define CHECK_OPERAND_LOGIC(name) \
    ([&]() -> std::string { \
        const std::string &_n = (name); \
        if (_n.empty()) return std::string(); \
                                           \
        if (symbolTable.count(_n)) return _n; \
        if (isInteger(_n) || isBoolean(_n) || isLiteral(_n)) { \
            storeTypes _t = whichType(_n); \
            insert(_n, _t, CONSTANT, _n, YES, 1); \
            return _n; \
        } \
        processError(std::string("reference to undefined symbol: ") + _n); \
        return std::string(); \
    }())

// --- Global State Definitions ---
// Missing private members in Compiler class
static std::set<std::string> keywords;
static std::set<char> specialSymbols;
static uint I_count = 0;
static uint B_count = 0;
static bool begChar = true;

// String rep of END_OF_FILE char
const std::string END_FILE_TOKEN = std::string(1, END_OF_FILE);
static std::vector<std::string> tempStack;   // LIFO list of temps created by getTemp()

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
    // NOTE: An initial call to term() MUST happen before this loop begins
    // (This is usually done in the calling production, e.g., in assignment or write statement)
    
    std::string op = ""; 

    while (token == "+" || token == "-" || token == "or" || token == "||") {
        op = token;
        token = nextToken(); // consume operator
        term();             // parse right-hand term

        // --- START BINARY OPERATION LOGIC (MOVE THIS INSIDE THE LOOP) ---
        // Pop operands: right then left
        std::string rightOpName = popOperand();
        std::string leftOpName = popOperand();

        if (leftOpName.empty() || rightOpName.empty()) {
            return;
        }

        // Apply operator: Emitter must calculate 'left op right', leave result in EAX.
        if (op == "+") {
            emitAdditionCode(rightOpName, leftOpName);
        } else if (op == "-") {
            emitSubtractionCode(rightOpName, leftOpName);
        } else if (op == "or" || op == "||") {
            emitOrCode(rightOpName, leftOpName);
        } else {
            processError("unknown additive/logical operator: " + op);
        }

        // 1. Free the right operand temporary (if used) as it is consumed.
        if (isTemporary(rightOpName)) freeTemp();

        // 2. Free the left operand temporary (if used) as it is consumed.
        if (isTemporary(leftOpName)) freeTemp();
            
        // 3. Update A register tracking and push the result ("EAX").
        contentsOfAReg = "EAX";
        
        // 4. PUSH the new result ("EAX") back onto the operand stack for the next iteration.
        pushOperand("EAX");
        // --- END BINARY OPERATION LOGIC ---
    }
}

void Compiler::term(){          // stage 1, prod 11
    // term -> factor terms
    factor();
    terms();
}

void Compiler::terms(){          // stage 1, prod 12
    // handles multiplicative and logical-and operators: *, /, %, and
    std::string op = "";
    while (token == "*" || token == "/" || token == "%" || token == "and" || token == "&&") {
        op = token;
        token = nextToken(); // consume operator
        factor();            // parse right-hand factor

        // Pop operands: right then left
        std::string rightOpName = popOperand();
        std::string leftOpName  = popOperand();

        if (leftOpName.empty() || rightOpName.empty()) {
            // ... (error handling) ... GOES HERE!!!!
            return;
        }

        // Apply operator: Emitter must calculate 'left op right', leave result in EAX.
        if (op == "*") {
            emitMultiplicationCode(rightOpName, leftOpName); // NOTE: Corrected argument order
        } else if (op == "/") {
            emitDivisionCode(rightOpName, leftOpName); // NOTE: Corrected argument order
        } else if (op == "%") {
            emitModuloCode(rightOpName, leftOpName); // NOTE: Corrected argument order
        } else if (op == "and" || op == "&&") {
            emitAndCode(rightOpName, leftOpName); // NOTE: Corrected argument order
        } else {
            processError("unknown multiplicative/logical operator: " + op);
        }

        // 1. Free the right operand temporary (if used) as it is consumed.
        if (isTemporary(rightOpName)) freeTemp();
            
        // 2. Free the left operand temporary (if used) as it is consumed.
        if (isTemporary(leftOpName)) freeTemp();
            
        // 3. Update A register tracking and push the result.
        //    The result of emit*Code is already in EAX.
        //    Use the special name "EAX" to denote the result is in the register.
        contentsOfAReg = "EAX";
        
        // 4. PUSH the special name "EAX" back onto the operand stack.
        //    This tells the next binary operation that its left operand is already loaded.
        pushOperand("EAX");
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

// =========================================================================
// Conditional register tracking for binary operations (op2 OP op1)
// =========================================================================

void Compiler::emitAdditionCode(string operand1, string operand2){ // op2 + op1
// Ensure operands exist (insert literals if needed), but skip CHECK_OPERAND_LOGIC 
    // if the operand is the special "EAX" tracking name used for chaining.

    if (operand1 != "EAX") operand1 = CHECK_OPERAND_LOGIC(operand1);
    if (operand2 != "EAX") operand2 = CHECK_OPERAND_LOGIC(operand2);

    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in addition (integers required)");
        return;
    }

    // A. Register Spill Management: only spill an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            // allocate inline (no header change)
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    const auto &op1Entry = symbolTable.at(operand1); // right
    const auto &op2Entry = symbolTable.at(operand2); // left

    // B. Load left operand into EAX if not already there
    if (contentsOfAReg != operand2 && contentsOfAReg != "EAX") {
        // FIX: Change 'leftEntry' to 'op2Entry' (already declared above)
        if (op2Entry.getMode() == CONSTANT && op2Entry.getInternalName().empty()) 
            emit("", "mov", "eax, " + op2Entry.getValue(), "; load immediate " + op2Entry.getValue());
        else
            emit("", "mov", "eax, [" + op2Entry.getInternalName() + "]", "; load " + operand2 + " into eax");
        // DO NOT change contentsOfAReg here.
    }

    // C. Perform addition (immediate if literal without internalName)
    if (op1Entry.getMode() == CONSTANT && op1Entry.getInternalName().empty() && isInteger(op1Entry.getValue())) {
        emit("", "add", "eax, " + op1Entry.getValue(), "; eax += " + op1Entry.getValue());
    } else {
        emit("", "add", "eax, [" + op1Entry.getInternalName() + "]", "; eax += " + operand1);
    }

    contentsOfAReg = "EAX"; // Signal that the result of the operation is in EAX
}

void Compiler::emitSubtractionCode(string operand1, string operand2){ // op2 - op1
    // Ensure operands exist (insert literals if needed), but skip CHECK_OPERAND_LOGIC 
    // if the operand is the special "EAX" tracking name used for chaining.

    if (operand1 != "EAX") operand1 = CHECK_OPERAND_LOGIC(operand1);
    if (operand2 != "EAX") operand2 = CHECK_OPERAND_LOGIC(operand2);

    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in subtraction (integers required)");
        return;
    }

    // A. Spill management: only spill an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    const auto &op1Entry = symbolTable.at(operand1); // right
    const auto &op2Entry = symbolTable.at(operand2); // left

    // B. Load left operand into EAX if not already there
    if (contentsOfAReg != operand2 && contentsOfAReg != "EAX") {
        // FIX: Change 'leftEntry' to 'op2Entry' (already declared above)
        if (op2Entry.getMode() == CONSTANT && op2Entry.getInternalName().empty()) 
            emit("", "mov", "eax, " + op2Entry.getValue(), "; load immediate " + op2Entry.getValue());
        else
            emit("", "mov", "eax, [" + op2Entry.getInternalName() + "]", "; load " + operand2 + " into eax");
        // DO NOT change contentsOfAReg here.
    }

    // C. Perform subtraction
    if (op1Entry.getMode() == CONSTANT && op1Entry.getInternalName().empty() && isInteger(op1Entry.getValue())) {
        emit("", "sub", "eax, " + op1Entry.getValue(), "; eax -= " + op1Entry.getValue());
    } else {
        emit("", "sub", "eax, [" + op1Entry.getInternalName() + "]", "; eax -= " + operand1);
    }

    contentsOfAReg = "EAX"; // Signal that the result of the operation is in EAX
}

void Compiler::emitMultiplicationCode(string operand1, string operand2) { // op2 * op1
// Ensure operands exist (insert literals if needed), but skip CHECK_OPERAND_LOGIC 
    // if the operand is the special "EAX" tracking name used for chaining.

    if (operand1 != "EAX") operand1 = CHECK_OPERAND_LOGIC(operand1);
    if (operand2 != "EAX") operand2 = CHECK_OPERAND_LOGIC(operand2);

    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in multiplication (integers required)");
        return;
    }

    // A. Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // B. Load multiplicand (operand2) into EAX if not already there
    const auto &leftEntry = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (leftEntry.getMode() == CONSTANT && leftEntry.getInternalName().empty())
            emit("", "mov", "eax, " + leftEntry.getValue(), "; load immediate " + leftEntry.getValue());
        else
            emit("", "mov", "eax, [" + leftEntry.getInternalName() + "]", "; load " + operand2 + " into eax");

        if (isTemporary(operand2)) contentsOfAReg = operand2;
        else contentsOfAReg.clear();
    }

    // C. Multiply using operand1 (right) as multiplier
    const auto &rightEntry = symbolTable.at(operand1);
    if (rightEntry.getMode() == CONSTANT && rightEntry.getInternalName().empty() && isInteger(rightEntry.getValue())) {
        emit("", "imul", "eax, " + rightEntry.getValue(), "; eax *= " + rightEntry.getValue());
    } else {
        emit("", "imul", "dword [" + rightEntry.getInternalName() + "]", "; eax *= " + operand1);
    }

    // D. Create and store result temp (allocate inline, only once)
    string tmp = getTemp(); // alloc==NO initially
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(INTEGER);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store multiplication result into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;

    // E. Free multiplier temp by name if you know it's safe (optional)
    // if (isTemporary(operand1)) freeTempByName(operand1);
}

void Compiler::emitDivisionCode(string operand1, string operand2){ // op2 / op1
    // Ensure operands exist (insert literals if needed), but skip CHECK_OPERAND_LOGIC 
    // if the operand is the special "EAX" tracking name used for chaining.

    if (operand1 != "EAX") operand1 = CHECK_OPERAND_LOGIC(operand1);
    if (operand2 != "EAX") operand2 = CHECK_OPERAND_LOGIC(operand2);

    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in division (integers required)");
        return;
    }

    // A. Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // B. Load dividend (operand2) into EAX if not already there
    const auto &leftEntry = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (leftEntry.getMode() == CONSTANT && leftEntry.getInternalName().empty())
            emit("", "mov", "eax, " + leftEntry.getValue(), "; load immediate " + leftEntry.getValue());
        else
            emit("", "mov", "eax, [" + leftEntry.getInternalName() + "]", "; load dividend " + operand2 + " into eax");
        contentsOfAReg = operand2;
    }

    // C. Sign-extend and divide
    emit("", "cdq", "", "; sign-extend eax into edx:eax for division");
    contentsOfAReg.clear(); // edx:eax special state

    const auto &rightEntry = symbolTable.at(operand1);
    emit("", "idiv", "dword [" + rightEntry.getInternalName() + "]", "; idiv by " + operand1);

    // D. Create and store quotient temp
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(INTEGER);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store quotient into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;

    // E. Free divisor temp by name if safe (optional)
    // if (isTemporary(operand1)) freeTempByName(operand1);
}

void Compiler::emitModuloCode(string operand1, string operand2){ // op2 % op1
    // Ensure operands exist (insert literals if needed), but skip CHECK_OPERAND_LOGIC 
    // if the operand is the special "EAX" tracking name used for chaining.

    if (operand1 != "EAX") operand1 = CHECK_OPERAND_LOGIC(operand1);
    if (operand2 != "EAX") operand2 = CHECK_OPERAND_LOGIC(operand2);

    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in modulo (integers required)");
        return;
    }

    // A. Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // B. Load dividend (operand2) into eax if not already there
    const auto &leftEntry = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (leftEntry.getMode() == CONSTANT && leftEntry.getInternalName().empty())
            emit("", "mov", "eax, " + leftEntry.getValue(), "; load immediate " + leftEntry.getValue());
        else
            emit("", "mov", "eax, [" + leftEntry.getInternalName() + "]", "; load dividend " + operand2 + " into eax");
        contentsOfAReg = operand2;
    }

    // C. Sign-extend and divide
    emit("", "cdq", "", "; sign-extend eax into edx:eax for idiv");
    contentsOfAReg.clear(); // edx:eax special state

    const auto &rightEntry = symbolTable.at(operand1);
    emit("", "idiv", "dword [" + rightEntry.getInternalName() + "]", "; idiv by " + operand1);

    // D. Move remainder into eax
    emit("", "mov", "eax, edx", "; move remainder (edx) to accumulator (eax)");

    // E. Create and store result temp (allocate inline)
    string tmp = getTemp(); // alloc==NO initially
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(INTEGER);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store remainder into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;

    // Optional: free divisor temp by name if you know it's safe
    // if (isTemporary(operand1)) freeTempByName(operand1);
}

// ------------------------------------------------------
// LOGICAL OPERATORS (with conditional tracking)
// ------------------------------------------------------

void Compiler::emitNegationCode(string operand1, string /*operand2*/){ // -op1
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    if (operand1.empty()) return;

    if (whichType(operand1) != INTEGER) {
        processError("illegal type in negation (integer required)");
        return;
    }

    // Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg) && contentsOfAReg != operand1) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load operand into EAX if needed
    if (contentsOfAReg != operand1) {
        emit("", "mov", "eax, [" + symbolTable.at(operand1).getInternalName() + "]",
             "; load " + operand1 + " into eax for negation");
    }

    // Perform negation
    emit("", "neg", "eax", "; negate eax");

    // Create and store result temp
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(INTEGER);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store negated value into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;
}

void Compiler::emitNotCode(string operand1, string /*operand2*/) {
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    if (operand1.empty()) return;

    if (whichType(operand1) != BOOLEAN) {
        processError("illegal type in not (boolean required)");
        return;
    }

    // Spill only if unrelated temp is in A
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg) && contentsOfAReg != operand1) {
        auto &spill = symbolTable.at(contentsOfAReg);
        if (spill.getAlloc() == NO) {
            spill.setAlloc(YES);
            if (spill.getInternalName().empty()) spill.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spill.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load operand into EAX if needed
    if (contentsOfAReg != operand1) {
        const auto &op = symbolTable.at(operand1);
        if (op.getMode() == CONSTANT && op.getInternalName().empty() && isBoolean(op.getValue())) {
            string imm = (op.getValue() == "true" || op.getValue() == "TRUE") ? "-1" : "0";
            emit("", "mov", "eax, " + imm, "; load immediate boolean " + op.getValue());
        } else {
            emit("", "mov", "eax, [" + symbolTable.at(operand1).getInternalName() + "]",
                 "; load " + operand1 + " into eax for not");
        }
    }

    // Perform NOT
    emit("", "not", "eax", "; bitwise not eax");

    // Create and store result temp (allocate inline)
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(BOOLEAN);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store boolean not into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;
}

void Compiler::emitAndCode(string operand1, string operand2) {
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    operand2 = CHECK_OPERAND_LOGIC(operand2);
    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != BOOLEAN || whichType(operand2) != BOOLEAN) {
        processError("illegal type in and (booleans required)");
        return;
    }

    // Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spill = symbolTable.at(contentsOfAReg);
        if (spill.getAlloc() == NO) {
            spill.setAlloc(YES);
            if (spill.getInternalName().empty()) spill.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spill.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load left operand (operand2) into EAX if needed
    const auto &left = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (left.getMode() == CONSTANT && left.getInternalName().empty() && isBoolean(left.getValue())) {
            string imm = (left.getValue() == "true" || left.getValue() == "TRUE") ? "-1" : "0";
            emit("", "mov", "eax, " + imm, "; load immediate boolean " + left.getValue());
        } else {
            emit("", "mov", "eax, [" + left.getInternalName() + "]", "; load " + operand2 + " into eax");
        }
        contentsOfAReg = isTemporary(operand2) ? operand2 : "";
    }

    // AND with operand1 (immediate boolean if available)
    const auto &right = symbolTable.at(operand1);
    if (right.getMode() == CONSTANT && right.getInternalName().empty() && isBoolean(right.getValue())) {
        string imm = (right.getValue() == "true" || right.getValue() == "TRUE") ? "-1" : "0";
        emit("", "and", "eax, " + imm, "; eax &= " + right.getValue());
    } else {
        emit("", "and", "eax, [" + right.getInternalName() + "]", "; eax &= " + operand1);
    }

    // Create and store result temp
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(BOOLEAN);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store boolean result into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;
}

void Compiler::emitOrCode(string operand1, string operand2) {
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    operand2 = CHECK_OPERAND_LOGIC(operand2);
    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != BOOLEAN || whichType(operand2) != BOOLEAN) {
        processError("illegal type in or (booleans required)");
        return;
    }

    // Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spill = symbolTable.at(contentsOfAReg);
        if (spill.getAlloc() == NO) {
            spill.setAlloc(YES);
            if (spill.getInternalName().empty()) spill.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spill.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load left operand (operand2) into EAX if needed
    const auto &left = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (left.getMode() == CONSTANT && left.getInternalName().empty() && isBoolean(left.getValue())) {
            string imm = (left.getValue() == "true" || left.getValue() == "TRUE") ? "-1" : "0";
            emit("", "mov", "eax, " + imm, "; load immediate boolean " + left.getValue());
        } else {
            emit("", "mov", "eax, [" + left.getInternalName() + "]", "; load " + operand2 + " into eax");
        }
        contentsOfAReg = isTemporary(operand2) ? operand2 : "";
    }

    // OR with operand1
    const auto &right = symbolTable.at(operand1);
    if (right.getMode() == CONSTANT && right.getInternalName().empty() && isBoolean(right.getValue())) {
        string imm = (right.getValue() == "true" || right.getValue() == "TRUE") ? "-1" : "0";
        emit("", "or", "eax, " + imm, "; eax |= " + right.getValue());
    } else {
        emit("", "or", "eax, [" + right.getInternalName() + "]", "; eax |= " + operand1);
    }

    // Create and store result temp
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(BOOLEAN);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store boolean result into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;
}

// ------------------------------------------------------
// COMPARISON OPERATORS (ensure contentsOfAReg is cleared after result)
// ------------------------------------------------------

void Compiler::emitEqualityCode(string operand1, string operand2){ // op2 == op1
    // Ensure operands exist (insert literals if needed)
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    operand2 = CHECK_OPERAND_LOGIC(operand2);
    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in equality (integers required)");
        return;
    }

    // Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load operand2 (left) into EAX
    const auto &leftEntry = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (leftEntry.getMode() == CONSTANT && leftEntry.getInternalName().empty() && isInteger(leftEntry.getValue()))
            emit("", "mov", "eax, " + leftEntry.getValue(), "; load immediate " + leftEntry.getValue());
        else
            emit("", "mov", "eax, [" + leftEntry.getInternalName() + "]", "; load " + operand2 + " into eax");
        // Clear tracking because comparison will overwrite EAX
        contentsOfAReg.clear();
    } else {
        contentsOfAReg.clear();
    }

    // Compare with operand1 (right)
    const auto &rightEntry = symbolTable.at(operand1);
    if (rightEntry.getMode() == CONSTANT && rightEntry.getInternalName().empty() && isInteger(rightEntry.getValue())) {
        emit("", "cmp", "eax, " + rightEntry.getValue(), "; compare with " + rightEntry.getValue());
    } else {
        emit("", "cmp", "eax, [" + rightEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    // Generate boolean result in EAX
    string Ltrue = getLabel();
    string Lend  = getLabel();
    emit("", "JE", Ltrue, "; jump if equal");
    emit("", "mov", "eax, 0", "; load FALSE (0)");
    emit("", "jmp", Lend, "; jump to end");
    emit(Ltrue + ":");
    emit("", "mov", "eax, -1", "; load TRUE (-1)");
    emit(Lend + ":");

    // Create and store result temp (allocate inline)
    string tmp = getTemp(); // alloc==NO initially
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(BOOLEAN);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store equality result into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;

    // Do not call ambiguous freeTemp() here; free temps explicitly elsewhere if needed
}

void Compiler::emitInequalityCode(string operand1, string operand2){ // op2 != op1
    // Ensure operands exist
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    operand2 = CHECK_OPERAND_LOGIC(operand2);
    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in inequality (integers required)");
        return;
    }

    // Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load operand2 (left) into EAX
    const auto &leftEntry = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (leftEntry.getMode() == CONSTANT && leftEntry.getInternalName().empty() && isInteger(leftEntry.getValue()))
            emit("", "mov", "eax, " + leftEntry.getValue(), "; load immediate " + leftEntry.getValue());
        else
            emit("", "mov", "eax, [" + leftEntry.getInternalName() + "]", "; load " + operand2 + " into eax");
        contentsOfAReg.clear();
    } else {
        contentsOfAReg.clear();
    }

    // Compare with operand1 (right)
    const auto &rightEntry = symbolTable.at(operand1);
    if (rightEntry.getMode() == CONSTANT && rightEntry.getInternalName().empty() && isInteger(rightEntry.getValue())) {
        emit("", "cmp", "eax, " + rightEntry.getValue(), "; compare with " + rightEntry.getValue());
    } else {
        emit("", "cmp", "eax, [" + rightEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    // Generate boolean result in EAX (JNE)
    string Ltrue = getLabel();
    string Lend  = getLabel();
    emit("", "JNE", Ltrue, "; jump if not equal");
    emit("", "mov", "eax, 0", "; load FALSE (0)");
    emit("", "jmp", Lend, "; jump to end");
    emit(Ltrue + ":");
    emit("", "mov", "eax, -1", "; load TRUE (-1)");
    emit(Lend + ":");

    // Create and store result temp (allocate inline)
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(BOOLEAN);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store inequality result into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;

    // Do not call ambiguous freeTemp() here
}

void Compiler::emitLessThanCode(string operand1, string operand2){ // op2 < op1
    // Ensure operands exist
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    operand2 = CHECK_OPERAND_LOGIC(operand2);
    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in less-than (integers required)");
        return;
    }

    // Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load left operand (operand2) into EAX
    const auto &leftEntry = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (leftEntry.getMode() == CONSTANT && leftEntry.getInternalName().empty() && isInteger(leftEntry.getValue()))
            emit("", "mov", "eax, " + leftEntry.getValue(), "; load immediate " + leftEntry.getValue());
        else
            emit("", "mov", "eax, [" + leftEntry.getInternalName() + "]", "; load " + operand2 + " into eax");
        // Clear tracking because comparison will overwrite EAX
        contentsOfAReg.clear();
    } else {
        contentsOfAReg.clear();
    }

    // Compare with operand1 (right)
    const auto &rightEntry = symbolTable.at(operand1);
    if (rightEntry.getMode() == CONSTANT && rightEntry.getInternalName().empty() && isInteger(rightEntry.getValue())) {
        emit("", "cmp", "eax, " + rightEntry.getValue(), "; compare with " + rightEntry.getValue());
    } else {
        emit("", "cmp", "eax, [" + rightEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    // Generate boolean result in EAX
    string Ltrue = getLabel();
    string Lend  = getLabel();
    emit("", "JL", Ltrue, "; jump if less");
    emit("", "mov", "eax, 0", "; load FALSE");
    emit("", "jmp", Lend, "; jump to end");
    emit(Ltrue + ":");
    emit("", "mov", "eax, -1", "; load TRUE");
    emit(Lend + ":");

    // Create and store result temp
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(BOOLEAN);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store < result into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;

    // Do not free temps here; free explicitly where lifecycle is known
}

void Compiler::emitLessThanOrEqualToCode(string operand1, string operand2){ // op2 <= op1
    // Ensure operands exist
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    operand2 = CHECK_OPERAND_LOGIC(operand2);
    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in less-than-or-equal (integers required)");
        return;
    }

    // Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load left operand (operand2) into EAX
    const auto &leftEntry = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (leftEntry.getMode() == CONSTANT && leftEntry.getInternalName().empty() && isInteger(leftEntry.getValue()))
            emit("", "mov", "eax, " + leftEntry.getValue(), "; load immediate " + leftEntry.getValue());
        else
            emit("", "mov", "eax, [" + leftEntry.getInternalName() + "]", "; load " + operand2 + " into eax");
        contentsOfAReg.clear();
    } else {
        contentsOfAReg.clear();
    }

    // Compare with operand1 (right)
    const auto &rightEntry = symbolTable.at(operand1);
    if (rightEntry.getMode() == CONSTANT && rightEntry.getInternalName().empty() && isInteger(rightEntry.getValue())) {
        emit("", "cmp", "eax, " + rightEntry.getValue(), "; compare with " + rightEntry.getValue());
    } else {
        emit("", "cmp", "eax, [" + rightEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    // Generate boolean result in EAX
    string Ltrue = getLabel();
    string Lend  = getLabel();
    emit("", "JLE", Ltrue, "; jump if less or equal");
    emit("", "mov", "eax, 0", "; load FALSE");
    emit("", "jmp", Lend, "; jump to end");
    emit(Ltrue + ":");
    emit("", "mov", "eax, -1", "; load TRUE");
    emit(Lend + ":");

    // Create and store result temp
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(BOOLEAN);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store <= result into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;

    // Do not free temps here
}

void Compiler::emitGreaterThanCode(string operand1, string operand2){ // op2 > op1
    // Ensure operands exist
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    operand2 = CHECK_OPERAND_LOGIC(operand2);
    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in greater-than (integers required)");
        return;
    }

    // Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load left operand (operand2) into EAX
    const auto &leftEntry = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (leftEntry.getMode() == CONSTANT && leftEntry.getInternalName().empty() && isInteger(leftEntry.getValue()))
            emit("", "mov", "eax, " + leftEntry.getValue(), "; load immediate " + leftEntry.getValue());
        else
            emit("", "mov", "eax, [" + leftEntry.getInternalName() + "]", "; load " + operand2 + " into eax");
        contentsOfAReg.clear();
    } else {
        contentsOfAReg.clear();
    }

    // Compare with operand1 (right)
    const auto &rightEntry = symbolTable.at(operand1);
    if (rightEntry.getMode() == CONSTANT && rightEntry.getInternalName().empty() && isInteger(rightEntry.getValue())) {
        emit("", "cmp", "eax, " + rightEntry.getValue(), "; compare with " + rightEntry.getValue());
    } else {
        emit("", "cmp", "eax, [" + rightEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    // Generate boolean result in EAX
    string Ltrue = getLabel();
    string Lend  = getLabel();
    emit("", "JG", Ltrue, "; jump if greater");
    emit("", "mov", "eax, 0", "; load FALSE (0)");
    emit("", "jmp", Lend, "; jump to end");
    emit(Ltrue + ":");
    emit("", "mov", "eax, -1", "; load TRUE (-1)");
    emit(Lend + ":");

    // Create and store result temp (allocate inline)
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(BOOLEAN);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store > result into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;

    // Do not call ambiguous freeTemp() here
}

void Compiler::emitGreaterThanOrEqualToCode(string operand1, string operand2){ // op2 >= op1
    // Ensure operands exist
    operand1 = CHECK_OPERAND_LOGIC(operand1);
    operand2 = CHECK_OPERAND_LOGIC(operand2);
    if (operand1.empty() || operand2.empty()) return;

    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in greater-than-or-equal (integers required)");
        return;
    }

    // Spill only an unrelated temp
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)
        && contentsOfAReg != operand1 && contentsOfAReg != operand2) {
        auto &spillEntry = symbolTable.at(contentsOfAReg);
        if (spillEntry.getAlloc() == NO) {
            spillEntry.setAlloc(YES);
            if (spillEntry.getInternalName().empty()) spillEntry.setInternalName(contentsOfAReg);
        }
        emit("", "mov", "[" + spillEntry.getInternalName() + "], eax", "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Load left operand (operand2) into EAX
    const auto &leftEntry = symbolTable.at(operand2);
    if (contentsOfAReg != operand2) {
        if (leftEntry.getMode() == CONSTANT && leftEntry.getInternalName().empty() && isInteger(leftEntry.getValue()))
            emit("", "mov", "eax, " + leftEntry.getValue(), "; load immediate " + leftEntry.getValue());
        else
            emit("", "mov", "eax, [" + leftEntry.getInternalName() + "]", "; load " + operand2 + " into eax");
        contentsOfAReg.clear();
    } else {
        contentsOfAReg.clear();
    }

    // Compare with operand1 (right)
    const auto &rightEntry = symbolTable.at(operand1);
    if (rightEntry.getMode() == CONSTANT && rightEntry.getInternalName().empty() && isInteger(rightEntry.getValue())) {
        emit("", "cmp", "eax, " + rightEntry.getValue(), "; compare with " + rightEntry.getValue());
    } else {
        emit("", "cmp", "eax, [" + rightEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    // Generate boolean result in EAX
    string Ltrue = getLabel();
    string Lend  = getLabel();
    emit("", "JGE", Ltrue, "; jump if greater or equal");
    emit("", "mov", "eax, 0", "; load FALSE (0)");
    emit("", "jmp", Lend, "; jump to end");
    emit(Ltrue + ":");
    emit("", "mov", "eax, -1", "; load TRUE (-1)");
    emit(Lend + ":");

    // Create and store result temp (allocate inline)
    string tmp = getTemp();
    SymbolTableEntry &tmpEntry = symbolTable.at(tmp);
    tmpEntry.setDataType(BOOLEAN);
    tmpEntry.setMode(VARIABLE);
    tmpEntry.setAlloc(YES);
    if (tmpEntry.getInternalName().empty()) tmpEntry.setInternalName(tmp);

    emit("", "mov", "[" + tmpEntry.getInternalName() + "], eax", "; store >= result into " + tmp);
    pushOperand(tmp);
    contentsOfAReg = tmp;

    // Do not call ambiguous freeTemp() here
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

// freeTemp: free the last temporary (LIFO). Keeps header signature unchanged.
void Compiler::freeTemp() {
    if (tempStack.empty()) return;          // nothing to free
    std::string temp = tempStack.back();
    tempStack.pop_back();

    auto it = symbolTable.find(temp);
    if (it != symbolTable.end()) {
        it->second.setAlloc(NO);
    }
    if (contentsOfAReg == temp) contentsOfAReg.clear();
}

// getTemp: create a new temp name and push onto tempStack
std::string Compiler::getTemp() {
    ++currentTempNo;
    if (currentTempNo > maxTempNo) maxTempNo = currentTempNo;
    std::string temp = "T" + std::to_string(currentTempNo);

    // Insert placeholder with alloc == NO so it won't appear in .data/.bss yet
    if (symbolTable.count(temp) == 0) {
        insert(temp, INTEGER, VARIABLE, "", NO, 1);
    } else {
        symbolTable.at(temp).setDataType(INTEGER);
        symbolTable.at(temp).setMode(VARIABLE);
        symbolTable.at(temp).setAlloc(NO);
    }

    // push onto LIFO stack for later freeTemp() calls
    tempStack.push_back(temp);
    return temp;
}

string Compiler::getLabel(){
    static int labelNo = 0;
    std::ostringstream oss;
    oss << "L" << labelNo++;
    return oss.str();
}

bool Compiler::isTemporary(string s) const {
    if (s.size() < 2) return false;
    if (s[0] != 'T') return false;
    for (size_t i = 1; i < s.size(); ++i) {
        if (!isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////

