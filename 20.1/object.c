//> Strings object-c
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
//> Hash Tables object-include-table
#include "table.h"
//< Hash Tables object-include-table
#include "value.h"
#include "vm.h"
//> allocate-obj

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)
//< allocate-obj
//> allocate-object

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
//> Garbage Collection init-is-marked
  object->isMarked = false;
//< Garbage Collection init-is-marked
//> add-to-list
  
  object->next = vm.objects;
  vm.objects = object;
//< add-to-list
//> Garbage Collection debug-log-allocate

#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

//< Garbage Collection debug-log-allocate
  return object;
}
//< allocate-object
//> Methods and Initializers new-bound-method
ObjBoundMethod* newBoundMethod(Value receiver,
                               ObjClosure* method) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod,
                                       OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}
//< Methods and Initializers new-bound-method
//> Classes and Instances new-class
ObjClass* newClass(ObjString* name) {
  ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  klass->name = name; // [klass]
//> Methods and Initializers init-methods
  initTable(&klass->methods);
//< Methods and Initializers init-methods
  return klass;
}
//< Classes and Instances new-class
//> Closures new-closure
ObjClosure* newClosure(ObjFunction* function) {
//> allocate-upvalue-array
  ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*,
                                   function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

//< allocate-upvalue-array
  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
//> init-upvalue-fields
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
//< init-upvalue-fields
  return closure;
}
//< Closures new-closure
//> Calls and Functions new-function
ObjFunction* newFunction() {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
//> Closures init-upvalue-count
  function->upvalueCount = 0;
//< Closures init-upvalue-count
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}
//< Calls and Functions new-function
//> Classes and Instances new-instance
ObjInstance* newInstance(ObjClass* klass) {
  ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}
//< Classes and Instances new-instance
//> Calls and Functions new-native
ObjNative* newNative(NativeFn function) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  return native;
}
//< Calls and Functions new-native

//> Challenge 19.1 allocate-string-flex
// sizeof(ObjString) does NOT include the flexible array member's own
// bytes -- the extra `length + 1` has to be added on explicitly so
// allocateObject reserves room for the characters right after the
// struct, in the same allocation.
static ObjString* allocateString(int length, uint32_t hash) {
  ObjString* string = (ObjString*)allocateObject(
      sizeof(ObjString) + length + 1, OBJ_STRING);
  string->length = length;
  string->hash = hash;

  push(OBJ_VAL(string));
  tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL); // Challenge 20.1
  pop();

  return string;
}
//< Challenge 19.1 allocate-string-flex
//> Hash Tables hash-string
static uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}
//< Hash Tables hash-string
//> take-string
ObjString* takeString(char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
//> Challenge 19.1 take-string-flex
  // Either way, the characters now have to be copied into the
  // ObjString's own flexible-array storage -- there's no longer a
  // separate buffer for a *found* duplicate to "become", and a *new*
  // string's final storage doesn't exist until after we know its
  // length, which we already did before the caller built `chars`.
  // The caller's temporary buffer is never kept; free it either way.
  ObjString* result;
  if (interned != NULL) {
    result = interned;
  } else {
    result = allocateString(length, hash);
    memcpy(result->chars, chars, length);
    result->chars[length] = '\0';
  }

  FREE_ARRAY(char, chars, length + 1);
  return result;
//< Challenge 19.1 take-string-flex
}
//< take-string
ObjString* copyString(const char* chars, int length) {
//> Hash Tables copy-string-hash
  uint32_t hash = hashString(chars, length);
//> copy-string-intern
  ObjString* interned = tableFindString(&vm.strings, chars, length,
                                        hash);
  if (interned != NULL) return interned;

//< copy-string-intern
//< Hash Tables copy-string-hash
//> Challenge 19.1 copy-string-flex
  ObjString* string = allocateString(length, hash);
  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';
  return string;
//< Challenge 19.1 copy-string-flex
}
//> Closures new-upvalue
ObjUpvalue* newUpvalue(Value* slot) {
  ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
//> init-closed
  upvalue->closed = NIL_VAL;
//< init-closed
  upvalue->location = slot;
//> init-next
  upvalue->next = NULL;
//< init-next
  return upvalue;
}
//< Closures new-upvalue
//> Calls and Functions print-function-helper
static void printFunction(ObjFunction* function) {
//> print-script
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
//< print-script
  printf("<fn %s>", function->name->chars);
}
//< Calls and Functions print-function-helper
//> print-object
void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
//> Methods and Initializers print-bound-method
    case OBJ_BOUND_METHOD:
      printFunction(AS_BOUND_METHOD(value)->method->function);
      break;
//< Methods and Initializers print-bound-method
//> Classes and Instances print-class
    case OBJ_CLASS:
      printf("%s", AS_CLASS(value)->name->chars);
      break;
//< Classes and Instances print-class
//> Closures print-closure
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function);
      break;
//< Closures print-closure
//> Calls and Functions print-function
    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;
//< Calls and Functions print-function
//> Classes and Instances print-instance
    case OBJ_INSTANCE:
      printf("%s instance",
             AS_INSTANCE(value)->klass->name->chars);
      break;
//< Classes and Instances print-instance
//> Calls and Functions print-native
    case OBJ_NATIVE:
      printf("<native fn>");
      break;
//< Calls and Functions print-native
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
//> Closures print-upvalue
    case OBJ_UPVALUE:
      printf("upvalue");
      break;
//< Closures print-upvalue
  }
}
//< print-object
