//> Classes lox-instance
package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

class LoxInstance {
//> Challenge 13.2 klass-package-visible
  // Package-visible (not private) so LoxFunction.bindBeta() can read the
  // instance's actual runtime class to walk its BETA dispatch chain.
  LoxClass klass;
//< Challenge 13.2 klass-package-visible
//> lox-instance-fields
  private final Map<String, Object> fields = new HashMap<>();
//< lox-instance-fields

  LoxInstance(LoxClass klass) {
    this.klass = klass;
  }

//> lox-instance-get-property
  Object get(Token name, Interpreter interpreter) {
    if (fields.containsKey(name.lexeme)) {
      return fields.get(name.lexeme);
    }

//> lox-instance-get-method
//> Challenge 13.2 get-uses-topmost
    LoxClass.BoundBetaMethod found = klass.findTopmostMethod(name.lexeme);
/* Classes lox-instance-get-method < Classes lox-instance-bind-method
    if (method != null) return method;
*/
//> lox-instance-bind-method
    if (found != null) {
      LoxFunction bound = found.function.bindBeta(this, found.definingClass);
//< Challenge 13.2 get-uses-topmost
//> Challenge 12.2 auto-invoke-getter
      if (bound.isGetter()) {
        return bound.call(interpreter, new ArrayList<>());
      }
//< Challenge 12.2 auto-invoke-getter
      return bound;
    }
//< lox-instance-bind-method

//< lox-instance-get-method
    throw new RuntimeError(name, // [hidden]
        "Undefined property '" + name.lexeme + "'.");
  }
//< lox-instance-get-property
//> lox-instance-set-property
  void set(Token name, Object value) {
    fields.put(name.lexeme, value);
  }
//< lox-instance-set-property
  @Override
  public String toString() {
    return klass.name + " instance";
  }
}
