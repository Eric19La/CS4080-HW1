//> Scanning on Demand compiler-c
#include <stdio.h>
//> Compiling Expressions compiler-include-stdlib
#include <stdlib.h>
//< Compiling Expressions compiler-include-stdlib
//> Local Variables compiler-include-string
#include <string.h>
//< Local Variables compiler-include-string

#include "common.h"
#include "compiler.h"
//> Garbage Collection compiler-include-memory
#include "memory.h"
//> Challenge 21.1 include-table
#include "table.h"
//< Challenge 21.1 include-table
//< Garbage Collection compiler-include-memory
#include "scanner.h"
//> Compiling Expressions include-debug

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif
//< Compiling Expressions include-debug
//> Compiling Expressions parser

typedef struct {
  Token current;
  Token previous;
//> had-error-field
  bool hadError;
//< had-error-field
//> panic-mode-field
  bool panicMode;
//< panic-mode-field
} Parser;
//> precedence

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
//> Challenge 17.3 prec-conditional
  PREC_CONDITIONAL, // ?:
//< Challenge 17.3 prec-conditional
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;
//< precedence
//> parse-fn-type

//< parse-fn-type
/* Compiling Expressions parse-fn-type < Global Variables parse-fn-type
typedef void (*ParseFn)();
*/
//> Global Variables parse-fn-type
typedef void (*ParseFn)(bool canAssign);
//< Global Variables parse-fn-type
//> parse-rule

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;
//< parse-rule
//> Local Variables local-struct

typedef struct {
  Token name;
  int depth;
//> Closures is-captured-field
  bool isCaptured;
//< Closures is-captured-field
//> Challenge 22.1 shadowed-slot-field
  // If some outer-scope local with this SAME name was already in
  // scope when this one was declared, its slot index; -1 otherwise.
  // Lets endScope() restore Compiler.localSlots's entry for this name
  // back to whatever it shadowed in O(1) when this local goes out of
  // scope, instead of re-scanning locals[] to find what's now visible.
  int shadowedSlot;
//< Challenge 22.1 shadowed-slot-field
//> Challenge 22.3 local-is-const-field
  bool isConst;
//< Challenge 22.3 local-is-const-field
} Local;
//< Local Variables local-struct
//> Closures upvalue-struct
typedef struct {
//> Challenge 22.4 upvalue-index-widened
  // Was uint8_t -- widened so capturing a local beyond slot 255 (now
  // possible, see OP_GET_LOCAL_LONG) doesn't get silently truncated
  // into capturing the WRONG local instead of just erroring out.
  int index;
//< Challenge 22.4 upvalue-index-widened
  bool isLocal;
} Upvalue;
//< Closures upvalue-struct
//> Calls and Functions function-type-enum
typedef enum {
  TYPE_FUNCTION,
//> Methods and Initializers initializer-type-enum
  TYPE_INITIALIZER,
//< Methods and Initializers initializer-type-enum
//> Methods and Initializers method-type-enum
  TYPE_METHOD,
//< Methods and Initializers method-type-enum
  TYPE_SCRIPT
} FunctionType;
//< Calls and Functions function-type-enum
//> Local Variables compiler-struct

/* Local Variables compiler-struct < Calls and Functions enclosing-field
typedef struct {
*/
//> Calls and Functions enclosing-field
typedef struct Compiler {
  struct Compiler* enclosing;
//< Calls and Functions enclosing-field
//> Calls and Functions function-fields
  ObjFunction* function;
  FunctionType type;

//< Calls and Functions function-fields
//> Challenge 22.4 growable-locals
  // Was a fixed Local locals[UINT8_COUNT] array -- grown on demand
  // instead, so a function isn't stuck at a 256-local ceiling.
  Local* locals;
  int localCount;
  int localCapacity;
//< Challenge 22.4 growable-locals
//> Closures upvalues-array
  Upvalue upvalues[UINT8_COUNT];
//< Closures upvalues-array
  int scopeDepth;
//> Challenge 21.1 string-constants-field
  // Caches name -> constant-table-index for THIS function's chunk, so
  // identifierConstant() doesn't add a fresh constant every time the
  // same name is referenced again. Per-compiler (not global), since
  // constant indices are relative to each function's own chunk.
  Table stringConstants;
//< Challenge 21.1 string-constants-field
//> Challenge 22.1 local-slots-field
  // Maps a local's name to the slot index of whichever one of that
  // name is currently visible (innermost wins), so resolveLocal() is
  // an O(1) average hash lookup instead of a linear scan backward
  // through locals[].
  Table localSlots;
//< Challenge 22.1 local-slots-field
} Compiler;
//< Local Variables compiler-struct
//> Methods and Initializers class-compiler-struct

typedef struct ClassCompiler {
  struct ClassCompiler* enclosing;
//> Superclasses has-superclass
  bool hasSuperclass;
//< Superclasses has-superclass
} ClassCompiler;
//< Methods and Initializers class-compiler-struct

Parser parser;
//< Compiling Expressions parser
//> Local Variables current-compiler
Compiler* current = NULL;
//< Local Variables current-compiler
//> Methods and Initializers current-class
ClassCompiler* currentClass = NULL;
//< Methods and Initializers current-class
//> Compiling Expressions compiling-chunk
/* Compiling Expressions compiling-chunk < Calls and Functions current-chunk
Chunk* compilingChunk;

static Chunk* currentChunk() {
  return compilingChunk;
}
*/
//> Calls and Functions current-chunk

static Chunk* currentChunk() {
  return &current->function->chunk;
}
//< Calls and Functions current-chunk

//< Compiling Expressions compiling-chunk
//> Compiling Expressions error-at
static void errorAt(Token* token, const char* message) {
//> check-panic-mode
  if (parser.panicMode) return;
//< check-panic-mode
//> set-panic-mode
  parser.panicMode = true;
//< set-panic-mode
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing.
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = true;
}
//< Compiling Expressions error-at
//> Compiling Expressions error
static void error(const char* message) {
  errorAt(&parser.previous, message);
}
//< Compiling Expressions error
//> Compiling Expressions error-at-current
static void errorAtCurrent(const char* message) {
  errorAt(&parser.current, message);
}
//< Compiling Expressions error-at-current
//> Compiling Expressions advance

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) break;

    errorAtCurrent(parser.current.start);
  }
}
//< Compiling Expressions advance
//> Compiling Expressions consume
static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}
//< Compiling Expressions consume
//> Global Variables check
static bool check(TokenType type) {
  return parser.current.type == type;
}
//< Global Variables check
//> Global Variables match
static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}
//< Global Variables match
//> Compiling Expressions emit-byte
static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}
//< Compiling Expressions emit-byte
//> Compiling Expressions emit-bytes
static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}
//< Compiling Expressions emit-bytes
//> Challenge 22.4 emit-local-long
// Emits one of OP_GET_LOCAL_LONG/OP_SET_LOCAL_LONG plus its 24-bit
// big-endian slot operand -- same convention writeConstant() already
// uses for OP_CONSTANT_LONG.
static void emitLocalLong(uint8_t op, int slot) {
  emitByte(op);
  emitByte((slot >> 16) & 0xff);
  emitByte((slot >> 8) & 0xff);
  emitByte(slot & 0xff);
}
//< Challenge 22.4 emit-local-long
//> Jumping Back and Forth emit-loop
static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) error("Loop body too large.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}
//< Jumping Back and Forth emit-loop
//> Jumping Back and Forth emit-jump
static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}
//< Jumping Back and Forth emit-jump
//> Compiling Expressions emit-return
static void emitReturn() {
/* Calls and Functions return-nil < Methods and Initializers return-this
  emitByte(OP_NIL);
*/
//> Methods and Initializers return-this
  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
  } else {
    emitByte(OP_NIL);
  }

//< Methods and Initializers return-this
  emitByte(OP_RETURN);
}
//< Compiling Expressions emit-return
//> Compiling Expressions make-constant
static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}
//< Compiling Expressions make-constant
//> Compiling Expressions emit-constant
static void emitConstant(Value value) {
//> Challenge 14.2 emit-constant-uses-write-constant
  // writeConstant() picks OP_CONSTANT or OP_CONSTANT_LONG for us,
  // lifting the 256-constant ceiling makeConstant()/OP_CONSTANT alone
  // would hit for number/string literals (the common case).
  writeConstant(currentChunk(), value, parser.previous.line);
//< Challenge 14.2 emit-constant-uses-write-constant
}
//< Compiling Expressions emit-constant
//> Jumping Back and Forth patch-jump
static void patchJump(int offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}
//< Jumping Back and Forth patch-jump
//> Challenge 22.4 grow-locals
// Grows compiler->locals[] by one slot if it's currently full. Used
// both by initCompiler() (for the implicit slot-0 local) and
// addLocal() -- the two places that write a new Local into the array.
static void growLocals(Compiler* compiler) {
  if (compiler->localCapacity < compiler->localCount + 1) {
    int oldCapacity = compiler->localCapacity;
    compiler->localCapacity = GROW_CAPACITY(oldCapacity);
    compiler->locals = GROW_ARRAY(Local, compiler->locals,
                                   oldCapacity, compiler->localCapacity);
  }
}
//< Challenge 22.4 grow-locals
//> Local Variables init-compiler
/* Local Variables init-compiler < Calls and Functions init-compiler
static void initCompiler(Compiler* compiler) {
*/
//> Calls and Functions init-compiler
static void initCompiler(Compiler* compiler, FunctionType type) {
//> store-enclosing
  compiler->enclosing = current;
//< store-enclosing
  compiler->function = NULL;
  compiler->type = type;
//< Calls and Functions init-compiler
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
//> Challenge 22.4 init-growable-locals
  compiler->locals = NULL;
  compiler->localCapacity = 0;
//< Challenge 22.4 init-growable-locals
//> Challenge 21.1 init-string-constants
  initTable(&compiler->stringConstants);
//< Challenge 21.1 init-string-constants
//> Challenge 22.1 init-local-slots
  initTable(&compiler->localSlots);
//< Challenge 22.1 init-local-slots
//> Calls and Functions init-function
  compiler->function = newFunction();
//< Calls and Functions init-function
  current = compiler;
//> Calls and Functions init-function-name
  if (type != TYPE_SCRIPT) {
    current->function->name = copyString(parser.previous.start,
                                         parser.previous.length);
  }
//< Calls and Functions init-function-name
//> Calls and Functions init-function-slot

  growLocals(current); // Challenge 22.4
  Local* local = &current->locals[current->localCount++];
  local->depth = 0;
//> Closures init-zero-local-is-captured
  local->isCaptured = false;
//< Closures init-zero-local-is-captured
/* Calls and Functions init-function-slot < Methods and Initializers slot-zero
  local->name.start = "";
  local->name.length = 0;
*/
//> Methods and Initializers slot-zero
  if (type != TYPE_FUNCTION) {
    local->name.start = "this";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
//< Methods and Initializers slot-zero
//< Calls and Functions init-function-slot
//> Challenge 22.1 register-slot-zero
  // This slot is written directly (not through addLocal()), so it has
  // to register itself in localSlots the same way addLocal() would --
  // otherwise a method body's references to "this" couldn't resolve.
  local->shadowedSlot = -1;
  tableSet(&compiler->localSlots,
           OBJ_VAL(copyString(local->name.start, local->name.length)),
           NUMBER_VAL(0));
//< Challenge 22.1 register-slot-zero
}
//< Local Variables init-compiler
//> Compiling Expressions end-compiler
/* Compiling Expressions end-compiler < Calls and Functions end-compiler
static void endCompiler() {
*/
//> Calls and Functions end-compiler
static ObjFunction* endCompiler() {
//< Calls and Functions end-compiler
  emitReturn();
//> Calls and Functions end-function
  ObjFunction* function = current->function;

//< Calls and Functions end-function
//> dump-chunk
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
/* Compiling Expressions dump-chunk < Calls and Functions disassemble-end
    disassembleChunk(currentChunk(), "code");
*/
//> Calls and Functions disassemble-end
    disassembleChunk(currentChunk(), function->name != NULL
        ? function->name->chars : "<script>");
//< Calls and Functions disassemble-end
  }
#endif
//< dump-chunk
//> Calls and Functions return-function

//> Challenge 21.1 free-string-constants
  freeTable(&current->stringConstants);
//< Challenge 21.1 free-string-constants
//> Challenge 22.1 free-local-slots
  freeTable(&current->localSlots);
//< Challenge 22.1 free-local-slots
//> Challenge 22.4 free-growable-locals
  FREE_ARRAY(Local, current->locals, current->localCapacity);
//< Challenge 22.4 free-growable-locals
//> restore-enclosing
  current = current->enclosing;
//< restore-enclosing
  return function;
//< Calls and Functions return-function
}
//< Compiling Expressions end-compiler
//> Local Variables begin-scope
static void beginScope() {
  current->scopeDepth++;
}
//< Local Variables begin-scope
//> Local Variables end-scope
static void endScope() {
  current->scopeDepth--;
//> pop-locals

  while (current->localCount > 0 &&
         current->locals[current->localCount - 1].depth >
            current->scopeDepth) {
    Local* local = &current->locals[current->localCount - 1]; // Challenge 22.1
/* Local Variables pop-locals < Closures end-scope
    emitByte(OP_POP);
*/
//> Closures end-scope
    if (local->isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
//< Closures end-scope
//> Challenge 22.1 restore-shadowed-slot
    // This local is leaving scope -- un-hide whatever name it was
    // shadowing (or, if it wasn't shadowing anything, remove the name
    // from the table entirely so a later reference correctly resolves
    // to a global/upvalue instead of this now-dead slot).
    ObjString* nameString = copyString(local->name.start, local->name.length);
    if (local->shadowedSlot == -1) {
      tableDelete(&current->localSlots, OBJ_VAL(nameString));
    } else {
      tableSet(&current->localSlots, OBJ_VAL(nameString),
               NUMBER_VAL(local->shadowedSlot));
    }
//< Challenge 22.1 restore-shadowed-slot
    current->localCount--;
  }
//< pop-locals
}
//< Local Variables end-scope
//> Compiling Expressions forward-declarations

static void expression();
//> Global Variables forward-declarations
static void statement();
static void declaration();
//< Global Variables forward-declarations
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

//< Compiling Expressions forward-declarations
//> Global Variables identifier-constant
//> Challenge 21.1 identifier-constant-dedup
static uint8_t identifierConstant(Token* name) {
  ObjString* string = copyString(name->start, name->length);

  // Already have this exact name in THIS chunk's constant table?
  // Reuse that slot instead of adding a duplicate.
  Value indexValue;
  if (tableGet(&current->stringConstants, OBJ_VAL(string), &indexValue)) {
    return (uint8_t)AS_NUMBER(indexValue);
  }

  uint8_t index = makeConstant(OBJ_VAL(string));
  tableSet(&current->stringConstants, OBJ_VAL(string), NUMBER_VAL(index));
  return index;
}
//< Challenge 21.1 identifier-constant-dedup
//< Global Variables identifier-constant
//> Challenge 21.2 global-variable
// Resolves a global variable's *name* to its *slot* in the VM's global
// array (vm.globals), instead of stashing the name as a chunk
// constant. OP_GET_GLOBAL/OP_SET_GLOBAL/OP_DEFINE_GLOBAL operands are
// now direct array indices, so the runtime never has to hash or
// compare strings to read or write a global.
//
// Deliberately kept at a single byte, same as OP_GET_LOCAL's slot
// operand -- this caps the whole program at 256 distinct global
// names, matching the existing (pre-existing, unrelated to this
// challenge) 256-local-per-function cap rather than introducing a new
// kind of limit.
static uint8_t globalVariable(Token* name) {
  ObjString* string = copyString(name->start, name->length);
  int slot = ensureGlobalSlot(string);
  if (slot > UINT8_MAX) {
    error("Too many global variables.");
    return 0;
  }
  return (uint8_t)slot;
}
//< Challenge 21.2 global-variable
//> Local Variables identifiers-equal
static bool identifiersEqual(Token* a, Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}
//< Local Variables identifiers-equal
//> Local Variables resolve-local
//> Challenge 22.1 resolve-local-hashed
// Was a linear scan backward through compiler->locals[]. localSlots
// (kept up to date by addLocal()/endScope()) always maps a name to
// whichever local currently "owns" it -- the innermost one in scope,
// exactly what the backward scan used to find first -- so a single
// hash lookup replaces the whole scan.
static int resolveLocal(Compiler* compiler, Token* name) {
  ObjString* nameString = copyString(name->start, name->length);
  Value slotValue;
  if (!tableGet(&compiler->localSlots, OBJ_VAL(nameString), &slotValue)) {
    return -1;
  }

  int slot = (int)AS_NUMBER(slotValue);
//> own-initializer-error
  if (compiler->locals[slot].depth == -1) {
    error("Can't read local variable in its own initializer.");
  }
//< own-initializer-error
  return slot;
}
//< Challenge 22.1 resolve-local-hashed
//< Local Variables resolve-local
//> Closures add-upvalue
static int addUpvalue(Compiler* compiler, int index, // Challenge 22.4
                      bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;
//> existing-upvalue

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

//< existing-upvalue
//> too-many-upvalues
  if (upvalueCount == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

//< too-many-upvalues
  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}
//< Closures add-upvalue
//> Closures resolve-upvalue
static int resolveUpvalue(Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
//> mark-local-captured
    compiler->enclosing->locals[local].isCaptured = true;
//< mark-local-captured
    return addUpvalue(compiler, local, true); // Challenge 22.4: no truncating cast
  }

//> resolve-upvalue-recurse
  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t)upvalue, false);
  }
  
//< resolve-upvalue-recurse
  return -1;
}
//> Challenge 22.3 is-upvalue-const
// Walks the same enclosing-compiler chain resolveUpvalue() just walked
// to find this upvalue, to answer "was the local it ultimately
// bottoms out at declared const?" -- needed so assigning to a const
// through a closure is caught too, not just a direct reference.
static bool isUpvalueConst(Compiler* compiler, int upvalueIndex) {
  Upvalue* upvalue = &compiler->upvalues[upvalueIndex];
  if (upvalue->isLocal) {
    return compiler->enclosing->locals[upvalue->index].isConst;
  }
  return isUpvalueConst(compiler->enclosing, upvalue->index);
}
//< Challenge 22.3 is-upvalue-const
//< Closures resolve-upvalue
//> Local Variables add-local
static void addLocal(Token name, bool isConst) { // Challenge 22.3
//> Challenge 22.4 too-many-locals
  // Was a hard cap at UINT8_COUNT (256), matching OP_GET_LOCAL's old
  // 1-byte-only operand. Now locals[] grows on demand and
  // OP_GET_LOCAL_LONG/OP_SET_LOCAL_LONG (24-bit operand) take over
  // past slot 255, so the only real ceiling left is that operand's
  // own range -- a limit so large in practice it's purely defensive.
  if (current->localCount == 0x1000000) {
    error("Too many local variables in function.");
    return;
  }

//< Challenge 22.4 too-many-locals
  growLocals(current); // Challenge 22.4
  int slot = current->localCount;
  Local* local = &current->locals[current->localCount++];
  local->name = name;
/* Local Variables add-local < Local Variables declare-undefined
  local->depth = current->scopeDepth;
*/
//> declare-undefined
  local->depth = -1;
//< declare-undefined
//> Closures init-is-captured
  local->isCaptured = false;
//< Closures init-is-captured
  local->isConst = isConst; // Challenge 22.3
//> Challenge 22.1 add-local-slot
  ObjString* nameString = copyString(name.start, name.length);
  Value shadowed;
  local->shadowedSlot = tableGet(&current->localSlots, OBJ_VAL(nameString),
                                  &shadowed)
      ? (int)AS_NUMBER(shadowed) : -1;
  tableSet(&current->localSlots, OBJ_VAL(nameString), NUMBER_VAL(slot));
//< Challenge 22.1 add-local-slot
}
//< Local Variables add-local
//> Local Variables declare-variable
static void declareVariable(bool isConst) { // Challenge 22.3
  if (current->scopeDepth == 0) return;

  Token* name = &parser.previous;
//> existing-in-scope
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break; // [negative]
    }

    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

//< existing-in-scope
  addLocal(*name, isConst);
}
//< Local Variables declare-variable
//> Global Variables parse-variable
static uint8_t parseVariable(const char* errorMessage, bool isConst) { // Challenge 22.3
  consume(TOKEN_IDENTIFIER, errorMessage);
//> Local Variables parse-local

  declareVariable(isConst); // Challenge 22.3
  if (current->scopeDepth > 0) return 0;

//< Local Variables parse-local
  uint8_t slot = globalVariable(&parser.previous); // Challenge 21.2
//> Challenge 22.3 set-global-is-const
  // Only a declaration flips this -- a plain reference to this name
  // elsewhere (through namedVariable()'s own call to globalVariable())
  // never reaches this line, so this can't accidentally un-const (or
  // const-ify) a slot just by reading it.
  vm.globals[slot].isConst = isConst;
//< Challenge 22.3 set-global-is-const
  return slot;
}
//< Global Variables parse-variable
//> Local Variables mark-initialized
static void markInitialized() {
//> Calls and Functions check-depth
  if (current->scopeDepth == 0) return;
//< Calls and Functions check-depth
  current->locals[current->localCount - 1].depth =
      current->scopeDepth;
}
//< Local Variables mark-initialized
//> Global Variables define-variable
static void defineVariable(uint8_t global) {
//> Local Variables define-variable
  if (current->scopeDepth > 0) {
//> define-local
    markInitialized();
//< define-local
    return;
  }

//< Local Variables define-variable
  emitBytes(OP_DEFINE_GLOBAL, global);
}
//< Global Variables define-variable
//> Calls and Functions argument-list
static uint8_t argumentList() {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
//> arg-limit
      if (argCount == 255) {
        error("Can't have more than 255 arguments.");
      }
//< arg-limit
      argCount++;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}
//< Calls and Functions argument-list
//> Jumping Back and Forth and
static void and_(bool canAssign) {
  int endJump = emitJump(OP_JUMP_IF_FALSE);

  emitByte(OP_POP);
  parsePrecedence(PREC_AND);

  patchJump(endJump);
}
//< Jumping Back and Forth and
//> Compiling Expressions binary
/* Compiling Expressions binary < Global Variables binary
static void binary() {
*/
//> Global Variables binary
static void binary(bool canAssign) {
//< Global Variables binary
  TokenType operatorType = parser.previous.type;
  ParseRule* rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
//> Types of Values comparison-operators
    case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
    case TOKEN_GREATER:       emitByte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS:          emitByte(OP_LESS); break;
    case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
//< Types of Values comparison-operators
    case TOKEN_PLUS:          emitByte(OP_ADD); break;
    case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
    default: return; // Unreachable.
  }
}
//< Compiling Expressions binary
//> Calls and Functions compile-call
static void call(bool canAssign) {
  uint8_t argCount = argumentList();
  emitBytes(OP_CALL, argCount);
}
//< Calls and Functions compile-call
//> Classes and Instances compile-dot
static void dot(bool canAssign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  uint8_t name = identifierConstant(&parser.previous);

  if (canAssign && match(TOKEN_EQUAL)) {
    expression();
    emitBytes(OP_SET_PROPERTY, name);
//> Methods and Initializers parse-call
  } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    emitBytes(OP_INVOKE, name);
    emitByte(argCount);
//< Methods and Initializers parse-call
  } else {
    emitBytes(OP_GET_PROPERTY, name);
  }
}
//< Classes and Instances compile-dot
//> Types of Values parse-literal
/* Types of Values parse-literal < Global Variables parse-literal
static void literal() {
*/
//> Global Variables parse-literal
static void literal(bool canAssign) {
//< Global Variables parse-literal
  switch (parser.previous.type) {
    case TOKEN_FALSE: emitByte(OP_FALSE); break;
    case TOKEN_NIL: emitByte(OP_NIL); break;
    case TOKEN_TRUE: emitByte(OP_TRUE); break;
    default: return; // Unreachable.
  }
}
//< Types of Values parse-literal
//> Compiling Expressions grouping
/* Compiling Expressions grouping < Global Variables grouping
static void grouping() {
*/
//> Global Variables grouping
static void grouping(bool canAssign) {
//< Global Variables grouping
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}
//< Compiling Expressions grouping
/* Compiling Expressions number < Global Variables number
static void number() {
*/
//> Compiling Expressions number
//> Global Variables number
static void number(bool canAssign) {
//< Global Variables number
  double value = strtod(parser.previous.start, NULL);
/* Compiling Expressions number < Types of Values const-number-val
  emitConstant(value);
*/
//> Types of Values const-number-val
  emitConstant(NUMBER_VAL(value));
//< Types of Values const-number-val
}
//< Compiling Expressions number
//> Jumping Back and Forth or
static void or_(bool canAssign) {
  int elseJump = emitJump(OP_JUMP_IF_FALSE);
  int endJump = emitJump(OP_JUMP);

  patchJump(elseJump);
  emitByte(OP_POP);

  parsePrecedence(PREC_OR);
  patchJump(endJump);
}
//< Jumping Back and Forth or
/* Strings parse-string < Global Variables string
static void string() {
*/
//> Strings parse-string
//> Global Variables string
static void string(bool canAssign) {
//< Global Variables string
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1,
                                  parser.previous.length - 2)));
}
//< Strings parse-string
/* Global Variables read-named-variable < Global Variables named-variable-signature
static void namedVariable(Token name) {
*/
//> Global Variables named-variable-signature
static void namedVariable(Token name, bool canAssign) {
//< Global Variables named-variable-signature
/* Global Variables read-named-variable < Local Variables named-local
  uint8_t arg = identifierConstant(&name);
*/
//> Global Variables read-named-variable
//> Local Variables named-local
  uint8_t getOp, setOp;
  bool isConst; // Challenge 22.3
  bool isLocal = false; // Challenge 22.4
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
    isConst = current->locals[arg].isConst; // Challenge 22.3
    isLocal = true; // Challenge 22.4
//> Closures named-variable-upvalue
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
    isConst = isUpvalueConst(current, arg); // Challenge 22.3
//< Closures named-variable-upvalue
  } else {
    arg = globalVariable(&name); // Challenge 21.2
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
    isConst = vm.globals[arg].isConst; // Challenge 22.3
  }
//< Local Variables named-local
/* Global Variables read-named-variable < Global Variables named-variable
  emitBytes(OP_GET_GLOBAL, arg);
*/
//> named-variable

/* Global Variables named-variable < Global Variables named-variable-can-assign
  if (match(TOKEN_EQUAL)) {
*/
//> named-variable-can-assign
  if (canAssign && match(TOKEN_EQUAL)) {
//< named-variable-can-assign
//> Challenge 22.3 const-assign-error
    if (isConst) {
      error("Can't assign to a const variable.");
    }
//< Challenge 22.3 const-assign-error
    expression();
/* Global Variables named-variable < Local Variables emit-set
    emitBytes(OP_SET_GLOBAL, arg);
*/
//> Local Variables emit-set
//> Challenge 22.4 emit-set-long
    if (isLocal && arg > UINT8_MAX) {
      emitLocalLong(OP_SET_LOCAL_LONG, arg);
    } else {
      emitBytes(setOp, (uint8_t)arg);
    }
//< Challenge 22.4 emit-set-long
//< Local Variables emit-set
  } else {
/* Global Variables named-variable < Local Variables emit-get
    emitBytes(OP_GET_GLOBAL, arg);
*/
//> Local Variables emit-get
//> Challenge 22.4 emit-get-long
    if (isLocal && arg > UINT8_MAX) {
      emitLocalLong(OP_GET_LOCAL_LONG, arg);
    } else {
      emitBytes(getOp, (uint8_t)arg);
    }
//< Challenge 22.4 emit-get-long
//< Local Variables emit-get
  }
//< named-variable
}
//< Global Variables read-named-variable
/* Global Variables variable-without-assign < Global Variables variable
static void variable() {
  namedVariable(parser.previous);
}
*/
//> Global Variables variable
static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}
//< Global Variables variable
//> Superclasses synthetic-token
static Token syntheticToken(const char* text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}
//< Superclasses synthetic-token
//> Superclasses super
static void super_(bool canAssign) {
//> super-errors
  if (currentClass == NULL) {
    error("Can't use 'super' outside of a class.");
  } else if (!currentClass->hasSuperclass) {
    error("Can't use 'super' in a class with no superclass.");
  }

//< super-errors
  consume(TOKEN_DOT, "Expect '.' after 'super'.");
  consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
  uint8_t name = identifierConstant(&parser.previous);
//> super-get
  
  namedVariable(syntheticToken("this"), false);
/* Superclasses super-get < Superclasses super-invoke
  namedVariable(syntheticToken("super"), false);
  emitBytes(OP_GET_SUPER, name);
*/
//< super-get
//> super-invoke
  if (match(TOKEN_LEFT_PAREN)) {
    uint8_t argCount = argumentList();
    namedVariable(syntheticToken("super"), false);
    emitBytes(OP_SUPER_INVOKE, name);
    emitByte(argCount);
  } else {
    namedVariable(syntheticToken("super"), false);
    emitBytes(OP_GET_SUPER, name);
  }
//< super-invoke
}
//< Superclasses super
//> Methods and Initializers this
static void this_(bool canAssign) {
//> this-outside-class
  if (currentClass == NULL) {
    error("Can't use 'this' outside of a class.");
    return;
  }
  
//< this-outside-class
  variable(false);
} // [this]
//< Methods and Initializers this
//> Compiling Expressions unary
/* Compiling Expressions unary < Global Variables unary
static void unary() {
*/
//> Global Variables unary
static void unary(bool canAssign) {
//< Global Variables unary
  TokenType operatorType = parser.previous.type;

  // Compile the operand.
/* Compiling Expressions unary < Compiling Expressions unary-operand
  expression();
*/
//> unary-operand
  parsePrecedence(PREC_UNARY);
//< unary-operand

  // Emit the operator instruction.
  switch (operatorType) {
//> Types of Values compile-not
    case TOKEN_BANG: emitByte(OP_NOT); break;
//< Types of Values compile-not
    case TOKEN_MINUS: emitByte(OP_NEGATE); break;
    default: return; // Unreachable.
  }
}
//< Compiling Expressions unary
//> Challenge 17.3 ternary
static void ternary(bool canAssign) {
  // The condition has already been compiled -- it's the left operand
  // that caused this infix rule to fire on TOKEN_QUESTION.
  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP); // Discard the condition before the 'then' branch.

  // The branch between '?' and ':' is delimited by tokens, so it's
  // unambiguous to allow a full expression there, same as the operand
  // of a parenthesized group.
  parsePrecedence(PREC_ASSIGNMENT);

  int elseJump = emitJump(OP_JUMP);
  patchJump(thenJump);
  emitByte(OP_POP); // Discard the condition before the 'else' branch.

  consume(TOKEN_COLON, "Expect ':' after then branch of ternary expression.");
  // Recursing at PREC_CONDITIONAL itself (not +1) makes '?:' right-
  // associative, so a ? b : c ? d : e reads as a ? b : (c ? d : e).
  parsePrecedence(PREC_CONDITIONAL);

  patchJump(elseJump);
}
//< Challenge 17.3 ternary
//> Compiling Expressions rules
ParseRule rules[] = {
/* Compiling Expressions rules < Calls and Functions infix-left-paren
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
*/
//> Calls and Functions infix-left-paren
  [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
//< Calls and Functions infix-left-paren
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, // [big]
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Classes and Instances table-dot
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
*/
//> Classes and Instances table-dot
  [TOKEN_DOT]           = {NULL,     dot,    PREC_CALL},
//< Classes and Instances table-dot
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
//> Challenge 17.3 table-ternary
  [TOKEN_QUESTION]      = {NULL,     ternary, PREC_CONDITIONAL},
  [TOKEN_COLON]         = {NULL,     NULL,   PREC_NONE},
//< Challenge 17.3 table-ternary
/* Compiling Expressions rules < Types of Values table-not
  [TOKEN_BANG]          = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-not
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
//< Types of Values table-not
/* Compiling Expressions rules < Types of Values table-equal
  [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-equal
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
//< Types of Values table-equal
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Types of Values table-comparisons
  [TOKEN_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-comparisons
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
//< Types of Values table-comparisons
/* Compiling Expressions rules < Global Variables table-identifier
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
*/
//> Global Variables table-identifier
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
//< Global Variables table-identifier
/* Compiling Expressions rules < Strings table-string
  [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
*/
//> Strings table-string
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
//< Strings table-string
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
/* Compiling Expressions rules < Jumping Back and Forth table-and
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
*/
//> Jumping Back and Forth table-and
  [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
//< Jumping Back and Forth table-and
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Types of Values table-false
  [TOKEN_FALSE]         = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-false
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
//< Types of Values table-false
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Types of Values table-nil
  [TOKEN_NIL]           = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-nil
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
//< Types of Values table-nil
/* Compiling Expressions rules < Jumping Back and Forth table-or
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
*/
//> Jumping Back and Forth table-or
  [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
//< Jumping Back and Forth table-or
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
/* Compiling Expressions rules < Superclasses table-super
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
*/
//> Superclasses table-super
  [TOKEN_SUPER]         = {super_,   NULL,   PREC_NONE},
//< Superclasses table-super
/* Compiling Expressions rules < Methods and Initializers table-this
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
*/
//> Methods and Initializers table-this
  [TOKEN_THIS]          = {this_,    NULL,   PREC_NONE},
//< Methods and Initializers table-this
/* Compiling Expressions rules < Types of Values table-true
  [TOKEN_TRUE]          = {NULL,     NULL,   PREC_NONE},
*/
//> Types of Values table-true
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
//< Types of Values table-true
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CONST]         = {NULL,     NULL,   PREC_NONE}, // Challenge 22.3
  [TOKEN_SWITCH]        = {NULL,     NULL,   PREC_NONE}, // Challenge 23.1
  [TOKEN_CASE]          = {NULL,     NULL,   PREC_NONE}, // Challenge 23.1
  [TOKEN_DEFAULT]       = {NULL,     NULL,   PREC_NONE}, // Challenge 23.1
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};
//< Compiling Expressions rules
//> Compiling Expressions parse-precedence
static void parsePrecedence(Precedence precedence) {
/* Compiling Expressions parse-precedence < Compiling Expressions precedence-body
  // What goes here?
*/
//> precedence-body
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    error("Expect expression.");
    return;
  }

/* Compiling Expressions precedence-body < Global Variables prefix-rule
  prefixRule();
*/
//> Global Variables prefix-rule
  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);
//< Global Variables prefix-rule
//> infix

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
/* Compiling Expressions infix < Global Variables infix-rule
    infixRule();
*/
//> Global Variables infix-rule
    infixRule(canAssign);
//< Global Variables infix-rule
  }
//> Global Variables invalid-assign

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
//< Global Variables invalid-assign
//< infix
//< precedence-body
}
//< Compiling Expressions parse-precedence
//> Compiling Expressions get-rule
static ParseRule* getRule(TokenType type) {
  return &rules[type];
}
//< Compiling Expressions get-rule
//> Compiling Expressions expression
static void expression() {
/* Compiling Expressions expression < Compiling Expressions expression-body
  // What goes here?
*/
//> expression-body
  parsePrecedence(PREC_ASSIGNMENT);
//< expression-body
}
//< Compiling Expressions expression
//> Local Variables block
static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}
//< Local Variables block
//> Calls and Functions compile-function
static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope(); // [no-end-scope]

  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
//> parameters
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        errorAtCurrent("Can't have more than 255 parameters.");
      }
      uint8_t constant = parseVariable("Expect parameter name.", false); // Challenge 22.3
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }
//< parameters
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block();

  ObjFunction* function = endCompiler();
/* Calls and Functions compile-function < Closures emit-closure
  emitBytes(OP_CONSTANT, makeConstant(OBJ_VAL(function)));
*/
//> Closures emit-closure
  emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
//< Closures emit-closure
//> Closures capture-upvalues

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
//> Challenge 22.4 emit-upvalue-index-wide
    // 3-byte big-endian instead of 1 -- a captured local's slot can
    // now exceed 255 (see OP_GET_LOCAL_LONG), up to the same 24-bit
    // ceiling addLocal() enforces. Not a hot enough path (runs once
    // per closure creation, not per access) to bother with a
    // short/long split the way OP_GET_LOCAL_LONG has.
    int index = compiler.upvalues[i].index;
    emitByte((index >> 16) & 0xff);
    emitByte((index >> 8) & 0xff);
    emitByte(index & 0xff);
//< Challenge 22.4 emit-upvalue-index-wide
  }
//< Closures capture-upvalues
}
//< Calls and Functions compile-function
//> Methods and Initializers method
static void method() {
  consume(TOKEN_IDENTIFIER, "Expect method name.");
  uint8_t constant = identifierConstant(&parser.previous);
//> method-body

//< method-body
/* Methods and Initializers method-body < Methods and Initializers method-type
  FunctionType type = TYPE_FUNCTION;
*/
//> method-type
  FunctionType type = TYPE_METHOD;
//< method-type
//> initializer-name
  if (parser.previous.length == 4 &&
      memcmp(parser.previous.start, "init", 4) == 0) {
    type = TYPE_INITIALIZER;
  }
  
//< initializer-name
//> method-body
  function(type);
//< method-body
  emitBytes(OP_METHOD, constant);
}
//< Methods and Initializers method
//> Classes and Instances class-declaration
static void classDeclaration() {
  consume(TOKEN_IDENTIFIER, "Expect class name.");
//> Methods and Initializers class-name
  Token className = parser.previous;
//< Methods and Initializers class-name
  uint8_t nameConstant = identifierConstant(&parser.previous);
  declareVariable(false); // Challenge 22.3

  emitBytes(OP_CLASS, nameConstant);
  // Challenge 21.2: OP_CLASS still needs the class's name as a chunk
  // constant (newClass() uses it as data), but defineVariable() now
  // needs a *global slot*, a different index space -- these are no
  // longer interchangeable, so compute the slot separately (and only
  // when the class is actually at global scope; defineVariable()
  // ignores this operand entirely for a local class).
  defineVariable(current->scopeDepth == 0 ? globalVariable(&className) : 0);

//> Methods and Initializers create-class-compiler
  ClassCompiler classCompiler;
//> Superclasses init-has-superclass
  classCompiler.hasSuperclass = false;
//< Superclasses init-has-superclass
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;

//< Methods and Initializers create-class-compiler
//> Superclasses compile-superclass
  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(false);
//> inherit-self

    if (identifiersEqual(&className, &parser.previous)) {
      error("A class can't inherit from itself.");
    }

//< inherit-self
//> superclass-variable
    beginScope();
    addLocal(syntheticToken("super"), false); // Challenge 22.3
    defineVariable(0);
    
//< superclass-variable
    namedVariable(className, false);
    emitByte(OP_INHERIT);
//> set-has-superclass
    classCompiler.hasSuperclass = true;
//< set-has-superclass
  }
  
//< Superclasses compile-superclass
//> Methods and Initializers load-class
  namedVariable(className, false);
//< Methods and Initializers load-class
  consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
//> Methods and Initializers class-body
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    method();
  }
//< Methods and Initializers class-body
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
//> Methods and Initializers pop-class
  emitByte(OP_POP);
//< Methods and Initializers pop-class
//> Superclasses end-superclass-scope

  if (classCompiler.hasSuperclass) {
    endScope();
  }
//< Superclasses end-superclass-scope
//> Methods and Initializers pop-enclosing

  currentClass = currentClass->enclosing;
//< Methods and Initializers pop-enclosing
}
//< Classes and Instances class-declaration
//> Calls and Functions fun-declaration
static void funDeclaration() {
  uint8_t global = parseVariable("Expect function name.", false); // Challenge 22.3
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}
//< Calls and Functions fun-declaration
//> Global Variables var-declaration
static void varDeclaration() {
  uint8_t global = parseVariable("Expect variable name.", false); // Challenge 22.3

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON,
          "Expect ';' after variable declaration.");

  defineVariable(global);
}
//< Global Variables var-declaration
//> Challenge 22.3 const-declaration
// Mirrors varDeclaration(), with two differences: it's always
// const (so parseVariable()/defineVariable() end up marking the
// local/global unassignable), and it requires an initializer --
// `const a;` would create a binding that can never be given a value,
// which is never useful and always a mistake.
static void constDeclaration() {
  uint8_t global = parseVariable("Expect variable name.", true);

  consume(TOKEN_EQUAL, "Const variable must be initialized.");
  expression();
  consume(TOKEN_SEMICOLON,
          "Expect ';' after variable declaration.");

  defineVariable(global);
}
//< Challenge 22.3 const-declaration
//> Global Variables expression-statement
static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emitByte(OP_POP);
}
//< Global Variables expression-statement
//> Jumping Back and Forth for-statement
static void forStatement() {
//> for-begin-scope
  beginScope();
//< for-begin-scope
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
/* Jumping Back and Forth for-statement < Jumping Back and Forth for-initializer
  consume(TOKEN_SEMICOLON, "Expect ';'.");
*/
//> for-initializer
  if (match(TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    expressionStatement();
  }
//< for-initializer

  int loopStart = currentChunk()->count;
/* Jumping Back and Forth for-statement < Jumping Back and Forth for-exit
  consume(TOKEN_SEMICOLON, "Expect ';'.");
*/
//> for-exit
  int exitJump = -1;
  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    // Jump out of the loop if the condition is false.
    exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Condition.
  }

//< for-exit
/* Jumping Back and Forth for-statement < Jumping Back and Forth for-increment
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
*/
//> for-increment
  if (!match(TOKEN_RIGHT_PAREN)) {
    int bodyJump = emitJump(OP_JUMP);
    int incrementStart = currentChunk()->count;
    expression();
    emitByte(OP_POP);
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    emitLoop(loopStart);
    loopStart = incrementStart;
    patchJump(bodyJump);
  }
//< for-increment

  statement();
  emitLoop(loopStart);
//> exit-jump

  if (exitJump != -1) {
    patchJump(exitJump);
    emitByte(OP_POP); // Condition.
  }

//< exit-jump
//> for-end-scope
  endScope();
//< for-end-scope
}
//< Jumping Back and Forth for-statement
//> Jumping Back and Forth if-statement
//> Challenge 23.1 switch-statement
// Cap on cases in a single switch -- mirrors the existing 255-arg/
// 255-param caps elsewhere in this file: a simple fixed array with a
// clear error if exceeded, rather than growable, since hitting this
// in real code would already be a design smell.
#define MAX_SWITCH_CASES 256

static void switchStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
  expression(); // Switch value. Stays on the stack for the whole statement.
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after switch value.");
  consume(TOKEN_LEFT_BRACE, "Expect '{' before switch body.");

  // Lets a `var` inside one case share scope-depth bookkeeping with
  // the rest of the switch, the same way a single enclosing block
  // would -- correct even though only one case's body ever actually
  // runs, since the compile-time slot accounting doesn't care which
  // branch runs at runtime.
  beginScope();

  // 0 = haven't seen "case"/"default" yet (no bare statements legal
  //     here -- the grammar requires at least one case/default first).
  // 1 = compiling a "case" body; previousCaseSkip is that case's
  //     comparison-failed jump, still needing a landing spot.
  // 2 = compiling the "default" body (must be last).
  int state = 0;
  int previousCaseSkip = -1;
  int caseEnds[MAX_SWITCH_CASES];
  int caseCount = 0;

  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    if (match(TOKEN_CASE) || match(TOKEN_DEFAULT)) {
      TokenType caseType = parser.previous.type;

      if (state == 2) {
        error("Can't have another case or default after the default case.");
      }

      if (state == 1) {
        // The previous case's body just ended without matching
        // nothing explicit broke out of it (no fallthrough is ever
        // allowed), so jump straight to the switch's end -- then let
        // that case's failed-comparison jump land right here, so the
        // next case's comparison can run against the still-live
        // switch value.
        if (caseCount == MAX_SWITCH_CASES) {
          error("Too many cases in switch statement.");
        } else {
          caseEnds[caseCount++] = emitJump(OP_JUMP);
        }
        patchJump(previousCaseSkip);
        emitByte(OP_POP); // The failed comparison's `false`.
      }

      if (caseType == TOKEN_CASE) {
        state = 1;
        // Duplicate the switch value: OP_EQUAL consumes its operands,
        // but a later case (or this jump target, if this one fails)
        // still needs the original switch value available to compare.
        emitByte(OP_DUP);
        expression();
        consume(TOKEN_COLON, "Expect ':' after case value.");
        emitByte(OP_EQUAL);
        previousCaseSkip = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // The successful comparison's `true`.
      } else {
        state = 2;
        consume(TOKEN_COLON, "Expect ':' after 'default'.");
        previousCaseSkip = -1;
      }
    } else {
      if (state == 0) {
        error("Can't have statements before the first case.");
      }
      declaration(); // Not statement(): a case body may declare locals.
    }
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after switch body.");

  // The switch body ended mid-case (no default caught the
  // fall-through) -- that case's failed-comparison jump still needs a
  // target: the very end, same as every caseEnds jump.
  if (state == 1) {
    patchJump(previousCaseSkip);
    emitByte(OP_POP);
  }

  for (int i = 0; i < caseCount; i++) {
    patchJump(caseEnds[i]);
  }

  emitByte(OP_POP); // The switch value itself.
  endScope();
}
//< Challenge 23.1 switch-statement
static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition."); // [paren]

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
//> pop-then
  emitByte(OP_POP);
//< pop-then
  statement();

//> jump-over-else
  int elseJump = emitJump(OP_JUMP);

//< jump-over-else
  patchJump(thenJump);
//> pop-end
  emitByte(OP_POP);
//< pop-end
//> compile-else

  if (match(TOKEN_ELSE)) statement();
//< compile-else
//> patch-else
  patchJump(elseJump);
//< patch-else
}
//< Jumping Back and Forth if-statement
//> Global Variables print-statement
static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}
//< Global Variables print-statement
//> Calls and Functions return-statement
static void returnStatement() {
//> return-from-script
  if (current->type == TYPE_SCRIPT) {
    error("Can't return from top-level code.");
  }

//< return-from-script
  if (match(TOKEN_SEMICOLON)) {
    emitReturn();
  } else {
//> Methods and Initializers return-from-init
    if (current->type == TYPE_INITIALIZER) {
      error("Can't return a value from an initializer.");
    }

//< Methods and Initializers return-from-init
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(OP_RETURN);
  }
}
//< Calls and Functions return-statement
//> Jumping Back and Forth while-statement
static void whileStatement() {
//> loop-start
  int loopStart = currentChunk()->count;
//< loop-start
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  emitByte(OP_POP);
  statement();
//> loop
  emitLoop(loopStart);
//< loop

  patchJump(exitJump);
  emitByte(OP_POP);
}
//< Jumping Back and Forth while-statement
//> Global Variables synchronize
static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;
    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_CONST: // Challenge 22.3
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_SWITCH: // Challenge 23.1
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
        ; // Do nothing.
    }

    advance();
  }
}
//< Global Variables synchronize
//> Global Variables declaration
static void declaration() {
//> Classes and Instances match-class
  if (match(TOKEN_CLASS)) {
    classDeclaration();
/* Calls and Functions match-fun < Classes and Instances match-class
  if (match(TOKEN_FUN)) {
*/
  } else if (match(TOKEN_FUN)) {
//< Classes and Instances match-class
//> Calls and Functions match-fun
    funDeclaration();
/* Global Variables match-var < Calls and Functions match-fun
  if (match(TOKEN_VAR)) {
*/
  } else if (match(TOKEN_VAR)) {
//< Calls and Functions match-fun
//> match-var
    varDeclaration();
//> Challenge 22.3 match-const
  } else if (match(TOKEN_CONST)) {
    constDeclaration();
//< Challenge 22.3 match-const
  } else {
    statement();
  }
//< match-var
/* Global Variables declaration < Global Variables match-var
  statement();
*/
//> call-synchronize

  if (parser.panicMode) synchronize();
//< call-synchronize
}
//< Global Variables declaration
//> Global Variables statement
static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
//> Jumping Back and Forth parse-for
  } else if (match(TOKEN_FOR)) {
    forStatement();
//< Jumping Back and Forth parse-for
//> Jumping Back and Forth parse-if
  } else if (match(TOKEN_IF)) {
    ifStatement();
//< Jumping Back and Forth parse-if
//> Challenge 23.1 parse-switch
  } else if (match(TOKEN_SWITCH)) {
    switchStatement();
//< Challenge 23.1 parse-switch
//> Calls and Functions match-return
  } else if (match(TOKEN_RETURN)) {
    returnStatement();
//< Calls and Functions match-return
//> Jumping Back and Forth parse-while
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
//< Jumping Back and Forth parse-while
//> Local Variables parse-block
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
//< Local Variables parse-block
//> parse-expressions-statement
  } else {
    expressionStatement();
//< parse-expressions-statement
  }
}
//< Global Variables statement

/* Scanning on Demand compiler-c < Compiling Expressions compile-signature
void compile(const char* source) {
*/
/* Compiling Expressions compile-signature < Calls and Functions compile-signature
bool compile(const char* source, Chunk* chunk) {
*/
//> Calls and Functions compile-signature
ObjFunction* compile(const char* source) {
//< Calls and Functions compile-signature
  initScanner(source);
/* Scanning on Demand dump-tokens < Compiling Expressions compile-chunk
  int line = -1;
  for (;;) {
    Token token = scanToken();
    if (token.line != line) {
      printf("%4d ", token.line);
      line = token.line;
    } else {
      printf("   | ");
    }
    printf("%2d '%.*s'\n", token.type, token.length, token.start); // [format]

    if (token.type == TOKEN_EOF) break;
  }
*/
//> Local Variables compiler
  Compiler compiler;
//< Local Variables compiler
/* Local Variables compiler < Calls and Functions call-init-compiler
  initCompiler(&compiler);
*/
//> Calls and Functions call-init-compiler
  initCompiler(&compiler, TYPE_SCRIPT);
//< Calls and Functions call-init-compiler
/* Compiling Expressions init-compile-chunk < Calls and Functions call-init-compiler
  compilingChunk = chunk;
*/
//> Compiling Expressions compile-chunk
//> init-parser-error

  parser.hadError = false;
  parser.panicMode = false;

//< init-parser-error
  advance();
//< Compiling Expressions compile-chunk
/* Compiling Expressions compile-chunk < Global Variables compile
  expression();
  consume(TOKEN_EOF, "Expect end of expression.");
*/
//> Global Variables compile

  while (!match(TOKEN_EOF)) {
    declaration();
  }

//< Global Variables compile
/* Compiling Expressions finish-compile < Calls and Functions call-end-compiler
  endCompiler();
*/
/* Compiling Expressions return-had-error < Calls and Functions call-end-compiler
  return !parser.hadError;
*/
//> Calls and Functions call-end-compiler
  ObjFunction* function = endCompiler();
  return parser.hadError ? NULL : function;
//< Calls and Functions call-end-compiler
}
//> Garbage Collection mark-compiler-roots
void markCompilerRoots() {
  Compiler* compiler = current;
  while (compiler != NULL) {
    markObject((Obj*)compiler->function);
//> Challenge 22.1 mark-local-slots
    // Unlike stringConstants' keys, local names are never also stored
    // as chunk constants (locals carry no name at runtime at all) --
    // so markObject(function) above doesn't transitively protect
    // them. They need marking directly here.
    markTable(&compiler->localSlots);
//< Challenge 22.1 mark-local-slots
    compiler = compiler->enclosing;
  }
}
//< Garbage Collection mark-compiler-roots
