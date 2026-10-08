//> Statements and State environment-class
package com.craftinginterpreters.lox;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class Environment {
//> enclosing-field
  final Environment enclosing;
//< enclosing-field
//> Challenge 8.2 uninitialized-sentinel
  static final Object UNINITIALIZED = new Object();
//< Challenge 8.2 uninitialized-sentinel
  private final Map<String, Object> values = new HashMap<>();
//> Challenge 11.4 slots-field
  // Parallel, index-addressable storage for resolver-driven local variable
  // access (see Resolver's per-scope slot numbering) -- avoids a map
  // lookup by name on the hot path. 'values' above is kept for the
  // dynamically-resolved global scope and a couple of internal by-name
  // accesses (this/super plumbing) that don't go through the resolver.
  private final List<Object> slots = new ArrayList<>();
//< Challenge 11.4 slots-field
//> environment-constructors
  Environment() {
    enclosing = null;
  }

  Environment(Environment enclosing) {
    this.enclosing = enclosing;
  }
//< environment-constructors
//> environment-get

  Object get(Token name) {
    if (values.containsKey(name.lexeme)) {
      Object value = values.get(name.lexeme);
//> Challenge 8.2 check-uninitialized
      if (value == UNINITIALIZED) {
        throw new RuntimeError(name,
            "Variable '" + name.lexeme + "' must be initialized before use.");
      }
//< Challenge 8.2 check-uninitialized
      return value;
    }
//> environment-get-enclosing

    if (enclosing != null) return enclosing.get(name);
//< environment-get-enclosing

    throw new RuntimeError(name,
        "Undefined variable '" + name.lexeme + "'.");
  }

//< environment-get
//> environment-assign
  void assign(Token name, Object value) {
    if (values.containsKey(name.lexeme)) {
      values.put(name.lexeme, value);
      return;
    }

//> environment-assign-enclosing
    if (enclosing != null) {
      enclosing.assign(name, value);
      return;
    }

//< environment-assign-enclosing
    throw new RuntimeError(name,
        "Undefined variable '" + name.lexeme + "'.");
  }
//< environment-assign
//> environment-define
  void define(String name, Object value) {
    values.put(name, value);
//> Challenge 11.4 define-slot
    slots.add(value);
//< Challenge 11.4 define-slot
  }
//< environment-define
//> Resolving and Binding ancestor
  Environment ancestor(int distance) {
    Environment environment = this;
    for (int i = 0; i < distance; i++) {
      environment = environment.enclosing; // [coupled]
    }

    return environment;
  }
//< Resolving and Binding ancestor
//> Resolving and Binding get-at
  Object getAt(int distance, String name) {
    return ancestor(distance).values.get(name);
  }
//< Resolving and Binding get-at
//> Challenge 11.4 get-at-slot
  Object getAt(int distance, int slot) {
    return ancestor(distance).slots.get(slot);
  }
//< Challenge 11.4 get-at-slot
//> Resolving and Binding assign-at
  void assignAt(int distance, Token name, Object value) {
    ancestor(distance).values.put(name.lexeme, value);
  }
//< Resolving and Binding assign-at
//> Challenge 11.4 assign-at-slot
  void assignAt(int distance, int slot, Object value) {
    ancestor(distance).slots.set(slot, value);
  }
//< Challenge 11.4 assign-at-slot
//> omit
  @Override
  public String toString() {
    String result = values.toString();
    if (enclosing != null) {
      result += " -> " + enclosing.toString();
    }

    return result;
  }
//< omit
}
