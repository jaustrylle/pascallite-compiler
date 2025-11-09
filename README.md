# Pascallite Compiler
## Angelo State University (CS4301) under Dr. Mark Motl

Stage 0 is the first stage of our Pascallite compiler and includes only the main routine and its interfaces with its major components. Stage 0, 1 and 2 are all organized as a translation grammar processor. The grammar it associates with is LL(1), so the processor generates a leftmost derivation of programs without backtracking. The grammar itself includes action symbols to build the symbol table, omitting the selection sets.

Our professor has already supplied:
1. A complete LL1 grammar
2. The translation actions

These include:
1. The productions (PROG, PROG_STMT, CONSTS, VARS, BEGIN_END_STMT, CONST_STMTS, VAR_STMTS, IDS, TYPE, LIT and BOOLEAN)
2. The tokens and lexical rules - nextChar, nextToken behavior and character classes
3. The action routines we call from the parser - insert, whichType, whichValue and code
4. They symbol table layout
5. The Compiler class interface (methods, members, emit, emitPrologue, emitEpilogue and emitStorage)
6. Pseudocode for every parser routine and the lexical scanner

-----------------------------------------------------------

STAGE 0
Stage 0 Compiler: parses and validates Pascallite program structure, constants and variables

✅ What Stage 0 supports
...

📦 How to build and run
- stage0.h - declares the parser methods
- stage0.cpp - implements the parser functions, enforcing the grammar and calling translation actions

🧪 Sample input/output
...

-----------------------------------------------------------
🧭 Phase 1: Define the Language
✅ Step 1: Specify Pascallite Grammar
- Write out the grammar in BNF or EBNF format.
- Include constructs like:
- Variable declarations: var x : integer;
- Assignments: x := 5;
- Control flow: if x > 0 then ..., while x < 10 do ...
- I/O: read(x);, write(x);
📁 Output: grammar.txt

🧪 Phase 2: Lexical Analysis
✅ Step 2: Build a Tokenizer
- Scan source code and convert it into tokens.
- Recognize keywords (var, begin, end), identifiers, numbers, operators (:=, +, -, etc.)
🛠️ Tools: Regex or hand-written scanner
📁 Output: tokens.txt

🌳 Phase 3: Syntax Analysis
✅ Step 3: Build a Parser
- Use recursive descent or a parser generator (e.g., Bison).
- Validate syntax and build an Abstract Syntax Tree (AST).
📁 Output: ast.json or in-memory tree

STAGE 2
🧠 Phase 4: Semantic Analysis
✅ Step 4: Build a Symbol Table
- Track declared variables, types, and scopes.
- Check for undeclared variables, type mismatches, etc.
📁 Output: symbol_table.txt

🧬 Phase 5: Intermediate Representation
✅ Step 5: Generate IR
- Convert AST into a lower-level representation like three-address code.
- Example: x := y + 5 → t1 = y + 5, x = t1
📁 Output: ir.txt

⚙️ Phase 6: Code Generation
✅ Step 6: Emit Assembly
- Translate IR into RAMM or x86 assembly.
- Handle:
- Variable storage
- Arithmetic
- Branching
- I/O
📁 Output: program.asm

STAGE 3
🧪 Phase 7: Testing and Debugging
✅ Step 7: Build a Test Suite
- Write Pascallite programs that test:
- Arithmetic
- Control flow
- Nested blocks
- I/O
📁 Output: tests/

🧰 Optional Enhancements
- Add optimization passes (e.g., constant folding)
- Support functions/procedures
- Build a GUI or web interface for compiling
