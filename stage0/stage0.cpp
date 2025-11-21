// Serena Reese and Amiran Fields - CS 4301 - Stage 0

/*
stage0.cpp
- Completes the implementation of Stage 0 for the CS4301 Pascallite compiler project
- Follows the header and pseudocode supplied in our course's materials
- Most of the processing is performed by the parser
- Main is a simple program as a result
*/

#include <stage0.h> // iostream, fstream, string, map, namespace, SymbolTable

#include <iomanip>
#include <cctype>
#include <sstream>
#include <cstdlib>      // for exit

#include <set>
#include <vector>
#include <chrono>       // for time
#include <ctime>

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

std::vector<std::string> splitNames(std::string names) {
    std::vector<std::string> result;
    std::stringstream ss(names);
    std::string item;
    while (std::getline(ss, item, ',')) {
        // Trim whitespace if necessary (optional)
        result.push_back(item);
    }
    return result;
}

// Global version for use in whichType/whichValue
bool isBooleanLiteral(std::string s) {
    return s == "true" || s == "false";
}

// Global version for use in whichType/whichValue
bool isIntegerLiteral(std::string s) {
    if(s.empty()) return false;
    size_t i = (s[0] == '+' || s[0] == '-') ? 1 : 0;
    for(; i < s.size(); ++i){
        if(!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
        // Ensures at least one digit is present (e.g., prevents '+' or '-' alone)
    return (s.size() > i);
}

/////////////////////////////////////////////////////////////////////////////

/* ------------------------------------------------------
    Compiler class declared in stage0.h, now define its funcs
    ------------------------------------------------------ */

Compiler::Compiler(char **argv){        // constructor
    sourceFile.open(argv[1]);
    listingFile.open(argv[2]);
    objectFile.open(argv[3]);

    // Initialize static global sets
    keywords = {"program", "const", "var", "begin", "end", "integer", "boolean", "true", "false", "not"};
    specialSymbols = {':', ',', ';', '=', '+', '-', '.', '(', ')', '{', '}'};
}

Compiler::~Compiler(){  // destructor
    if (sourceFile.is_open()) sourceFile.close();
    if (listingFile.is_open()) listingFile.close();
    if (objectFile.is_open()) objectFile.close();
}

void Compiler::createListingHeader(){
    std::string timeStr = getTime();

    // Listing header output to listingFile, not console; SOURCE STATEMENT begins in line 23
    listingFile << "STAGE0:\tSERENA REESE, AMIRAN FIELDS\t\t" << timeStr << "\n\n";
    listingFile << std::left << "LINE NO.\t" << std::setw(23) << "SOURCE STATEMENT" << "\n\n";
    lineNo = 1;
}

void Compiler::parser(){
    nextChar(); // ch must be initialized to 1st char of source file

    if(nextToken() != "program"){
        processError("keyword \"program\" expected");
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
        listingFile << "COMPILATION TERMINATED\t\t"
                    << errorCount << " " << errorWord << " ENCOUNTERED"
                    << std::endl;
    }
}

/* ------------------------------------------------------
    Methods implementing grammar prods
    ------------------------------------------------------ */

void Compiler::prog(){  // stage0, prod 1
        // token is already "program"

    progStmt();
    if(token == "const"){
        consts();
    }
    if(token == "var"){
        vars();
    }
    if(token != "begin"){
        processError("keyword \"begin\" expected");
        // Recovery for Stage 1, Stage 0 we terminate
    }

    beginEndStmt();
        // Use the static global token value
    if(token != END_FILE_TOKEN){
        processError("no text may follow \"end\"");
    }
}

void Compiler::progStmt(){      // stage 0, prod 2
    std::string x;

    x = nextToken();    // get program name

    if(!isNonKeyId(x)){
        processError("program name expected");
    }

    token = nextToken();        // expect semicolon
    if(token != ";"){
        processError("semicolon expected");
    }

    token = nextToken();        // advance to next tok
    insert(x, PROG_NAME, CONSTANT, x, NO, 0);
    code("program", x);
}

void Compiler::consts(){        // stage0, prod 3
    // token is already "const" from prog()

    token = nextToken();        // advance to next tok
    if(!isNonKeyId(token)){
        processError("non-keyword identifier must follow \"const\"");
    }

    constStmts();       // process const declars
}

void Compiler::vars(){  // stage 0, prod 4
    // token is already "var" from prog()

    token = nextToken();        // advance to next tok
    if(!isNonKeyId(token)){
        processError("non-keyword identifier must follow \"var\"");
    }

    varStmts(); // process var declars
}

void Compiler::beginEndStmt(){  // stage 0, prod 5
    // token is already "begin"

    token = nextToken();        // advance to next tok
    if(token != "end"){
        // In Stage 0, "end" immediately, in Stage 1 process statements
        processError("keyword \"end\" expected");
    }

    token = nextToken();        // advance to next tok
    if(token != "."){
        processError("period expected");
    }

    token = nextToken();        // final advance, should be END_OF_FILE
    code("end", ".");
}

void Compiler::constStmts(){    // stage 0, prod 6
    std::string x, y;
    storeTypes type;
    std::string val; // actual value to store

    if(!isNonKeyId(token)){
        processError("non-keyword identifier expected");
    }

    x = token;
    if(nextToken() != "="){
        processError("\"=\" expected");
    }

    y = nextToken(); // Token on the right of '='

    // 1. Check for unary operator (+, -, not)
    if(y == "+" || y == "-"){
        // Case 1: Signed Integer
        std::string sign = y;
        std::string next = nextToken();
        if(!isInteger(next)){
            processError("integer expected after sign");
        }
        val = sign + next; // e.g., "-1"
        type = INTEGER;
    }
    else if(y == "not"){
        // Case 2: NOT Boolean
        std::string next = nextToken();
        if(!isBoolean(next)){
            processError("boolean expected after \"not\"");
        }
        val = (next == "true") ? "false" : "true"; // Flip the value
        type = BOOLEAN;
    }
    else {
        // Case 3: Simple Literal (0, true) OR Non-Key-Id (big)
        if(isInteger(y) || isBoolean(y)){
            // Subcase 3a: Literal (e.g., "0", "true")
            type = isInteger(y) ? INTEGER : BOOLEAN;
            val = y;
        } else if (isNonKeyId(y)) {
            // Subcase 3b: Existing Constant Name (e.g., "big")
            type = whichType(y);
            val = whichValue(y);
        } else {
             processError("token to right of \"=\" illegal");
        }
    }

    // 4. Expect and process semicolon
    if(nextToken() != ";"){
        processError("semicolon expected");
    }

    // 5. Final Type check
    if(type != INTEGER && type != BOOLEAN){
        processError("data type of token on the right-hand side must be INTEGER or BOOLEAN");
    }

    // 6. Insert into Symbol Table
    insert(x, type, CONSTANT, val, YES, 1);

    // 7. Advance token and check for recursion or block end
    token = nextToken();
    if(token != "begin" && token != "var" && !isNonKeyId(token)){
        processError("non-keyword identifier, \"begin\", or \"var\" expected");
    }

    if(isNonKeyId(token)){
        constStmts();   // recursive call
    }
}

void Compiler::varStmts(){      // stage 0, prod 7
    std::string x, y;
    storeTypes varType;

    if(!isNonKeyId(token)){
        processError("non-keyword identifier expected");
    }

    x = ids();  // parse identifier list (comma-separated string)
    if(token != ":"){
        processError("\":\" expected");
    }

    token = nextToken();        // advance to type
    if(token != "integer" && token != "boolean"){
        processError("illegal type follows \":\"");
    }

    y = token; // "integer" or "boolean"
    varType = (y == "integer") ? INTEGER : BOOLEAN;

    token = nextToken(); // advance past type
    if(token != ";"){
        processError("semicolon expected");
    }

    // Must use enums from stage0.h
    insert(x, varType, VARIABLE, "", YES, 1);

    token = nextToken();        // advance to next decl or block
    if(token != "begin" && !isNonKeyId(token)){
        processError("non-keyword identifier or \"begin\" expected");
    }

    if(isNonKeyId(token)){
        varStmts();     // recursive call for add decl
    }
}

string Compiler::ids(){         // stage 0, prod 8
    std::string tempString;

    if(!isNonKeyId(token)){
        processError("non-keyword identifier expected");
    }

    tempString = token;

    token = nextToken();        // advance to next tok
    if(token == ","){
        token = nextToken();    // advance past comma
        if(!isNonKeyId(token)){
            processError("non-keyword identifier expected");
        }
        tempString = tempString + "," + ids();      // recursive call
    }

    return tempString;
}

/* ------------------------------------------------------
    Helper funcs for Pascallite lexicon
    ------------------------------------------------------ */

bool Compiler::isKeyword(string s) const{       // is s a keyword?
    return keywords.count(s);
}

bool Compiler::isSpecialSymbol(char c) const{  // is c a spec symb?
    return specialSymbols.count(c);
}

bool Compiler::isNonKeyId(string s) const{      // is s a non_key_id?
    if(s.empty() || !std::islower(static_cast<unsigned char>(s[0]))) return false;

    for(char c : s){
        if(!std::islower(static_cast<unsigned char>(c)) && !std::isdigit(static_cast<unsigned char>(c)) && c != '_') return false;
        }

    return !isKeyword(s);
}

bool Compiler::isInteger(string s) const { // is s an int?
    if(s.empty()) return false;
    size_t i = (s[0] == '+' || s[0] == '-') ? 1 : 0;

    for(; i < s.size(); ++i){
        if(!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }

    // there must be at least one digit
    return ( (s[0] == '+' || s[0] == '-') ? s.size() > 1 : s.size() > 0 );
}

bool Compiler::isBoolean(string s) const{      // is s a bool?
    return s == "true" || s == "false";
}

bool Compiler::isLiteral(string s) const{      // is s a lit?
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

void Compiler::code(string op, string operand1, string operand2){
    if(op == "program"){
        emitPrologue(operand1);
    } else if(op == "end"){
        emitEpilogue();
    } else{
        processError("compiler error: illegal arguments to code()");
    }
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

/////////////////////////////////////////////////////////////////////////////





