// Serena Reese and Amiran Fields - CS 4301 - Stage 0

/*
stage0.cpp
- Completes the implementation of Stage 0 for the CS4301 Pascallite compiler project
- Follows the header and pseudocode supplied in our course's materials
- Most of the processing is performed by the parser
- Main is a simple program as a result
*/

/*
Stage 0 directions:
1. Check stage0.h for declared methods
2. Define declared methods here in this file, using the pseucode in PDF Overall Compiler Structure - Stage 0
3. Mark TODO where we want to refine the behavior during Stage 1
*/

/*
To compile:
1. make stage0
*/

#include <stage0.h> // iostream, fstream, string, map, namespace, SymbolTable

/////////////////////////////////////////////////////////////////////////////

/* ------------------------------------------------------
   Compiler class declared in stage0.h, now define its funcs
   ------------------------------------------------------ */

Compiler::Compiler(char **argv){        // constructor
    sourceFile.open(argv[1]);
    listingFile.open(argv[2]);
    objectFile.open(argv[3]);
}

Compiler::Compiler(){   // destructor
    sourceFile.close();
    listingFile.close();
    objectFile.close();
}

void Compiler::createListingHeader(){
    // line no. & src stm aligned under headings
    std::cout << "STAGE0: " << name << ", " << getTime.getTime() << "\n";
    std::cout << std::left << std::setw(10) << "LINE NO: " << "SOURCE STATEMENT" << "\n";
}

void Compiler::parser(){
    nextChar(); // ch must be initialized to 1st char of source file

    if(nextToken() != "program"){
        // 1st effect of nextToken() - var token assigned val of next token
        // 2nd effect = next token read from srcFile to assign, val returned = next token
        processError("keyword \"program\" expected");
    }

    prog();     // parser implements grammar rules, calling 1st rule
}

void Compiler::createListingTrailer(){  // print compilation terminated + no. errs encountered
    std::cout << "COMPILATION TERMINATED, " << errorCount << "ERRORS ENCOUNTERED" << std::endl;

    if(listingFile.is_open()){
        listingFile << "COMPILATION TERMINATED, " << errorCount << "ERRORS ENCOUNTERED" << std::endl;
    }
}

/* ------------------------------------------------------
   Methods implementing grammar prods
   ------------------------------------------------------ */

void Compiler::prog(){  // stage0, prod 1
    // tok = "program"
    if(token != "program"){
        processError("keyword \"program\" expected");
    }

    progStmt();
    if(token == "const"){
        consts();
    }
    if(token == "var"){
        vars();
    }
    if(token != "begin"){
        processError("keyword \"begin\" expected");
    }

    beginEndStmt();
    if(token != END_OF_FILE){
        processError("no text may follow \"end\");
    }
}

void progStmt(){        // stage 0, prod 2
    std::string x;

    if(token != "program"){
        processError("keyword \"program\" expected");
    }

    x = nextToken();    // get program name
    if(!isNonKeyId(x)){
        processError("program name expected");
    }

    token = nextToken();        // expect semicolon
    if(token != ";"){
        processError("semicolon expected");
    }

    token = nextToken();        // advance to next tok
    code("program", x);
    insert(x, "PROG_NAME", "CONSTANT", x, "NO", 0);
}

void consts(){  // stage0, prod 3
    if(token != "const"){
        processError("keyword \"const\" expected");
    }

    std::string next = nextToken();     // advance to next tok
    if(!isNonKeyId(next)){
        processError("non-keyword identifier must follow \"const\"");
    }

    constStmts();       // process const declars
}

void vars(){    // stage 0, prod 4
    if(token != "var"){
        processError("keyword \"var\" expected");
    }

    std::string next = nextToken();     // advance to next tok
    if(!isNonKeyId(next)){
        processError("non-keyword identifier must follow \"var\"");
    }

    varStmts(); // process var declars
}

void beginEndStmt(){    // stage 0, prod 5
    if(token != "begin"){
        processError("keyword \"begin\" expected");
    }

    token = nextToken();        // advance to next tok
    if(token != "end"){
        processError("keyword \"end\" expected");
    }

    token = nextToken();        // advance to next tok
    if(token != "."){
        processError("period expected");
    }

    token = nextToken();        // final advance
    code("end", ".");
}

void constStmts(){      // stage 0, prod 6
    std::string x, y;

    if(!isNonKeyId(token)){
        processError("non-keyword identifier expected");
    }

    x = token;
    if(nextToken() != "="){
        processError("\"=\" expected");
    }

    y = nextToken();
    if(y != "+" && y != "-" && y != "not" && !isNonKeyId(y) && y != "true" && y != "false" && !isInteger(y)){
        processError("token to right of "/=/" illegal");
    }

    if(y == "+" || y == "-"){
        std::string sign = y;
        std::string next = nextToken();
        if(!isInteger(next)){
            processError("integer expected after sign");
        }
        y = sign + next;
    } else if(y == "not"){
        std::string next = nextToken();
        if(!isBoolean(next)){
            processError("boolean expected after \"not\"");
        }
        y = (next == "true") ? "false" : "true";
    }

    if(nextToken() != ";"){
        processError("semicolon expected");
    }

    std::string type = whichType(y);
    if(type != "INTEGER" && type != "BOOLEAN"){
        processError("data type of token on the right-hand side must be INTEGER or BOOLEAN");
    }

    insert(x, type, "CONSTANT", whichValue(y), "YES", 1);

    x = nextToken();
    if(x != "begin" && x != "var" && !isNonKeyId(x)){
        processError("non-keyword identifier, \"begin\", or \"var\" expected");
    }

    if(isNonKeyId(x)){
        constStmts();   // recursive call
    }
}

void varStmts(){        // stage 0, prod 7
    std::string x, y;

    if(!isNonKeyId(token){
        processError("non-keyword identifier expected");
    }

    x = ids();  // parse identifier list
    if(token != ":"){
        processError("\":\" expected");
    }

    token = nextToken();        // advance to type
    if(token != "integer" && token != "boolean"){
        processError("illegal type follows \":\"");
    }

    y = token;
    if(token != ";"){
        processError("semicolon expected");
    }

    insert(x, y, "VARIABLE", "", "YES", 1);

    token = nextToken();        // advance to next decl or block
    if(token != "begin" && !isNonKeyId(token)){
        processError("non-keyword identifier or \"begin\" expected");
    }

    if(isNonKeyId(token)){
        varStmts();     // recursive call for add decl
    }
}

string ids(){   // stage 0, prod 8
    std::string temp, tempString;

    if(!isNonKeyId(token)){
        processError("non-keyword identifier expected");
    }

    tempString = token;
    temp = token;

    token = nextToken();        // advance to next tok
    if(token == ","){
        token = nextToken();    // advance past comma
        if(!isNonKeyId(token)){
            processError("non-keyword identifier expected");
        }
        tempString = temp + "," + ids();        // recursive call
    }

    return tempString;
}

/* ------------------------------------------------------
   Helper funcs for Pascallite lexicon
   ------------------------------------------------------ */

bool isKeyword(string s) const{ // is s a keyword?
    ...
}

bool isSpecialSymbol(char c) const{     // is c a spec symb?
    ...
}

bool isNonKeyId(string s) const{        // is s a non_key_id?
    ...
}

bool isInteger(string s) const{ // is s an int?
    ...
}

bool isBoolean(string s) const{ // is s a bool?
    ...
}

bool isLiteral(string s) const{ // is s a lit?
    ...
}

/* ------------------------------------------------------
   Action routines
   ------------------------------------------------------ */

void insert(string externalName, storeTypes inType, modes inMode, string inValue, allocation inAlloc, int inUnites){
    ...
}

storeTypes whichType(string name){      // which data type does name have?
    ...
}

string whichValue(string name){ // which value does name have?
    ...
}

void code(string op, string operand1 = "", string operand2 = ""){
    ...
}

/* ------------------------------------------------------
   Emit funcs
   ------------------------------------------------------ */

void emit(string label = "", string instruction = "", string operands = "", string comment = ""){
    ...
}

void emitPrologue(string progName, string = ""){
    ...
}

void emitEpilogue(string = "", string = ""){
    ...
}

void emitStorage(){
    ...
}

/* ------------------------------------------------------
   Lexical routines
   ------------------------------------------------------ */

char nextChar(){        // returns next char or END_OF_FILE marker
    ...
}

string nextToken(){     // returns next tok or END_OF_FILE marker
    ...
}

/* ------------------------------------------------------
   Other routines
   ------------------------------------------------------ */

string genInternalName(storeTypes stype) const{
    ...
}

void processError(string err){  // output err to listingFile, call exit
    if(listingFile.is_open()){
        listingFile << "ERROR: " << err << std::endl;
    }

    std::cerr << "Fatal error: " << err << std::endl;
    exit(EXIT_FAILURE);
}

/////////////////////////////////////////////////////////////////////////////
