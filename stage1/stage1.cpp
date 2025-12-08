// Serena Reese and Amiran Fields - CS 4301 - Stage 1

/*
stage1.cpp
- Completes the implementation of Stage 1 for the CS4301 Pascallite compiler project
- Follows the header and pseudocode supplied in our course's materials
- Uses registers A (eax) and D (edx)
- D serves for mod division remainders
- Each register assigned at most one operand at a time
- parser needs to call freeTemp like this -> if (isTemporary(right)) freeTemp(right);, pass name
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

std::string whichTypeStr(storeTypes type);

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

std::string whichTypeStr(storeTypes type) {
    switch (type) {
        case INTEGER:
            return "INTEGER";
        case BOOLEAN:
            return "BOOLEAN";
        default:
            return "UNKNOWN_TYPE";
    }
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
    // semicolons consumed by execStmt
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

    // semicolons consumed by execStmt
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

    // semicolons consumed by execStmt
}

void Compiler::express(){       // stage 1, prod 9
    // express -> term expresses
    term();
    expresses();    // handles REL_OP
}

void Compiler::expresses(){    // handles REL_OP, stage 1, prod 10 (comparison emitter handling)
    if (token == "=" || token == "<>" || token == "<=" || token == ">=" || token == "<" || token == ">") {
        std::string op = token;
        token = nextToken(); // consume REL_OP
        term(); // Parse the RHS expression (which leaves its name on the stack)
        std::string right = popOperand();
        std::string left = popOperand();
        
        // ---------------------------------------------------------------
        // 1. ENSURE LHS IS IN EAX (Pre-Load for EAX Chaining)
        // ---------------------------------------------------------------
        
        if (contentsOfAReg != left) {
            emit("", "mov", "eax, [" + symbolTable.at(left).getInternalName() + "]", "; Load LHS (" + left + ") into EAX for comparison");
            // Note: Don't track here, as EAX is immediately overwritten by the boolean result.
        }
        
        // Clear contentsOfAReg regardless, as EAX is about to be overwritten by 0 or -1.
            contentsOfAReg.clear();
        
        // ---------------------------------------------------------------
        // 2. DISPATCH TO COMPARISON EMITTER (EAX = LHS op RHS)
        // ---------------------------------------------------------------
        if (op == "=") {
            emitEqualityCode(right, left);
        } else if (op == "<>") {
            emitInequalityCode(right, left);
        } else if (op == "<") {
            emitLessThanCode(right, left);
        } else if (op == ">") {
            emitGreaterThanCode(right, left);
        } else if (op == "<=") {
            emitLessThanOrEqualToCode(right, left);
        } else if (op == ">=") {
            emitGreaterThanOrEqualToCode(right, left);
        }
        
        // Emitters handle freeing temps for both left and right.
        
        // ---------------------------------------------------------------
        // 3. RESULT MANAGEMENT (Create new temp for Boolean result)
        // ---------------------------------------------------------------
        std::string resultName = getTemp();
        // Save the boolean result (0 or -1) from EAX into the new temporary's memory
        emit("", "mov", "[" + symbolTable.at(resultName).getInternalName() + "], eax", "; Save boolean result to " + resultName);
        
        // Update AReg tracking and push the result
        contentsOfAReg = resultName;
        pushOperand(resultName);
    }
}

void Compiler::term(){        // stage 1, prod 11
    // term -> factor terms
    factor();    // parses MULT_LEVEL_OP
    
    // Now, handle the rest of the terms (the additive loop)
    terms();    // handles ADD_LEVEL_OP
}

void Compiler::terms(){    // stage 1, prod 12: Handles ADD_LEVEL_OP (+, -, or)
    while (token == "+" || token == "-" || token == "or") {
        std::string op = token;
        token = nextToken();
        factor(); // Parse right-hand factor (additive level RHS)
        
        std::string right = popOperand();
        std::string left = popOperand();

        // ---------------------------------------------------------------
        // 1. ENSURE LHS IS IN EAX (Pre-Load for EAX Chaining)
        // Note: term() should have done the initial load, but we re-check here.
        // ---------------------------------------------------------------
        if (contentsOfAReg != left) {
            // Load the value of the LHS operand from memory into EAX
            emit("", "mov", "eax, [" + symbolTable.at(left).getInternalName() + "]", "; Load LHS (" + left + ") into EAX");
            contentsOfAReg = left;
        }
            
        // ---------------------------------------------------------------
        // 2. DISPATCH TO EMITTER (EAX = LHS op RHS)
        // ---------------------------------------------------------------
        if (op == "+") {
            emitAdditionCode(right, left);
        } else if (op == "-") {
            emitSubtractionCode(right, left);
        } else if (op == "or") {
            emitOrCode(right, left);
        }
        // Note: Emitter handles freeing 'right' temp.
        
        // ---------------------------------------------------------------
        // 3. RESULT MANAGEMENT (Temporary Generation for New Result)
        // ---------------------------------------------------------------
        std::string resultName = left;
        
        // 4. CLEANUP AND CONTINUE
        // If LHS was an old temporary, and we replaced it with a new name, free the old temp.
        if (isTemporary(left) && resultName != left) {
            freeTemp();
        }
            // Push the name tracking the new result back onto the stack
            pushOperand(resultName);
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

        if (unary == "+") {
            // Match: Unary Plus is a no-op.
            pushOperand(opnd);
        } else {
            // --- Unary Minus and NOT (Code Generating) ---

            // Apply unary op: Emitter calculates the operation, leaves result in EAX.
            if (unary == "-") {
                emitNegationCode(opnd); // Emitter should ensure opnd is in EAX, then negates.
            } else if (unary == "not") {
                emitNotCode(opnd);
            }
            
            // If the operand was a temporary, its value has been consumed and its name
            // can now track the new result (since its old value is no longer needed).
            // This relies on the subsequent logic (factors, terms) to handle freeing.
            
            // For now, just track the operand's name as the new content of AReg.
            contentsOfAReg = opnd;
            pushOperand(opnd); // The operand's name now represents the negated/NOT'd value in EAX
        }

    } else {
        // No unary operator; just parse part
        part();
    }

    // After factor, allow further factor-level processing if grammar requires
    factors();
}

void Compiler::factors() {
    // Loop: Handle subsequent multiplications, divisions, and logical ANDs
    while (token == "*" || token == "div" || token == "mod" || token == "and") {
        std::string op = token;
        token = nextToken();
        
        part(); // Parse the right operand (RHS). Pushes RHS name onto operandStk.
        
        std::string right = popOperand(); // RHS (e.g., "5")
        std::string left = popOperand();  // LHS (e.g., "3" or "T0")
        
        // Error Handling
        if (left.empty() || right.empty()) {
            processError("Missing operands for " + op);
            return;
        }
        
        // ---------------------------------------------------------------
        // 2. DISPATCH TO EMITTER (EAX = LHS op RHS)
        // ---------------------------------------------------------------
        if (op == "*") {
            // The emitter must load the RHS if it's not a constant and then perform imul
            emitMultiplicationCode(left, right); 
        } else if (op == "div") {
            emitDivisionCode(right, left);  
        } else if (op == "mod") {
            emitModuloCode(right, left);
        } else if (op == "and") {
            emitAndCode(right, left);
        }
        // Note: The Emitter is responsible for freeing the 'right' temporary if needed.
        
        // ---------------------------------------------------------------
        // 3. RESULT MANAGEMENT (Temporary Generation for New Result)
        // ---------------------------------------------------------------
        std::string resultName = right;
        
        // 4. CLEANUP AND CONTINUE
        // We only free the RHS temporary here if it hasn't been freed by the emitter (less common).
        // Since emitMultiplicationCode should free the RHS, we only need to push the result.
        
        // If LHS was an old temporary, we free it here, as its name is replaced by resultName (if resultName != left)
        if (isTemporary(left) && resultName != left) {
             freeTemp(); // This might be required if getTemp uses a complex pool
        }

        // Push the name tracking the new result (e.g., "T1") back onto the stack
        pushOperand(resultName);
    }
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
        std::string literalToken = token;
        
        if (symbolTable.find(literalToken) == symbolTable.end()) {
            // Fix: Use the correct type (INTEGER or BOOLEAN) instead of the undefined LITERAL
            storeTypes literalDataType = whichType(literalToken);
            
            // Assuming your insert signature is:
            // insert(externalName, storeTypes inType, modes inMode, string inValue, allocation inAlloc, int inUnits)
            insert(literalToken, literalDataType, CONSTANT, literalToken, YES, 1);
        }
        
        pushOperand(literalToken);
        token = nextToken();
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

void Compiler::code(string op, string operand1, string operand2) {
    bool isBinOp = false; // track if current op is binary

    // -------- SIMPLE DISPATCH --------
    if (op == "program") {
        emitPrologue(operand1);
    } 
    else if (op == "end") {
        emitEpilogue();
    } 
    else if (op == "read") {
        emitReadCode(operand1);
    } 
    else if (op == "write") {
        emitWriteCode(operand1);
    }

    // -------- BINARY OPERATORS --------
    else if (op == "+") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitAdditionCode(operand1, operand2);
    }

    else if (op == "-") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitSubtractionCode(operand1, operand2);
    }

    else if (op == "*") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitMultiplicationCode(operand1, operand2);
    }

    else if (op == "div" || op == "/") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitDivisionCode(operand1, operand2);
    }

    else if (op == "mod" || op == "%") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitModuloCode(operand1, operand2);
    }

    else if (op == "and" || op == "&&") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitAndCode(operand1, operand2);
    }

    else if (op == "or" || op == "||") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitOrCode(operand1, operand2);
    }

    else if (op == "==") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitEqualityCode(operand1, operand2);
    }

    else if (op == "!=") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitInequalityCode(operand1, operand2);
    }

    else if (op == "<") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitLessThanCode(operand1, operand2);
    }

    else if (op == "<=") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitLessThanOrEqualToCode(operand1, operand2);
    }

    else if (op == ">") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitGreaterThanCode(operand1, operand2);
    }

    else if (op == ">=") {
        isBinOp = true;
        if (contentsOfAReg != operand2)
            emit("", "mov", "eax, [" + symbolTable.at(operand2).getInternalName() + "]",
                 "; load " + operand2 + " into eax");
        emitGreaterThanOrEqualToCode(operand1, operand2);
    }

    // -------- UNARY OPERATORS --------
    else if (op == "neg") {
        emitNegationCode(operand1, operand2);
    }
    else if (op == "not") {
        emitNotCode(operand1, operand2);
    }

    // -------- ASSIGNMENT --------
    else if (op == ":=") {
        emitAssignCode(operand1, operand2);
    }

    // -------- ERROR --------
    else {
        processError("compiler error: illegal arguments to code(): " + op);
    }


    // =====================================================================
    //           P O S T - O P E R A T I O N  C L E A N U P
    // =====================================================================
    if (isBinOp) {
        // EAX now contains the result. The result inherits the name of the 
        // LEFT operand (operand2) for chaining.
        contentsOfAReg = operand2; // <-- FIXED: Must track the LHS name (operand2)
    
        // Right-hand operand (operand1) is consumed -> free temp if needed
        if (isTemporary(operand1)) // <-- FIXED: Must free the RHS temp (operand1)
            freeTemp();
            
        // Note: The variable 'operand2' is kept in the operand stack by the 
        // higher-level expression functions (e.g., factors) which will push it back 
        // after the call to code().
        return; 
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

// where literal constants are converted into symbol table entries and assigned properties
// DOES NOT perform the internal name assignment though
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

    // Load A into eax
    if (contentsOfAReg != name) {
        // Load the value into eax from the symbol's internal storage
        emit("", "mov", "eax, [" + entry.getInternalName() + "]", "; load " + name + " in eax");    // load name into eax
        contentsOfAReg = name;
    }

    // For INTEGER or BOOLEAN, call WriteInt (assumes value in eax)
    if (entry.getDataType() == INTEGER || entry.getDataType() == BOOLEAN) {
        emit("", "call", "WriteInt", "; write int in eax to standard out");
        // Print newline / CRLF
        emit("", "call", "Crlf", "; write \\r\\n to standard out");
    } else {
        processError("cannot write value of this type: " + name);
    }
}

void Compiler::emitAssignCode(string operand1, string operand2){     // op2 = op1
    if (operand1.empty() || operand2.empty()) {
        processError("internal error: empty operand in emitAssignCode");
        return;
    }

    // Ensure target (operand2) exists and source (operand1) exists/is inserted (initial symbol table checks remain unchanged)    
    // Check if operand2 is a defined symbol (target)
    if (!symbolTable.count(operand2)) {
        processError("reference to undefined symbol on left-hand side: " + operand2);
        return;
    }
    
    // Ensure operand1 exists (checking/inserting constant literals)
    if (!symbolTable.count(operand1)) {
        if (isLiteral(operand1) || isInteger(operand1) || isBoolean(operand1)){
            insert(operand1, whichType(operand1), CONSTANT, operand1, YES, 1);
        } else {
            processError("reference to undefined symbol: " + operand1);
            return;
        }
    }

    // Pascallite assignments require types to match.
    if (whichType(operand1) != whichType(operand2)) {
        processError("type mismatch in assignment: cannot assign " + whichTypeStr(whichType(operand1)) + " to " + whichTypeStr(whichType(operand2)));
        return;
    }

    // A. Register Spill Management: Spill temporaries unrelated to the current operation.
    if (!contentsOfAReg.empty() && contentsOfAReg != operand1) {
        if (isTemporary(contentsOfAReg) && symbolTable.count(contentsOfAReg)) {
            emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax",
                 "; spill A reg (" + contentsOfAReg + ")");
        }
        contentsOfAReg.clear(); 
    }

    const SymbolTableEntry &destEntry = symbolTable.at(operand2); // lhs is "a"

    // C. Perform Assignment: EAX -> Destination Memory
    emit("", "mov", "[" + destEntry.getInternalName() + "], eax", 
         "; " + operand2 + " = AReg"); // **FIXED COMMENT** to match target template

    // D. Final State Cleanup and Type Update
    // Deallocate Temporary storage if source was a temporary (value moved to lhs)
    if (isTemporary(operand1)) {
        symbolTable.at(operand1).setAlloc(NO); // Manually deallocate the specific temp
        // freeTemp(); // Use your internal freeTemp if it correctly handles counters
    }

    // EAX now holds the value of the destination variable (operand2).
    // The previous name (operand1) is gone, replaced by the destination name (operand2).
    // This is the CRITICAL change for EAX chaining:
    contentsOfAReg = operand2; // <-- CRITICAL: EAX now tracks the LHS variable ("a")
}

// =========================================================================
// Conditional register tracking for binary operations (op2 OP op1)
// =========================================================================

void Compiler::emitAdditionCode(string operand1, string operand2){
    // ASSUMPTION: operand2 (LHS) is already in EAX.

    const auto &op1Entry = symbolTable.at(operand1); // RHS

    // 1. Perform Operation (This is the only code generation needed)
    if (op1Entry.getMode() == CONSTANT && op1Entry.getInternalName().empty() && isInteger(op1Entry.getValue())) {
        emit("", "add", "eax, " + op1Entry.getInternalName(), "; AReg = " + operand2 + " + " + operand1);
    } else {
        emit("", "add", "eax, [" + op1Entry.getInternalName() + "]", "; AReg = " + operand2 + " + " + operand1);
    }

    // 3. Free the RHS temporary
    if (isTemporary(operand1)) freeTemp();
    
    // The result is in EAX, tracked by operand2's name.
    // The calling function (e.g., terms) pushes operand2 back onto the stack.
}

void Compiler::emitSubtractionCode(string operand1, string operand2){       // op2 - op1
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in subtraction (integers required)");
        return;
    }

    // A. Spill management (same as addition, but ensure `contentsOfAReg.clear()` is called after spill)
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    const auto &op1Entry = symbolTable.at(operand1);

    // C. Perform Operation
    if (op1Entry.getMode() == CONSTANT && op1Entry.getInternalName().empty() && isInteger(op1Entry.getValue())) {
        emit("", "sub", "eax, " + op1Entry.getInternalName(), "; eax -= " + op1Entry.getValue());
    } else {
        // Must reference memory for SUB
        emit("", "sub", "eax, [" + op1Entry.getInternalName() + "]", "; eax -= " + operand1); 
    }
    
    // E. Temporary Cleanup
    if (isTemporary(operand1)) freeTemp();
    // Do NOT free operand2 here.
}

void Compiler::emitMultiplicationCode(string operand1, string operand2){       // op2 * op1, BINARY
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in multiplication (integers required)");
        return;
    }

    // A. Register Spill Management (REVISED)
    // Check if EAX holds a temporary AND that temporary is NOT the left operand.
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg) && contentsOfAReg != operand2) {
        // ... (Spill logic remains the same, but it's now conditional) ...
        
        emit("", "mov", "[" + symbolTable.at(contentsOfAReg).getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }
    // If contentsOfAReg == operand2 (e.g., T0), we skip the spill.

    const auto &op1Entry = symbolTable.at(operand1);

    // C. Perform Operation - REVISED LOGIC
    // Check if the right operand is a constant literal value (e.g., "5") *not* yet assigned an internal name (rare in your setup)
    if (op1Entry.getMode() == CONSTANT && op1Entry.getInternalName().empty()) {
        // Use two-operand IMUL with the immediate value if possible.
        // NOTE: Standard IMUL syntax for immediate values often uses 'imul eax, eax, value' 
        //       but let's use the simplest, most compatible syntax for now.
        emit("", "imul", "eax, " + op1Entry.getInternalName(), "; AReg = " + operand2 + " * " + op1Entry.getValue());
    } else {
        // This is the standard path for variables, temporaries, and constants 
        // already assigned an internal memory location (like [I1]).
        emit("", "imul", "dword [" + op1Entry.getInternalName() + "]", "; AReg = " + operand2 + " * " + operand1);
    }
    
    // E. Temporary Cleanup
    if (isTemporary(operand1)) freeTemp();
}

void Compiler::emitDivisionCode(string operand1, string operand2){       // op2 / op1
    // (op1Entry is the divisor, op2Entry is the dividend)
    const auto &op1Entry = symbolTable.at(operand1);
    
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in division (integers required)");
        return;
    }

    // A. Spill unrelated A reg content (Keep as is)
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // C. Sign-extend eax into edx:eax
    emit("", "cdq", "", "; sign-extend eax into edx:eax for division");
    contentsOfAReg.clear(); // EAX/EDX contents are now computed, clear tracking

    // D. Perform IDIV (Quotient in EAX, Remainder in EDX)
    // FIX: Use op1Entry instead of the undefined divisorEntry
    emit("", "idiv", "dword [" + op1Entry.getInternalName() + "]", "; idiv by " + operand1);

    if (isTemporary(operand1)) freeTemp();
}

void Compiler::emitModuloCode(string operand1, string operand2){        // op2 % op1
    // (op1Entry is the divisor, op2Entry is the dividend)
    const auto &op1Entry = symbolTable.at(operand1);
    
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
        processError("illegal type in modulo (integers required)");
        return;
    }

    // A. Spill unrelated A reg content (Keep as is)
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // C. Sign-extend eax into edx:eax
    emit("", "cdq", "", "; sign-extend eax into edx:eax for idiv");
    contentsOfAReg.clear(); // EAX/EDX contents are now computed, clear tracking

    // D. Perform IDIV (Quotient in EAX, Remainder in EDX)
    // FIX: Use op1Entry instead of the undefined divisorEntry
    emit("", "idiv", "dword [" + op1Entry.getInternalName() + "]", "; idiv by " + operand1);

    // E. Move remainder (EDX) into EAX (the accumulator for the result)
    emit("", "mov", "eax, edx", "; move remainder (edx) to accumulator (eax)");
    
    // G. Temporary Cleanup
    if (isTemporary(operand1)) freeTemp();
}

// ------------------------------------------------------
// LOGICAL OPERATORS (with conditional tracking)
// ------------------------------------------------------

void Compiler::emitNegationCode(string operand1, string /*operand2*/){      // -op1, UNARY
    // type checks, error handling
    if (!symbolTable.count(operand1)) {
        processError("reference to undefined symbol in negation: " + operand1);
        return;
    }
    if (whichType(operand1) != INTEGER) {
        processError("illegal type in negation (integer required)");
        return;
    }

    // --- Spill Logic (Save EAX if it holds a temporary result) ---
    // The spill logic ensures that if EAX currently holds the result of a previous
    // calculation (tracked by contentsOfAReg) and we are about to clobber EAX,
    // that result is first saved to memory.
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg) && contentsOfAReg != operand1) {
        auto &entry = symbolTable.at(contentsOfAReg);
        if (entry.getAlloc() == NO) {
            // Allocate storage manually if the temporary hasn't been saved yet
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                // If the temp has no internal name (I_T0, etc.), generate one now
                entry.setInternalName(genInternalName(entry.getDataType()));
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear(); // EAX is now clear
    }

    // --- 1. Ensure EAX has the value of the operand ---
    // Fix: The variable name was 'operand' but should be 'operand1'.
    if (contentsOfAReg != operand1) {
        emit("", "mov", "eax, [" + symbolTable.at(operand1).getInternalName() + "]", 
             "; Load " + operand1 + " into EAX for negation");
        contentsOfAReg = operand1; // EAX is now tracked by the operand's name
    }

    // --- 2. Perform the operation ---
    emit("", "neg", "eax", "; AReg = -AReg");
    
    // --- 3. Free the temporary storage for the consumed operand ---
    // The value of 'operand1' is consumed and its *name* will be reused to track the new result.
    if (isTemporary(operand1)) {
        // Set the storage name alloc to NO so its storage location is available.
        symbolTable.at(operand1).setAlloc(NO);
    }
    
    // --- 4. Update AReg Tracking ---
    // EAX now holds the result of the negation. The name 'operand1' is what is
    // pushed back onto the operand stack (in factor()), so 'operand1' will now 
    // symbolically represent the new value in EAX.
    // We clear it here because the value changed, even though the name (operand1) is reused.
    contentsOfAReg.clear(); 
}

void Compiler::emitNotCode(string operand1, string /*operand2*/) { // !op1, UNARY

    // --- Error Handling and Type Checks ---
    if (!symbolTable.count(operand1)) {
        processError("reference to undefined symbol in not: " + operand1);
        return;
    }
    if (whichType(operand1) != BOOLEAN) {
        processError("illegal type in not (boolean required)");
        return;
    }

    // --- 1. Spill EAX if it holds a valuable, unsaved temporary ---
    // If EAX holds the result of a previous calculation (contentsOfAReg is a temporary
    // and its value is NOT the one we are about to load in step 2), save it.
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg) && contentsOfAReg != operand1) {
        auto &entry = symbolTable.at(contentsOfAReg);
        if (entry.getAlloc() == NO) {
            // Allocate storage manually (as per instruction for stage 1)
            entry.setAlloc(YES);
            // Ensure internal name is set if it's a temp (like T0 -> I_T0)
            if (entry.getInternalName().empty()) {
                entry.setInternalName(genInternalName(entry.getDataType())); // Use genInternalName
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear(); // EAX is now empty/stale
    }
    // Note: If contentsOfAReg == operand1, we can skip the load below!
    

    // --- 2. Load Operand1 into EAX (if not already there) ---
    // This is the operand we are about to NOT.
    if (contentsOfAReg != operand1) {
        emit("", "mov", "eax, [" + symbolTable.at(operand1).getInternalName() + "]", 
             "; Load " + operand1 + " into EAX for NOT");
        contentsOfAReg = operand1; // EAX now holds operand1's value
    }

    // --- 3. Perform the NOT Operation ---
    // Note: Pascallite uses 0 for False and 1 for True. 'not eax' is a bitwise NOT,
    // which inverts all bits (e.g., 0 becomes all ones). A logical NOT is needed.
    // The typical logical NOT sequence (A_reg = !A_reg):
    
    // a) Compare EAX to 0 (False)
    emit("", "cmp", "eax, 0", "; check if AReg is False (0)");
    
    // b) If not False (i.e., True, 1), clear EAX (set result to False, 0)
    //    If False (0), the carry flag will be set.
    
    // Create a label for the jump
    string L1 = getLabel();
    emit("", "je", L1, "; if False, jump to L1 (result is True)"); 
    
    // EAX was True (1), so result is False (0)
    emit("", "mov", "eax, 0", "; result is False"); 
    string L2 = getLabel();
    emit("", "jmp", L2, "; jump over True result"); 
    
    // L1: EAX was False (0), so result is True (1)
    emit(L1, "mov", "eax, 1", "; result is True"); 
    
    // L2: Continue
    emit(L2, "", "", ""); 

    
    // --- 4. Free the temporary storage for the consumed operand ---
    if (isTemporary(operand1)) {
        // Since freeTemp() is argument-less, we MUST set alloc=NO directly
        // on the operand that was consumed (operand1) to make the name available for reuse.
        symbolTable.at(operand1).setAlloc(NO); 
        // We *could* call freeTemp() here if it decrements the temporary counter, 
        // but it's safer to only modify the specific temporary entry.
    }
    
    // --- 5. Update AReg Tracking ---
    // EAX now holds the result of the NOT, which is no longer tracked by 'operand1'.
    // The name 'operand1' is now available to track the *new* result in EAX 
    // when it is pushed back onto the operand stack in factor().
    contentsOfAReg.clear();
}

void Compiler::emitAndCode(string operand1, string operand2){            // op2 && op1
    if (whichType(operand1) != BOOLEAN || whichType(operand2) != BOOLEAN) {
        processError("illegal type in and (booleans required)");
        return;
    }

    // A. Spill
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // C. AND with operand1
    const auto &op1Entry = symbolTable.at(operand1);
    if (op1Entry.getMode() == CONSTANT && isInteger(op1Entry.getValue())) {
        emit("", "and", "eax, " + op1Entry.getInternalName(), "; eax &= " + op1Entry.getValue());
    } else {
        emit("", "and", "eax, [" + op1Entry.getInternalName() + "]",
             "; eax &= " + operand1);
    }

    // D. Temporary Cleanup
    if (isTemporary(operand1)) freeTemp();
}

void Compiler::emitOrCode(string operand1, string operand2){             // op2 || op1
    if (whichType(operand1) != BOOLEAN || whichType(operand2) != BOOLEAN) {
        processError("illegal type in or (booleans required)");
        return;
    }

    // A. Spill
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // C. OR with operand1
    const auto &op1Entry = symbolTable.at(operand1);
    if (op1Entry.getMode() == CONSTANT && isInteger(op1Entry.getValue())) {
        emit("", "or", "eax, " + op1Entry.getInternalName(), "; eax |= " + op1Entry.getValue());
    } else {
        emit("", "or", "eax, [" + op1Entry.getInternalName() + "]",
             "; eax |= " + operand1);
    }

    // D. Temporary Cleanup
    if (isTemporary(operand1)) freeTemp();
}

// ------------------------------------------------------
// COMPARISON OPERATORS (ensure contentsOfAReg is cleared after result)
// ------------------------------------------------------
//// These must ensure the LHS (op2) is loaded into EAX before cmp instructions
//// Loading logic belongs in calling parser function, not in emitters
//// Emitters here assume EAX holds op2 for LHS, only compares with op1 for RHS

void Compiler::emitEqualityCode(string operand1, string operand2){       // op2 == op1
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
            processError("illegal type in equality (integers required)");
            return;
        }
    
    // Spill unrelated A reg content
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Parser MUST ensure EAX = operand2 here!
    
    // Compare eax with operand1
    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getInternalName(), "; compare with " + srcEntry.getValue());
    } else {
        emit("", "cmp", "eax, [" + srcEntry.getInternalName() + "]", "; compare with " + operand1);
    }

    // --- Generate Boolean Result in EAX ---
    string Ltrue = getLabel();
    string Lend  = getLabel();
    emit("", "JE", Ltrue, "; jump if equal");
    emit("", "mov", "eax, 0", "; load FALSE (0)");
    emit("", "jmp", Lend, "; jump to end");
    emit(Ltrue + ":");
    emit("", "mov", "eax, -1", "; load TRUE (-1)");
    emit(Lend + ":");

    // EAX now holds the boolean result. contentsOfAReg MUST remain clear 
    // until the calling expression routine assigns a new temporary name.
    // Deassign/free temporaries (Correct for comparison)
    if (isTemporary(operand1)) freeTemp();
    if (isTemporary(operand2)) freeTemp();
}

void Compiler::emitInequalityCode(string operand1, string operand2){    // op2 != op1
    if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
            processError("illegal type in inequality (integers required)");
            return;
        }

    // Spill unrelated A reg content
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // emitEqualityCode and emitInequalityCode must ensure LHS is loaded into EAX before cmp instruction
    // Parser so far does this
    
    // Compare eax with operand1
    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getInternalName(), "; compare with " + srcEntry.getValue());
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
}

void Compiler::emitLessThanCode(string operand1, string operand2){      // op2 < op1
        if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
            processError("illegal type in less-than (integers required)");
            return;
        }

    // Spill unrelated A reg content
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    // Compare eax with operand1
    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getInternalName(), "; compare with " + srcEntry.getValue());
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
        if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
            processError("illegal type in less-than-or-equal (integers required)");
            return;
        }

    // Spill unrelated A reg content
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }
    
    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getInternalName(), "; compare with " + srcEntry.getValue());
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
        if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
            processError("illegal type in greater-than (integers required)");
            return;
        }

    // Spill unrelated A reg content
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getInternalName(), "; compare with " + srcEntry.getValue());
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
        if (whichType(operand1) != INTEGER || whichType(operand2) != INTEGER) {
            processError("illegal type in greater-than-or-equal (integers required)");
            return;
        }

    // Spill unrelated A reg content
    if (!contentsOfAReg.empty() && isTemporary(contentsOfAReg)) {
        auto &entry = symbolTable.at(contentsOfAReg);  // okay inside Compiler member
        if (entry.getAlloc() == NO) {
            // allocate manually here since allocateTempStorage() is unavailable
            entry.setAlloc(YES);
            if (entry.getInternalName().empty()) {
                entry.setInternalName(contentsOfAReg);
            }
        }
        emit("", "mov", "[" + entry.getInternalName() + "], eax",
             "; spill A reg (" + contentsOfAReg + ")");
        contentsOfAReg.clear();
    }

    const auto &srcEntry = symbolTable.at(operand1);
    if (srcEntry.getMode() == CONSTANT && isInteger(srcEntry.getValue())) {
        emit("", "cmp", "eax, " + srcEntry.getInternalName(), "; compare with " + srcEntry.getValue());
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

// Must rely on contentsOfAReg to decide what to free, so only
// use in a context where name in contentsOfAReg is temp being consumed
// usually only true for unary ops or when temp = finalResult
void Compiler::freeTemp() {
    // This implementation assumes the last-used temporary is the one to be freed.    
    if (maxTempNo >= 0) {
        string tempName = "T" + std::to_string(maxTempNo);
        
        // Instead of decrementing the counter, we simply mark the storage for the highest
        // temporary name as available, effectively enabling its reuse if the compiler
        // generated a system to recycle from maxTempNo downwards (not guaranteed).
        
        // if *highest* temporary first:
        if (symbolTable.count(tempName)) {
            symbolTable.at(tempName).setAlloc(NO);
        }
        
        --currentTempNo; // Only way to reuse the highest temp name
    }
    // Note: Do NOT use contentsOfAReg here, that is the name of the expression result
}

// 2) Create temp name (no allocation of storage)
string Compiler::getTemp() {
    // Current strategy: start at -1, so first call yields T0
    // Use the counter value, then increment for the *next* call.
    
    // Increment first to get the next number (from -1 to 0, 0 to 1, etc.)
    ++currentTempNo;
    if (currentTempNo > maxTempNo) maxTempNo = currentTempNo;
    
    string temp = "T" + std::to_string(currentTempNo);
    
    // Insert placeholder if desired, but mark alloc == NO (as required)
    if (symbolTable.count(temp) == 0) {
        // Assume your insert handles creating the internal name (e.g., I_T0)
        // Set mode=VARIABLE since it's an intermediate result, not a constant.
        insert(temp, INTEGER, VARIABLE, "", NO, 1); // alloc == NO
    } else {
        // Reset properties if the name was somehow reused/changed type
        symbolTable.at(temp).setDataType(INTEGER);
        symbolTable.at(temp).setMode(VARIABLE);
        symbolTable.at(temp).setAlloc(NO);
    }
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
