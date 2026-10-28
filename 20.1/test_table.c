// Standalone C-level test for Challenge 20.1: Table now supports
// Value keys of any primitive type (number, bool, nil), not just
// ObjString*. There's no Lox-syntax way to exercise this directly
// (Lox doesn't expose a user-facing Map type), so this drives the
// table.c API straight from C.
#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

static int pass = 0;
static int fail = 0;

static void check(const char* desc, bool ok) {
  if (ok) {
    printf("PASS: %s\n", desc);
    pass++;
  } else {
    printf("FAIL: %s\n", desc);
    fail++;
  }
}

int main() {
  initAllocator(1024 * 1024);
  initVM();

  Table table;
  initTable(&table);

  tableSet(&table, NUMBER_VAL(1), NUMBER_VAL(100));
  tableSet(&table, NUMBER_VAL(2), NUMBER_VAL(200));
  tableSet(&table, NUMBER_VAL(3.5), NUMBER_VAL(350));
  tableSet(&table, BOOL_VAL(true), OBJ_VAL(copyString("yes", 3)));
  tableSet(&table, BOOL_VAL(false), OBJ_VAL(copyString("no", 2)));
  tableSet(&table, NIL_VAL, OBJ_VAL(copyString("nothing", 7)));

  ObjString* key = copyString("hello", 5);
  tableSet(&table, OBJ_VAL(key), NUMBER_VAL(42));

  Value out;

  check("get number key 1", tableGet(&table, NUMBER_VAL(1), &out) && AS_NUMBER(out) == 100);
  check("get number key 2", tableGet(&table, NUMBER_VAL(2), &out) && AS_NUMBER(out) == 200);
  check("get number key 3.5", tableGet(&table, NUMBER_VAL(3.5), &out) && AS_NUMBER(out) == 350);
  check("get bool key true", tableGet(&table, BOOL_VAL(true), &out) && IS_STRING(out));
  check("get bool key false", tableGet(&table, BOOL_VAL(false), &out) && IS_STRING(out));
  check("get nil key", tableGet(&table, NIL_VAL, &out) && IS_STRING(out));
  check("get string key", tableGet(&table, OBJ_VAL(key), &out) && AS_NUMBER(out) == 42);
  check("missing number key correctly not found", !tableGet(&table, NUMBER_VAL(999), &out));

  bool wasNew = tableSet(&table, NUMBER_VAL(1), NUMBER_VAL(111));
  check("overwriting existing number key reports not-new", wasNew == false);
  check("overwritten number key value updated", tableGet(&table, NUMBER_VAL(1), &out) && AS_NUMBER(out) == 111);

  check("delete bool-true key", tableDelete(&table, BOOL_VAL(true)));
  check("deleted key no longer found", !tableGet(&table, BOOL_VAL(true), &out));
  check("bool-false key intact after delete (tombstone didn't break probing)",
        tableGet(&table, BOOL_VAL(false), &out));
  check("nil key intact after delete", tableGet(&table, NIL_VAL, &out));
  check("string key intact after delete", tableGet(&table, OBJ_VAL(key), &out));

  for (int i = 0; i < 100; i++) {
    tableSet(&table, NUMBER_VAL(i), NUMBER_VAL(i * 2));
  }
  bool okAfterResize = true;
  for (int i = 0; i < 100; i++) {
    Value v;
    if (!tableGet(&table, NUMBER_VAL(i), &v) || AS_NUMBER(v) != i * 2) {
      okAfterResize = false;
      break;
    }
  }
  check("all 100 number keys survive a resize", okAfterResize);
  check("string key survives resize", tableGet(&table, OBJ_VAL(key), &out) && AS_NUMBER(out) == 42);
  check("nil key survives resize", tableGet(&table, NIL_VAL, &out));

  printf("\n%d passed, %d failed\n", pass, fail);

  freeTable(&table);
  freeVM();

  return fail == 0 ? 0 : 1;
}
