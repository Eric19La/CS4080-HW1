//> Classes lox-class
package com.craftinginterpreters.lox;

import java.util.List;
import java.util.Map;

/* Classes lox-class < Classes lox-class-callable
class LoxClass {
*/
//> lox-class-callable
//> Challenge 12.1 lox-class-extends-instance
class LoxClass extends LoxInstance implements LoxCallable {
//< Challenge 12.1 lox-class-extends-instance
//< lox-class-callable
  final String name;
//> Inheritance lox-class-superclass-field
  final LoxClass superclass;
//< Inheritance lox-class-superclass-field
//> Challenge 13.1 mixins-field
  private final List<LoxClass> mixins;
//< Challenge 13.1 mixins-field
/* Classes lox-class < Classes lox-class-methods

  LoxClass(String name) {
    this.name = name;
  }
*/
//> lox-class-methods
  private final Map<String, LoxFunction> methods;

/* Classes lox-class-methods < Inheritance lox-class-constructor
  LoxClass(String name, Map<String, LoxFunction> methods) {
*/
//> Inheritance lox-class-constructor
//> Challenge 12.1 lox-class-metaclass-constructor
  LoxClass(String name, LoxClass superclass,
           Map<String, LoxFunction> methods, List<LoxClass> mixins,
           LoxClass metaclass) {
    super(metaclass);
    this.superclass = superclass;
//< Inheritance lox-class-constructor
    this.name = name;
    this.methods = methods;
//> Challenge 13.1 mixins-assign
    this.mixins = mixins;
//< Challenge 13.1 mixins-assign
  }
//< Challenge 12.1 lox-class-metaclass-constructor
//< lox-class-methods
//> lox-class-find-method
  LoxFunction findMethod(String name) {
    if (methods.containsKey(name)) {
      return methods.get(name);
    }

//> Challenge 13.1 find-method-check-mixins
    // Checked before the superclass: a class's own mixed-in capabilities
    // take priority over what it inherits, but its own directly-defined
    // methods (checked above) always win over both.
    for (LoxClass mixin : mixins) {
      LoxFunction method = mixin.findMethod(name);
      if (method != null) return method;
    }
//< Challenge 13.1 find-method-check-mixins
//> Inheritance find-method-recurse-superclass
    if (superclass != null) {
      return superclass.findMethod(name);
    }

//< Inheritance find-method-recurse-superclass
    return null;
  }
//< lox-class-find-method

  @Override
  public String toString() {
    return name;
  }
//> lox-class-call-arity
  @Override
  public Object call(Interpreter interpreter,
                     List<Object> arguments) {
    LoxInstance instance = new LoxInstance(this);
//> lox-class-call-initializer
    LoxFunction initializer = findMethod("init");
    if (initializer != null) {
      initializer.bind(instance).call(interpreter, arguments);
    }

//< lox-class-call-initializer
    return instance;
  }

  @Override
  public int arity() {
/* Classes lox-class-call-arity < Classes lox-initializer-arity
    return 0;
*/
//> lox-initializer-arity
    LoxFunction initializer = findMethod("init");
    if (initializer == null) return 0;
    return initializer.arity();
//< lox-initializer-arity
  }
//< lox-class-call-arity
}
