//> Chunks of Bytecode memory-c
#include <stdlib.h>
//> Challenge 14.3 allocator-includes
#include <stdio.h>
#include <string.h>
//< Challenge 14.3 allocator-includes

//> Garbage Collection memory-include-compiler
#include "compiler.h"
//< Garbage Collection memory-include-compiler
#include "memory.h"
//> Strings memory-include-vm
#include "vm.h"
//< Strings memory-include-vm
//> Garbage Collection debug-log-includes

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif
//< Garbage Collection debug-log-includes
//> Garbage Collection heap-grow-factor

#define GC_HEAP_GROW_FACTOR 2
//< Garbage Collection heap-grow-factor

//> Challenge 14.3 custom-allocator
// A hand-rolled explicit-free-list allocator over a single arena
// obtained from malloc() exactly once (in initAllocator()). No other
// code in this file calls malloc()/realloc()/free() again.
//
// Every block -- free or in use -- starts with a BlockHeader and ends
// with a footer (a bare size_t, a duplicate of header->size). Storing
// the size at both ends ("boundary tags") is what makes coalescing
// possible in both directions: from any block you can step forward by
// `size` bytes to find the next block's header, or step backward by
// sizeof(size_t) bytes to read the *previous* block's footer and learn
// its size (and therefore its header's address) without needing any
// separate index of blocks.

typedef struct BlockHeader {
  size_t size;               // Total size: header + payload + footer.
  bool free;
  struct BlockHeader* next;  // Valid only while this block is free.
} BlockHeader;

#define HEADER_SIZE sizeof(BlockHeader)
#define FOOTER_SIZE sizeof(size_t)
#define BLOCK_OVERHEAD (HEADER_SIZE + FOOTER_SIZE)
#define ALLOC_ALIGNMENT 8
#define MIN_PAYLOAD ALLOC_ALIGNMENT

static char* arenaStart = NULL;
static char* arenaEnd = NULL;
static BlockHeader* freeList = NULL;

static size_t alignUp(size_t size) {
  return (size + (ALLOC_ALIGNMENT - 1)) & ~(size_t)(ALLOC_ALIGNMENT - 1);
}

static size_t* footerOf(BlockHeader* header) {
  return (size_t*)((char*)header + header->size - FOOTER_SIZE);
}

static void writeFooter(BlockHeader* header) {
  *footerOf(header) = header->size;
}

void initAllocator(size_t arenaSize) {
  arenaStart = (char*)malloc(arenaSize);
  if (arenaStart == NULL) {
    fprintf(stderr, "Fatal: could not allocate interpreter arena.\n");
    exit(1);
  }
  arenaEnd = arenaStart + arenaSize;

  BlockHeader* header = (BlockHeader*)arenaStart;
  header->size = arenaSize;
  header->free = true;
  header->next = NULL;
  writeFooter(header);

  freeList = header;
}

static void removeFromFreeList(BlockHeader* target) {
  if (freeList == target) {
    freeList = target->next;
    return;
  }

  BlockHeader* current = freeList;
  while (current != NULL && current->next != target) {
    current = current->next;
  }
  if (current != NULL) {
    current->next = target->next;
  }
}

// First-fit: walk the free list and take the first block big enough.
// Simple, not segregated-by-size the way a real allocator would be --
// fine here since getLine()-style "doesn't need to be fast" applies:
// this is a teaching VM, not a production allocator.
static void* arenaMalloc(size_t size) {
  if (size == 0) size = 1;
  size_t needed = alignUp(size) + BLOCK_OVERHEAD;

  BlockHeader* current = freeList;
  while (current != NULL && current->size < needed) {
    current = current->next;
  }

  if (current == NULL) return NULL; // Arena exhausted.

  removeFromFreeList(current);

  size_t remaining = current->size - needed;
  if (remaining >= BLOCK_OVERHEAD + MIN_PAYLOAD) {
    // Split off the unused tail into its own free block rather than
    // handing the caller more space than they asked for.
    BlockHeader* split = (BlockHeader*)((char*)current + needed);
    split->size = remaining;
    split->free = true;
    split->next = freeList;
    writeFooter(split);
    freeList = split;

    current->size = needed;
  }
  // Otherwise the leftover sliver is too small to ever be useful as
  // its own block, so just hand the whole thing over.

  current->free = false;
  writeFooter(current);
  return (char*)current + HEADER_SIZE;
}

static void arenaFree(void* pointer) {
  if (pointer == NULL) return;

  BlockHeader* header = (BlockHeader*)((char*)pointer - HEADER_SIZE);
  header->free = true;

  // Forward coalesce: is the next block (by address) free?
  char* nextAddr = (char*)header + header->size;
  if (nextAddr < arenaEnd) {
    BlockHeader* next = (BlockHeader*)nextAddr;
    if (next->free) {
      removeFromFreeList(next);
      header->size += next->size;
    }
  }

  // Backward coalesce: is the block before this one (by address) free?
  // Its footer sits immediately before our own header.
  if ((char*)header > arenaStart) {
    size_t precedingSize = *(size_t*)((char*)header - FOOTER_SIZE);
    BlockHeader* preceding = (BlockHeader*)((char*)header - precedingSize);
    if ((char*)preceding >= arenaStart && preceding->free) {
      // Absorb `header` into `preceding`, which is already linked in
      // the free list -- no list surgery needed, just grow it.
      preceding->size += header->size;
      writeFooter(preceding);
      return;
    }
  }

  writeFooter(header);
  header->next = freeList;
  freeList = header;
}

// Raw realloc()-equivalent against the arena -- no GC bookkeeping.
// Exists as a separate layer under reallocate() so that markObject()'s
// gray stack (bookkeeping memory for the collector itself, not Lox
// heap memory) can grow without affecting vm.bytesAllocated or
// re-triggering collectGarbage() while a collection is already running.
static void* arenaRealloc(void* pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    arenaFree(pointer);
    return NULL;
  }

  void* result = arenaMalloc(newSize);
  if (result == NULL) return NULL;

  if (pointer != NULL) {
    size_t copySize = oldSize < newSize ? oldSize : newSize;
    memcpy(result, pointer, copySize);
    arenaFree(pointer);
  }

  return result;
}
//< Challenge 14.3 custom-allocator

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
//> Garbage Collection updated-bytes-allocated
  vm.bytesAllocated += newSize - oldSize;
//< Garbage Collection updated-bytes-allocated
//> Garbage Collection call-collect
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif
//> collect-on-next

    if (vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
//< collect-on-next
  }

//< Garbage Collection call-collect
  if (newSize == 0) {
//> Challenge 14.3 reallocate-uses-arena-free
    arenaFree(pointer);
//< Challenge 14.3 reallocate-uses-arena-free
    return NULL;
  }

//> Challenge 14.3 reallocate-uses-arena-realloc
  void* result = arenaRealloc(pointer, oldSize, newSize);
//< Challenge 14.3 reallocate-uses-arena-realloc
//> out-of-memory
  if (result == NULL) exit(1);
//< out-of-memory
  return result;
}
//> Garbage Collection mark-object
void markObject(Obj* object) {
  if (object == NULL) return;
//> check-is-marked
  if (object->isMarked) return;

//< check-is-marked
//> log-mark-object
#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

//< log-mark-object
  object->isMarked = true;
//> add-to-gray-stack

  if (vm.grayCapacity < vm.grayCount + 1) {
//> Challenge 14.3 gray-stack-uses-arena
    int oldGrayCapacity = vm.grayCapacity;
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    vm.grayStack = (Obj**)arenaRealloc(vm.grayStack,
        sizeof(Obj*) * oldGrayCapacity, sizeof(Obj*) * vm.grayCapacity);
//< Challenge 14.3 gray-stack-uses-arena
//> exit-gray-stack

    if (vm.grayStack == NULL) exit(1);
//< exit-gray-stack
  }

  vm.grayStack[vm.grayCount++] = object;
//< add-to-gray-stack
}
//< Garbage Collection mark-object
//> Garbage Collection mark-value
void markValue(Value value) {
  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}
//< Garbage Collection mark-value
//> Garbage Collection mark-array
static void markArray(ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}
//< Garbage Collection mark-array
//> Garbage Collection blacken-object
static void blackenObject(Obj* object) {
//> log-blacken-object
#ifdef DEBUG_LOG_GC
  printf("%p blacken ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

//< log-blacken-object
  switch (object->type) {
//> Methods and Initializers blacken-bound-method
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = (ObjBoundMethod*)object;
      markValue(bound->receiver);
      markObject((Obj*)bound->method);
      break;
    }
//< Methods and Initializers blacken-bound-method
//> Classes and Instances blacken-class
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      markObject((Obj*)klass->name);
//> Methods and Initializers mark-methods
      markTable(&klass->methods);
//< Methods and Initializers mark-methods
      break;
    }
//< Classes and Instances blacken-class
//> blacken-closure
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      markObject((Obj*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) {
        markObject((Obj*)closure->upvalues[i]);
      }
      break;
    }
//< blacken-closure
//> blacken-function
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      markObject((Obj*)function->name);
      markArray(&function->chunk.constants);
      break;
    }
//< blacken-function
//> Classes and Instances blacken-instance
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      markObject((Obj*)instance->klass);
      markTable(&instance->fields);
      break;
    }
//< Classes and Instances blacken-instance
//> blacken-upvalue
    case OBJ_UPVALUE:
      markValue(((ObjUpvalue*)object)->closed);
      break;
//< blacken-upvalue
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;
  }
}
//< Garbage Collection blacken-object
//> Strings free-object
static void freeObject(Obj* object) {
//> Garbage Collection log-free-object
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)object, object->type);
#endif

//< Garbage Collection log-free-object
  switch (object->type) {
//> Methods and Initializers free-bound-method
    case OBJ_BOUND_METHOD:
      FREE(ObjBoundMethod, object);
      break;
//< Methods and Initializers free-bound-method
//> Classes and Instances free-class
    case OBJ_CLASS: {
//> Methods and Initializers free-methods
      ObjClass* klass = (ObjClass*)object;
      freeTable(&klass->methods);
//< Methods and Initializers free-methods
      FREE(ObjClass, object);
      break;
    } // [braces]
//< Classes and Instances free-class
//> Closures free-closure
    case OBJ_CLOSURE: {
//> free-upvalues
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues,
                 closure->upvalueCount);
//< free-upvalues
      FREE(ObjClosure, object);
      break;
    }
//< Closures free-closure
//> Calls and Functions free-function
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
//< Calls and Functions free-function
//> Classes and Instances free-instance
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      freeTable(&instance->fields);
      FREE(ObjInstance, object);
      break;
    }
//< Classes and Instances free-instance
//> Calls and Functions free-native
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
//< Calls and Functions free-native
//> Challenge 19.1 free-string-flex
    case OBJ_STRING: {
      // The chars are now part of THIS SAME allocation (flexible
      // array member), not a separate one -- one free, sized to
      // match exactly what allocateString() originally requested.
      ObjString* string = (ObjString*)object;
      reallocate(string, sizeof(ObjString) + string->length + 1, 0);
      break;
    }
//< Challenge 19.1 free-string-flex
//> Closures free-upvalue
    case OBJ_UPVALUE:
      FREE(ObjUpvalue, object);
      break;
//< Closures free-upvalue
  }
}
//< Strings free-object
//> Garbage Collection mark-roots
static void markRoots() {
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }
//> mark-closures

  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj*)vm.frames[i].closure);
  }
//< mark-closures
//> mark-open-upvalues

  for (ObjUpvalue* upvalue = vm.openUpvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {
    markObject((Obj*)upvalue);
  }
//< mark-open-upvalues
//> mark-globals

  markTable(&vm.globals);
//< mark-globals
//> call-mark-compiler-roots
  markCompilerRoots();
//< call-mark-compiler-roots
//> Methods and Initializers mark-init-string
  markObject((Obj*)vm.initString);
//< Methods and Initializers mark-init-string
}
//< Garbage Collection mark-roots
//> Garbage Collection trace-references
static void traceReferences() {
  while (vm.grayCount > 0) {
    Obj* object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}
//< Garbage Collection trace-references
//> Garbage Collection sweep
static void sweep() {
  Obj* previous = NULL;
  Obj* object = vm.objects;
  while (object != NULL) {
    if (object->isMarked) {
//> unmark
      object->isMarked = false;
//< unmark
      previous = object;
      object = object->next;
    } else {
      Obj* unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }

      freeObject(unreached);
    }
  }
}
//< Garbage Collection sweep
//> Garbage Collection collect-garbage
void collectGarbage() {
//> log-before-collect
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
//> log-before-size
  size_t before = vm.bytesAllocated;
//< log-before-size
#endif
//< log-before-collect
//> call-mark-roots

  markRoots();
//< call-mark-roots
//> call-trace-references
  traceReferences();
//< call-trace-references
//> sweep-strings
  tableRemoveWhite(&vm.strings);
//< sweep-strings
//> call-sweep
  sweep();
//< call-sweep
//> update-next-gc

  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;
//< update-next-gc
//> log-after-collect

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
//> log-collected-amount
  printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - vm.bytesAllocated, before, vm.bytesAllocated,
         vm.nextGC);
//< log-collected-amount
#endif
//< log-after-collect
}
//< Garbage Collection collect-garbage
//> Strings free-objects
void freeObjects() {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }
//> Garbage Collection free-gray-stack

//> Challenge 14.3 free-gray-stack-uses-arena
  arenaFree(vm.grayStack);
//< Challenge 14.3 free-gray-stack-uses-arena
//< Garbage Collection free-gray-stack
}
//< Strings free-objects
