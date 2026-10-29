//> A Virtual Machine vm-h
#ifndef clox_vm_h
#define clox_vm_h

/* A Virtual Machine vm-h < Calls and Functions vm-include-object
#include "chunk.h"
*/
//> Calls and Functions vm-include-object
#include "object.h"
//< Calls and Functions vm-include-object
//> Hash Tables vm-include-table
#include "table.h"
//< Hash Tables vm-include-table
//> vm-include-value
#include "value.h"
//< vm-include-value
//> stack-max

//< stack-max
/* A Virtual Machine stack-max < Calls and Functions frame-max
#define STACK_MAX 256
*/
//> Calls and Functions frame-max
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)
//< Calls and Functions frame-max
//> Calls and Functions call-frame

typedef struct {
/* Calls and Functions call-frame < Closures call-frame-closure
  ObjFunction* function;
*/
//> Closures call-frame-closure
  ObjClosure* closure;
//< Closures call-frame-closure
  uint8_t* ip;
  Value* slots;
} CallFrame;
//< Calls and Functions call-frame

//> Challenge 21.2 global-struct
// One slot in the VM's global-variable array. `isDefined` plays the
// role the old hash table's "key present" check played: a global can
// be *referenced* (and thus allocated a slot) before it's ever
// assigned, by code compiled ahead of the `var` statement that defines
// it (e.g. inside a function body). `name` exists only so a runtime
// error ("Undefined variable 'x'.") can recover the name from a bare
// slot index.
typedef struct {
  Value value;
  bool isDefined;
  ObjString* name;
} Global;
//< Challenge 21.2 global-struct

typedef struct {
/* A Virtual Machine vm-h < Calls and Functions frame-array
  Chunk* chunk;
*/
/* A Virtual Machine ip < Calls and Functions frame-array
  uint8_t* ip;
*/
//> Calls and Functions frame-array
  CallFrame frames[FRAMES_MAX];
  int frameCount;
  
//< Calls and Functions frame-array
//> vm-stack
//> Challenge 15.3 growable-stack
  Value* stack;
  int stackCapacity;
//< Challenge 15.3 growable-stack
  Value* stackTop;
//< vm-stack
//> Global Variables vm-globals
//> Challenge 21.2 globals-array
  Global* globals;
  int globalCount;
  int globalCapacity;
  // Compile-time (and startup-native-registration-time) name -> slot
  // index map. Persists for the VM's whole lifetime -- not per-chunk --
  // so separate REPL lines, and native functions registered before any
  // Lox source is compiled, all agree on which slot a given name owns.
  Table globalNames;
//< Challenge 21.2 globals-array
//< Global Variables vm-globals
//> Hash Tables vm-strings
  Table strings;
//< Hash Tables vm-strings
//> Methods and Initializers vm-init-string
  ObjString* initString;
//< Methods and Initializers vm-init-string
//> Closures open-upvalues-field
  ObjUpvalue* openUpvalues;
//< Closures open-upvalues-field
//> Garbage Collection vm-fields

  size_t bytesAllocated;
  size_t nextGC;
//< Garbage Collection vm-fields
//> Strings objects-root
  Obj* objects;
//< Strings objects-root
//> Garbage Collection vm-gray-stack
  int grayCount;
  int grayCapacity;
  Obj** grayStack;
//< Garbage Collection vm-gray-stack
} VM;

//> interpret-result
typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

//< interpret-result
//> Strings extern-vm
extern VM vm;

//< Strings extern-vm
void initVM();
void freeVM();
//> Challenge 21.2 ensure-global-slot-h
// Get-or-create this name's slot in vm.globals. Used both by the
// compiler (resolving a global reference/declaration) and by
// defineNative() (registering a native function before any Lox source
// has been compiled) -- both need the same name -> slot mapping.
int ensureGlobalSlot(ObjString* name);
//< Challenge 21.2 ensure-global-slot-h
/* A Virtual Machine interpret-h < Scanning on Demand vm-interpret-h
InterpretResult interpret(Chunk* chunk);
*/
//> Scanning on Demand vm-interpret-h
InterpretResult interpret(const char* source);
//< Scanning on Demand vm-interpret-h
//> push-pop
void push(Value value);
Value pop();
//< push-pop

#endif
