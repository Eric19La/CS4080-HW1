// Challenge 9.3: control-flow signal used to unwind out of the nearest
// enclosing loop when a 'break' statement executes.
package com.craftinginterpreters.lox;

class BreakException extends RuntimeException {
  BreakException() {
    super(null, null, false, false);
  }
}
