//> Hash Tables table-c
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

//> max-load
#define TABLE_MAX_LOAD 0.75

//< max-load
void initTable(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}
//> free-table
void freeTable(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}
//< free-table
//> Challenge 20.1 hash-value
// Hash any primitive Value -- not just strings. Strings reuse their
// already-cached FNV-1a hash (computed once, at creation, since Lox
// strings are immutable). Other object types fall back to a
// pointer-identity hash: deterministic and correct, but not
// meaningful beyond "is this the exact same object" -- see the
// Challenge 20.1 discussion on why class instances as keys need more
// than this to be genuinely useful.
static uint32_t hashValue(Value value) {
  if (IS_BOOL(value)) return AS_BOOL(value) ? 1u : 0u;
  if (IS_NIL(value)) return 2u;
  if (IS_NUMBER(value)) {
    double d = AS_NUMBER(value);
    uint64_t bits;
    memcpy(&bits, &d, sizeof(bits));
    // A standard 64-bit mix (splitmix64-style) over the double's raw
    // bits: deterministic (same double always hashes the same way)
    // and scatters well, same goals FNV-1a serves for strings.
    bits = (bits ^ (bits >> 33)) * 0xff51afd7ed558ccdULL;
    bits = (bits ^ (bits >> 33)) * 0xc4ceb9fe1a85ec53ULL;
    bits ^= (bits >> 33);
    return (uint32_t)bits;
  }
  if (IS_STRING(value)) return AS_STRING(value)->hash;
  return (uint32_t)(uintptr_t)AS_OBJ(value);
}
//< Challenge 20.1 hash-value
//> find-entry
//> omit
// NOTE: The "Optimization" chapter has a manual copy of this function.
// If you change it here, make sure to update that copy.
//< omit
static Entry* findEntry(Entry* entries, int capacity, Value key) {
//> Challenge 20.1 find-entry-value
  uint32_t index = hashValue(key) & (capacity - 1);
//< Challenge 20.1 find-entry-value
//> find-entry-tombstone
  Entry* tombstone = NULL;

//< find-entry-tombstone
  for (;;) {
    Entry* entry = &entries[index];
//> Challenge 20.1 find-entry-state
    if (entry->state == ENTRY_EMPTY) {
      // Empty entry.
      return tombstone != NULL ? tombstone : entry;
    } else if (entry->state == ENTRY_TOMBSTONE) {
      if (tombstone == NULL) tombstone = entry;
    } else if (valuesEqual(entry->key, key)) {
      // We found the key.
      return entry;
    }
//< Challenge 20.1 find-entry-state

//> Optimization next-index
    index = (index + 1) & (capacity - 1);
//< Optimization next-index
  }
}
//< find-entry
//> table-get
bool tableGet(Table* table, Value key, Value* value) {
  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
//> Challenge 20.1 table-get-state
  if (entry->state != ENTRY_OCCUPIED) return false;
//< Challenge 20.1 table-get-state

  *value = entry->value;
  return true;
}
//< table-get
//> table-adjust-capacity
static void adjustCapacity(Table* table, int capacity) {
  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
//> Challenge 20.1 adjust-capacity-state
    entries[i].key = NIL_VAL;
    entries[i].value = NIL_VAL;
    entries[i].state = ENTRY_EMPTY;
//< Challenge 20.1 adjust-capacity-state
  }
//> re-hash

//> resize-init-count
  table->count = 0;
//< resize-init-count
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
//> Challenge 20.1 rehash-state
    if (entry->state != ENTRY_OCCUPIED) continue;
//< Challenge 20.1 rehash-state

    Entry* dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
//> Challenge 20.1 rehash-mark-occupied
    dest->state = ENTRY_OCCUPIED;
//< Challenge 20.1 rehash-mark-occupied
//> resize-increment-count
    table->count++;
//< resize-increment-count
  }
//< re-hash

//> Hash Tables free-old-array
  FREE_ARRAY(Entry, table->entries, table->capacity);
//< Hash Tables free-old-array
  table->entries = entries;
  table->capacity = capacity;
}
//< table-adjust-capacity
//> table-set
bool tableSet(Table* table, Value key, Value value) {
//> table-set-grow
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

//< table-set-grow
  Entry* entry = findEntry(table->entries, table->capacity, key);
//> Challenge 20.1 table-set-state
  bool isNewKey = entry->state != ENTRY_OCCUPIED;
  if (isNewKey && entry->state == ENTRY_EMPTY) table->count++;

  entry->key = key;
  entry->value = value;
  entry->state = ENTRY_OCCUPIED;
//< Challenge 20.1 table-set-state
  return isNewKey;
}
//< table-set
//> table-delete
bool tableDelete(Table* table, Value key) {
  if (table->count == 0) return false;

  // Find the entry.
  Entry* entry = findEntry(table->entries, table->capacity, key);
//> Challenge 20.1 table-delete-state
  if (entry->state != ENTRY_OCCUPIED) return false;

  // Place a tombstone in the entry.
  entry->key = NIL_VAL;
  entry->value = BOOL_VAL(true);
  entry->state = ENTRY_TOMBSTONE;
//< Challenge 20.1 table-delete-state
  return true;
}
//< table-delete
//> table-add-all
void tableAddAll(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
//> Challenge 20.1 add-all-state
    if (entry->state == ENTRY_OCCUPIED) {
//< Challenge 20.1 add-all-state
      tableSet(to, entry->key, entry->value);
    }
  }
}
//< table-add-all
//> table-find-string
ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash) {
  if (table->count == 0) return NULL;

//> Optimization find-string-index
  uint32_t index = hash & (table->capacity - 1);
//< Optimization find-string-index
  for (;;) {
    Entry* entry = &table->entries[index];
//> Challenge 20.1 find-string-state
    if (entry->state == ENTRY_EMPTY) {
      // Stop if we find an empty non-tombstone entry.
      return NULL;
    } else if (entry->state == ENTRY_OCCUPIED && IS_STRING(entry->key)) {
      ObjString* key = AS_STRING(entry->key);
      if (key->length == length &&
          key->hash == hash &&
          memcmp(key->chars, chars, length) == 0) {
        // We found it.
        return key;
      }
    }
    // Tombstones, and occupied-but-non-matching entries, keep probing.
//< Challenge 20.1 find-string-state

//> Optimization find-string-next
    index = (index + 1) & (table->capacity - 1);
//< Optimization find-string-next
  }
}
//< table-find-string
//> Garbage Collection table-remove-white
void tableRemoveWhite(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
//> Challenge 20.1 remove-white-state
    if (entry->state == ENTRY_OCCUPIED && IS_OBJ(entry->key) &&
        !AS_OBJ(entry->key)->isMarked) {
//< Challenge 20.1 remove-white-state
      tableDelete(table, entry->key);
    }
  }
}
//< Garbage Collection table-remove-white
//> Garbage Collection mark-table
void markTable(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
//> Challenge 20.1 mark-table-state
    if (entry->state == ENTRY_OCCUPIED && IS_OBJ(entry->key)) {
      markObject(AS_OBJ(entry->key));
    }
//< Challenge 20.1 mark-table-state
    markValue(entry->value);
  }
}
//< Garbage Collection mark-table
