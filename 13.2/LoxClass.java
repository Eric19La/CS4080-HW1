//> Classes lox-class
package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.Collections;
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
//> Challenge 13.2 beta-dispatch
  // A method "belongs to" a class if the class defines it directly or
  // mixes it in -- mixins don't get their own position in the BETA
  // chain, they're folded into whichever class lists them.
  private LoxFunction findOwnMethod(String name) {
    if (methods.containsKey(name)) return methods.get(name);
    for (LoxClass mixin : mixins) {
      LoxFunction method = mixin.findMethod(name);
      if (method != null) return method;
    }
    return null;
  }

  // Root-to-leaf chain of classes for THIS class (the actual runtime
  // class of some instance), i.e. [mostDistantAncestor, ..., this].
  private List<LoxClass> ancestorChain() {
    List<LoxClass> chain = new ArrayList<>();
    for (LoxClass c = this; c != null; c = c.superclass) {
      chain.add(c);
    }
    Collections.reverse(chain);
    return chain;
  }

  static class BoundBetaMethod {
    final LoxFunction function;
    final LoxClass definingClass;

    BoundBetaMethod(LoxFunction function, LoxClass definingClass) {
      this.function = function;
      this.definingClass = definingClass;
    }
  }

  // Called on the instance's actual class. Finds the method defined by
  // the class CLOSEST TO THE ROOT of the hierarchy -- BETA calls the
  // highest override first, unlike normal OOP's "most derived wins."
  BoundBetaMethod findTopmostMethod(String name) {
    for (LoxClass c : ancestorChain()) {
      LoxFunction method = c.findOwnMethod(name);
      if (method != null) return new BoundBetaMethod(method, c);
    }
    return null;
  }

  // Called on the instance's actual class, from inside inner(). Finds
  // the nearest override strictly BELOW currentDefiningClass, walking
  // toward this (the instance's actual class). Returns null if there
  // isn't one -- inner() does nothing in that case.
  BoundBetaMethod findNextMethod(String name, LoxClass currentDefiningClass) {
    List<LoxClass> chain = ancestorChain();
    int index = chain.indexOf(currentDefiningClass);
    for (int i = index + 1; i < chain.size(); i++) {
      LoxFunction method = chain.get(i).findOwnMethod(name);
      if (method != null) return new BoundBetaMethod(method, chain.get(i));
    }
    return null;
  }
//< Challenge 13.2 beta-dispatch

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
//> Challenge 13.2 call-uses-topmost-init
    BoundBetaMethod initializer = findTopmostMethod("init");
    if (initializer != null) {
      initializer.function.bindBeta(instance, initializer.definingClass)
          .call(interpreter, arguments);
    }
//< Challenge 13.2 call-uses-topmost-init

//< lox-class-call-initializer
    return instance;
  }

  @Override
  public int arity() {
/* Classes lox-class-call-arity < Classes lox-initializer-arity
    return 0;
*/
//> lox-initializer-arity
//> Challenge 13.2 arity-uses-topmost-init
    BoundBetaMethod initializer = findTopmostMethod("init");
    if (initializer == null) return 0;
    return initializer.function.arity();
//< Challenge 13.2 arity-uses-topmost-init
//< lox-initializer-arity
  }
//< lox-class-call-arity
}
