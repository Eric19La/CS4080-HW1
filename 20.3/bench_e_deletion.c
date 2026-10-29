// Benchmark E: deletion-heavy workloads. Lox has no delete syntax, so
// this has to drive table.c directly from C, same as the Challenge
// 20.1 test harness.
//
// Two scenarios:
//   1. "Steady churn": repeatedly delete and re-insert the SAME N
//      keys. Tombstones get reused immediately by the next insert of
//      the same key, so table->count should stay flat.
//   2. "Churn with fresh keys": each cycle deletes N keys and inserts
//      N *different* keys it's never seen before. The tombstones left
//      behind by this cycle's deletes are NOT where the next cycle's
//      new keys happen to land, so they accumulate until the next
//      resize sweeps them -- this is the shape of workload the
//      chapter's own wording ("choke when deletions are common") is
//      pointing at.
#include <stdio.h>
#include <time.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define N 500
#define CYCLES 2000

static double elapsed(struct timespec start, struct timespec end) {
  return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main() {
  initAllocator(256 * 1024 * 1024);
  initVM();

  struct timespec t0, t1;

  // Scenario 1: steady churn, same N keys, over and over.
  {
    Table table;
    initTable(&table);
    for (int i = 0; i < N; i++) tableSet(&table, NUMBER_VAL(i), NUMBER_VAL(i));

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int cycle = 0; cycle < CYCLES; cycle++) {
      for (int i = 0; i < N; i++) tableDelete(&table, NUMBER_VAL(i));
      for (int i = 0; i < N; i++) tableSet(&table, NUMBER_VAL(i), NUMBER_VAL(i));
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    printf("Scenario 1 (steady churn, same keys):\n");
    printf("  time: %f s\n", elapsed(t0, t1));
    printf("  final table->count: %d (live keys: %d)\n", table.count, N);
    printf("  final table->capacity: %d\n\n", table.capacity);

    freeTable(&table);
  }

  // Scenario 2: churn with a fresh batch of keys every cycle.
  {
    Table table;
    initTable(&table);
    int nextKey = 0;
    for (int i = 0; i < N; i++) tableSet(&table, NUMBER_VAL(nextKey++), NUMBER_VAL(0));

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int cycle = 0; cycle < CYCLES; cycle++) {
      for (int i = 0; i < N; i++) {
        tableDelete(&table, NUMBER_VAL(cycle * N + i));
      }
      for (int i = 0; i < N; i++) {
        tableSet(&table, NUMBER_VAL(nextKey++), NUMBER_VAL(0));
      }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);

    printf("Scenario 2 (churn with fresh keys each cycle):\n");
    printf("  time: %f s\n", elapsed(t0, t1));
    printf("  final table->count: %d (live keys: %d)\n", table.count, N);
    printf("  final table->capacity: %d\n", table.capacity);
    printf("  total keys ever inserted: %d\n", nextKey);

    freeTable(&table);
  }

  freeVM();
  return 0;
}
