# PascalliteCompiler
Translates Pascallite into RAMM assembly; handles memory, boolean logic, arithmetic and control flow

- Define the grammar: Use BNF or EBNF for Pascallite
- Write a parser: Recursive descent or use tools like Flex/Bison
- Build an AST: Represent control flow, expressions, etc.
- Generate assembly: Translate AST nodes into x86 instructions
- Assemble and link: Use NASM or GAS to produce executables

STAGE 0
Stage 0 Compiler: parses and validates Pascallite program structure, constants, and variables

Describe here:
- ✅ What Stage 0 supports
- 📦 How to build and run
- 🧪 Sample input/output

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
