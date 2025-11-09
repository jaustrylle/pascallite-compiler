/*
stage0.cpp
- Completes the implementation of Stage 0 for the CS4301 Pascallite compiler project
- Follows the header and pseudocode supplied in our course's materials
- Most of the processing is performed by the parser
- Main is a simple program as a result
*/

// REFER TO OVERALL COMPILER STRUCTURE - STAGE 0 FOR THIS PART
// Mark TODO where we want to refine the behavior in Stage 1

#include "stage0.h"
#include <ctime>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <stdexcept>

using namespace std;

/* ----------------------------
   Helper / Utility functions
   ---------------------------- */

static string toLower(const string &s) {
    string r = s;
    for (char &c : r) c = (char)tolower((unsigned char)c);
    return r;
}

/* ----------------------------
   SymbolTableEntry methods
   ---------------------------- */

SymbolTableEntry::SymbolTableEntry(string in, storeTypes st, modes m,
                                   string v, allocation a, int u) {
    setInternalName(in);
    setDataType(st);
    setMode(m);
    setValue(v);
    setAlloc(a);
    setUnits(u);
}

// Define Compiler methods

/* ----------------------------
   Compiler: constructor / destructor
   ---------------------------- */

Compiler::Compiler(char **argv) {
    // argv[1] = source, argv[2] = listing, argv[3] = object
    sourceFile.open(argv[1]);
    if (!sourceFile.is_open()) {
        cerr << "Cannot open source file: " << argv[1] << endl;
        exit(EXIT_FAILURE);
    }
    listingFile.open(argv[2]);
    if (!listingFile.is_open()) {
        cerr << "Cannot open listing file: " << argv[2] << endl;
        exit(EXIT_FAILURE);
    }
    objectFile.open(argv[3]);
    if (!objectFile.is_open()) {
        cerr << "Cannot open object file: " << argv[3] << endl;
        exit(EXIT_FAILURE);
    }
    // initialize state
    ch = ' ';
    token = "";
    lineNo = 0;
    errorCount = 0;
    nextChar();        // initialize ch to first char and write listing header lines as needed
    nextToken();       // initialize token
}

Compiler::~Compiler() {
    if (sourceFile.is_open()) sourceFile.close();
    if (listingFile.is_open()) listingFile.close();
    if (objectFile.is_open()) objectFile.close();
}

/* ----------------------------
   Listing header / trailer / errors
   ---------------------------- */

void Compiler::createListingHeader() {
    time_t t = time(nullptr);
    listingFile << "STAGE0 : YOUR NAME (S) " << ctime(&t);
    listingFile << "LINE NO. SOURCE STATEMENT" << endl;
    listingFile.flush();
}

void Compiler::createListingTrailer() {
    listingFile << endl << "COMPILATION TERMINATED" << endl;
    listingFile << errorCount << " ERRORS ENCOUNTERED" << endl;
    listingFile.flush();
}

void Compiler::processError(string err) {
    ++errorCount;
    listingFile << "ERROR (line " << lineNo << "): " << err << endl;
    listingFile.flush();
    exit(EXIT_FAILURE);
}

/* ----------------------------
   Emit functions
   ---------------------------- */

void Compiler::emit(string label, string instruction, string operands, string comment) {
    // Left-justify fields to match spec
    objectFile << left << setw(8) << label;
    objectFile << left << setw(8) << instruction;
    objectFile << left << setw(24) << operands;
    objectFile << comment << '\n';
}

void Compiler::emitPrologue(string progName, string) {
    // simple prologue similar to spec
    time_t t = time(nullptr);
    objectFile << "; YOUR NAME (S)\n";
    objectFile << ctime(&t);
    objectFile << "% INCLUDE \"Along32.inc\"\n";
    objectFile << "% INCLUDE \"Macros_Along.inc\"\n\n";
    emit("SECTION", ".text");
    emit("global", "_start", "", "; program " + progName);
    emit("_start", ":", "", "");
}

void Compiler::emitEpilogue(string, string) {
    emit("", "Exit", "{0}", "");
    emitStorage();
}

void Compiler::emitStorage() {
    // Emit constant storage then uninitialized (.bss) storage for variables
    emit("SECTION", ".data");
    for (auto &kv : symbolTable) {
        const string &ext = kv.first;
        const SymbolTableEntry &e = kv.second;
        if (e.getAlloc() == YES && e.getMode() == CONSTANT) {
            // emit a label and dd directive with value
            emit(e.getInternalName(), "dd", e.getValue(), "; " + ext);
        }
    }
    emit("SECTION", ".bss");
    for (auto &kv : symbolTable) {
        const string &ext = kv.first;
        const SymbolTableEntry &e = kv.second;
        if (e.getAlloc() == YES && e.getMode() == VARIABLE) {
            emit(e.getInternalName(), "resd", to_string(e.getUnits()), "; " + ext);
        }
    }
}

/* ----------------------------
   Internal name generation
   ---------------------------- */

string Compiler::genInternalName(storeTypes stype) const {
    static int nextI = 0;
    static int nextB = 0;
    if (stype == INTEGER) {
        stringstream ss; ss << "I" << nextI++;
        return ss.str();
    } else if (stype == BOOLEAN) {
        stringstream ss; ss << "B" << nextB++;
        return ss.str();
    } else { // PROG_NAME
        static int nextP = 0;
        stringstream ss; ss << "P" << nextP++;
        return ss.str();
    }
}

/* ----------------------------
   Helper lexicon checks
   ---------------------------- */

bool Compiler::isKeyword(string s) const {
    string t = toLower(s);
    static const vector<string> kw = {
        "program","const","var","begin","end","integer","boolean","not","true","false"
    };
    for (auto &k : kw) if (t == k) return true;
    return false;
}

bool Compiler::isSpecialSymbol(char c) const {
    static const string sy = ";:,=+-.{}";
    return sy.find(c) != string::npos;
}

bool Compiler::isNonKeyId(string s) const {
    if (s.empty()) return false;
    if (!islower((unsigned char)s[0])) return false;
    for (char c : s) if (!(isalnum((unsigned char)c) || c == '_')) return false;
    return !isKeyword(s);
}

bool Compiler::isInteger(string s) const {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') {
        if (s.size() == 1) return false;
        i = 1;
    }
    for (; i < s.size(); ++i) if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

bool Compiler::isBoolean(string s) const {
    string t = toLower(s);
    return t == "true" || t == "false";
}

bool Compiler::isLiteral(string s) const {
    if (isInteger(s)) return true;
    if (isBoolean(s)) return true;
    if (s == "not") return true; // handled as literal prefix by grammar
    return false;
}

// Implement nextChar and nextToken

/* ----------------------------
   Lexical scanner: nextChar, nextToken
   ---------------------------- */

char Compiler::nextChar() {
    char prev = ch;
    if (!sourceFile.get(ch)) {
        ch = END_OF_FILE;
    } else {
        // Echo character to listingFile, manage line numbers
        if (ch == '\n') {
            listingFile << '\n';
            lineNo++;
        } else {
            listingFile << ch;
        }
    }
    return ch;
}

string Compiler::nextToken() {
    token.clear();
    while (token.empty()) {
        if (ch == END_OF_FILE) {
            token = string(1, END_OF_FILE);
            return token;
        }
        if (ch == '{') { // comment
            while (true) {
                if (nextChar() == END_OF_FILE) processError("unexpected end of file in comment");
                if (ch == '}') {
                    nextChar(); // consume closing brace
                    break;
                }
            }
            continue;
        }
        if (ch == '}') {
            processError("'}' cannot begin token");
        }
        if (isspace((unsigned char)ch)) {
            nextChar();
            continue;
        }
        if (isSpecialSymbol(ch)) {
            token = string(1, ch);
            nextChar();
            // Special handling for ":" to treat as ":" token (no trailing space)
            return token;
        }
        if (islower((unsigned char)ch)) {
            token.push_back(ch);
            while (true) {
                char peek = nextChar();
                if (peek == END_OF_FILE) processError("unexpected end of file");
                if (isalnum((unsigned char)peek) || peek == '_') {
                    token.push_back(peek);
                } else {
                    break;
                }
            }
            return token;
        }
        if (isdigit((unsigned char)ch)) {
            token.push_back(ch);
            while (true) {
                char peek = nextChar();
                if (peek == END_OF_FILE) processError("unexpected end of file");
                if (isdigit((unsigned char)peek)) token.push_back(peek);
                else break;
            }
            return token;
        }
        // signs + or - can be tokens when found by grammar in constStmts, treat them as single-char tokens
        if (ch == '+' || ch == '-') {
            token = string(1, ch);
            nextChar();
            return token;
        }
        // Unknown/illegal symbol
        processError("illegal symbol");
    }
    return token;
}

// Implement parser routines with required Token checks and action calls
// These action calls should include insert, whichType, whichValue and code

/* ----------------------------
   Action Routines: insert, whichType, whichValue, code
   ---------------------------- */

void Compiler::insert(string externalName, storeTypes inType, modes inMode,
                      string inValue, allocation inAlloc, int inUnits) {
    // externalName may be a comma-separated list according to grammar (ids)
    // Split on commas and insert each name
    stringstream ss(externalName);
    string name;
    while (getline(ss, name, ',')) {
        // Trim whitespace
        while (!name.empty() && isspace((unsigned char)name.front())) name.erase(name.begin());
        while (!name.empty() && isspace((unsigned char)name.back())) name.pop_back();
        if (name.empty()) continue;
        if (symbolTable.find(name) != symbolTable.end()) {
            processError("multiple name definition: " + name);
        } else if (isKeyword(name)) {
            processError("illegal use of keyword: " + name);
        } else {
            if (!name.empty() && isupper((unsigned char)name[0])) {
                // Compiler-defined external name: internal name is same as external
                SymbolTableEntry e(name, inType, inMode, inValue, inAlloc, inUnits);
                e.setInternalName(name);
                symbolTable[name] = e;
            } else {
                string internal = genInternalName(inType);
                SymbolTableEntry e(internal, inType, inMode, inValue, inAlloc, inUnits);
                e.setInternalName(internal);
                symbolTable[name] = e;
            }
        }
    }
}

storeTypes Compiler::whichType(string name) {
    // if literal
    if (isInteger(name)) return INTEGER;
    if (isBoolean(name)) return BOOLEAN;
    // else it's an identifier - should be in symbol table
    auto it = symbolTable.find(name);
    if (it == symbolTable.end()) processError("reference to undefined constant: " + name);
    return it->second.getDataType();
}

string Compiler::whichValue(string name) {
    if (isInteger(name) || isBoolean(name)) return name;
    auto it = symbolTable.find(name);
    if (it == symbolTable.end()) processError("reference to undefined constant: " + name);
    string val = it->second.getValue();
    if (val.empty()) processError("reference to undefined constant value: " + name);
    return val;
}

void Compiler::code(string op, string operand1, string operand2) {
    if (op == "program") emitPrologue(operand1);
    else if (op == "end") emitEpilogue();
    else processError("illegal code operation: " + op);
}

/* ----------------------------
   Parser and production implementations
   ---------------------------- */

void Compiler::parser() {
    // ch and token were initialized in constructor
    if (token != "program") processError("keyword 'program' expected");
    prog();
}

void Compiler::prog() {
    if (token != "program") processError("keyword 'program' expected");
    progStmt();
    if (token == "const") consts();
    if (token == "var") vars();
    if (token != "begin") processError("keyword 'begin' expected");
    beginEndStmt();
    if (token != string(1, END_OF_FILE)) processError("no text may follow 'end'");
}

void Compiler::progStmt() {
    string x;
    if (token != "program") processError("keyword 'program' expected");
    x = nextToken(); // program name
    if (!isNonKeyId(x)) processError("program name expected");
    if (nextToken() != ";") processError("semicolon expected");
    nextToken(); // advance past ';'
    code("program", x);
    insert(x, PROG_NAME, CONSTANT, x, NO, 0);
}

void Compiler::consts() {
    if (token != "const") processError("keyword 'const' expected");
    if (nextToken() != "" && !isNonKeyId(token)) processError("non-keyword identifier must follow 'const'");
    constStmts();
}

void Compiler::constStmts() {
    string x, y;
    if (!isNonKeyId(token)) processError("non-keyword identifier expected");
    x = token;
    if (nextToken() != "=") processError("'=' expected");
    y = nextToken();
    // y can be "+", "-", "not", NON_KEY_ID, true, false, INTEGER
    if (!(y == "+" || y == "-" || y == "not" || isNonKeyId(y) || isBoolean(y) || isInteger(y)))
        processError("token to right of '=' illegal");
    if (y == "+" || y == "-") {
        if (!isInteger(nextToken())) processError("integer expected after sign");
        y = y + token; // combine sign and integer
    } else if (y == "not") {
        if (!isBoolean(nextToken())) processError("boolean expected after 'not'");
        if (token == "true") y = "false";
        else y = "true";
    }
    if (nextToken() != ";") processError("semicolon expected");
    // check data type of y
    if (!(isInteger(y) || isBoolean(y))) processError("data type of token on right-hand side must be INTEGER or BOOLEAN");
    // insert constant: external name x, type whichType(y), value whichValue(y)
    insert(x, whichType(y), CONSTANT, whichValue(y), YES, 1);
    x = nextToken();
    if (!(x == "begin" || x == "var" || isNonKeyId(x))) processError("non-keyword identifier, 'begin', or 'var' expected");
    if (isNonKeyId(x)) {
        token = x; // set current token to x and continue recursively
        constStmts();
    }
}

void Compiler::vars() {
    if (token != "var") processError("keyword 'var' expected");
    if (nextToken() != "" && !isNonKeyId(token)) processError("non-keyword identifier must follow 'var'");
    varStmts();
}

void Compiler::varStmts() {
    string x, y;
    if (!isNonKeyId(token)) processError("non-keyword identifier expected");
    x = ids(); // returns comma-separated ids and leaves token with next token after ids
    if (token != ":") processError("':' expected");
    if (nextToken() != "integer" && nextToken() != "boolean") processError("illegal type follows ':'");
    y = token; // "integer" or "boolean"
    if (nextToken() != ";") processError("semicolon expected");
    insert(x, (y == "integer" ? INTEGER : BOOLEAN), VARIABLE, "", YES, 1);
    if (nextToken() != "begin" && !isNonKeyId(token)) processError("non-keyword identifier or 'begin' expected");
    if (isNonKeyId(token)) varStmts();
}

void Compiler::beginEndStmt() {
    if (token != "begin") processError("keyword 'begin' expected");
    if (nextToken() != "end") processError("keyword 'end' expected");
    if (nextToken() != ".") processError("period expected");
    nextToken(); // advance past '.'
    code("end", ".");
}

string Compiler::ids() {
    if (!isNonKeyId(token)) processError("non-keyword identifier expected");
    string tempString = token;
    if (nextToken() == ",") {
        if (nextToken() != "" && !isNonKeyId(token)) processError("non-keyword identifier expected");
        // recursively build comma-separated list
        tempString = tempString + "," + ids();
    }
    return tempString;
}
