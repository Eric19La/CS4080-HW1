//> Chunks of Bytecode chunk-c
#include <stdlib.h>

#include "chunk.h"
//> chunk-c-include-memory
#include "memory.h"
//< chunk-c-include-memory
//> Garbage Collection chunk-include-vm
#include "vm.h"
//< Garbage Collection chunk-include-vm

void initChunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
//> Challenge 14.1 init-lines
  chunk->lineCount = 0;
  chunk->lineCapacity = 0;
  chunk->lines = NULL;
//< Challenge 14.1 init-lines
//> chunk-init-constant-array
  initValueArray(&chunk->constants);
//< chunk-init-constant-array
}
//> free-chunk
void freeChunk(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
//> Challenge 14.1 free-lines
  FREE_ARRAY(LineStart, chunk->lines, chunk->lineCapacity);
//< Challenge 14.1 free-lines
//> chunk-free-constants
  freeValueArray(&chunk->constants);
//< chunk-free-constants
  initChunk(chunk);
}
//< free-chunk
/* Chunks of Bytecode write-chunk < Chunks of Bytecode write-chunk-with-line
void writeChunk(Chunk* chunk, uint8_t byte) {
*/
//> write-chunk
//> write-chunk-with-line
void writeChunk(Chunk* chunk, uint8_t byte, int line) {
//< write-chunk-with-line
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code,
        oldCapacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->count++;

//> Challenge 14.1 write-line-rle
  // Run-length encode: only record a new LineStart when the line
  // actually changes from the previous instruction's line.
  if (chunk->lineCount > 0 &&
      chunk->lines[chunk->lineCount - 1].line == line) {
    return;
  }

  if (chunk->lineCapacity < chunk->lineCount + 1) {
    int oldCapacity = chunk->lineCapacity;
    chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
    chunk->lines = GROW_ARRAY(LineStart, chunk->lines,
        oldCapacity, chunk->lineCapacity);
  }

  LineStart* lineStart = &chunk->lines[chunk->lineCount++];
  lineStart->offset = chunk->count - 1;
  lineStart->line = line;
//< Challenge 14.1 write-line-rle
}
//< write-chunk
//> add-constant
int addConstant(Chunk* chunk, Value value) {
//> Garbage Collection add-constant-push
  push(value);
//< Garbage Collection add-constant-push
  writeValueArray(&chunk->constants, value);
//> Garbage Collection add-constant-pop
  pop();
//< Garbage Collection add-constant-pop
  return chunk->constants.count - 1;
}
//< add-constant
//> Challenge 14.1 get-line
int getLine(Chunk* chunk, int instruction) {
  // Entries are offset-ascending (appended in increasing order as the
  // chunk was written), so scanning backward from the end finds the
  // last run that starts at or before `instruction` -- i.e. the run it
  // belongs to. Only ever called when reporting a runtime error, so a
  // simple linear scan (rather than a binary search) is fine here.
  for (int i = chunk->lineCount - 1; i >= 0; i--) {
    if (chunk->lines[i].offset <= instruction) {
      return chunk->lines[i].line;
    }
  }

  return -1; // Unreachable for a valid instruction offset.
}
//< Challenge 14.1 get-line
