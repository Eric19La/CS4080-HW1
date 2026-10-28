//> Hash Tables table-h
#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"
//> Challenge 20.1 entry-state
// Generalized keys (any primitive Value, not just ObjString*) mean we
// can no longer use "key == NULL" as the empty-slot sentinel the way
// the original string-only table did -- nil is now a legitimate KEY,
// so it can't also mean "nothing here." An explicit state tag per
// slot replaces that overloaded-null trick.
typedef enum {
  ENTRY_EMPTY,
  ENTRY_TOMBSTONE,
  ENTRY_OCCUPIED
} EntryState;
//< Challenge 20.1 entry-state
//> entry

typedef struct {
//> Challenge 20.1 value-key
  Value key;
//< Challenge 20.1 value-key
  Value value;
//> Challenge 20.1 entry-state-field
  EntryState state;
//< Challenge 20.1 entry-state-field
} Entry;
//< entry

typedef struct {
  int count;
  int capacity;
  Entry* entries;
} Table;

//> init-table-h
void initTable(Table* table);
//> free-table-h
void freeTable(Table* table);
//< free-table-h
//> table-get-h
//> Challenge 20.1 value-key-api
bool tableGet(Table* table, Value key, Value* value);
//< Challenge 20.1 value-key-api
//< table-get-h
//> table-set-h
bool tableSet(Table* table, Value key, Value value);
//< table-set-h
//> table-delete-h
bool tableDelete(Table* table, Value key);
//< table-delete-h
//> table-add-all-h
void tableAddAll(Table* from, Table* to);
//< table-add-all-h
//> table-find-string-h
ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash);
//< table-find-string-h
//> Garbage Collection table-remove-white-h

void tableRemoveWhite(Table* table);
//< Garbage Collection table-remove-white-h
//> Garbage Collection mark-table-h
void markTable(Table* table);
//< Garbage Collection mark-table-h

//< init-table-h
#endif
