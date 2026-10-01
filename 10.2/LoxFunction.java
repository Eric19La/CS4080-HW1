//> Functions lox-function
package com.craftinginterpreters.lox;

import java.util.List;

class LoxFunction implements LoxCallable {
  private final Stmt.Function declaration;
//> Challenge 10.2 expr-declaration-field
  private final Expr.Function exprDeclaration;
//< Challenge 10.2 expr-declaration-field
//> closure-field
  private final Environment closure;

//< closure-field
/* Functions lox-function < Functions closure-constructor
  LoxFunction(Stmt.Function declaration) {
*/
/* Functions closure-constructor < Classes is-initializer-field
  LoxFunction(Stmt.Function declaration, Environment closure) {
*/
//> Classes is-initializer-field
  private final boolean isInitializer;

  LoxFunction(Stmt.Function declaration, Environment closure,
              boolean isInitializer) {
    this.isInitializer = isInitializer;
//< Classes is-initializer-field
//> closure-constructor
    this.closure = closure;
//< closure-constructor
    this.declaration = declaration;
//> Challenge 10.2 null-expr-declaration
    this.exprDeclaration = null;
//< Challenge 10.2 null-expr-declaration
  }
//> Challenge 10.2 anonymous-function-constructor
  LoxFunction(Expr.Function declaration, Environment closure) {
    this.isInitializer = false;
    this.closure = closure;
    this.declaration = null;
    this.exprDeclaration = declaration;
  }
//< Challenge 10.2 anonymous-function-constructor
//> Classes bind-instance
  LoxFunction bind(LoxInstance instance) {
    Environment environment = new Environment(closure);
    environment.define("this", instance);
/* Classes bind-instance < Classes lox-function-bind-with-initializer
    return new LoxFunction(declaration, environment);
*/
//> lox-function-bind-with-initializer
    return new LoxFunction(declaration, environment,
                           isInitializer);
//< lox-function-bind-with-initializer
  }
//< Classes bind-instance
//> Challenge 10.2 params-body-helpers
  private List<Token> params() {
    return declaration != null ? declaration.params : exprDeclaration.params;
  }

  private List<Stmt> body() {
    return declaration != null ? declaration.body : exprDeclaration.body;
  }
//< Challenge 10.2 params-body-helpers
//> function-to-string
  @Override
  public String toString() {
//> Challenge 10.2 anonymous-tostring
    if (declaration == null) return "<fn anonymous>";
//< Challenge 10.2 anonymous-tostring
    return "<fn " + declaration.name.lexeme + ">";
  }
//< function-to-string
//> function-arity
  @Override
  public int arity() {
    return params().size();
  }
//< function-arity
//> function-call
  @Override
  public Object call(Interpreter interpreter,
                     List<Object> arguments) {
/* Functions function-call < Functions call-closure
    Environment environment = new Environment(interpreter.globals);
*/
//> call-closure
    Environment environment = new Environment(closure);
//< call-closure
    List<Token> params = params();
    for (int i = 0; i < params.size(); i++) {
      environment.define(params.get(i).lexeme,
          arguments.get(i));
    }

/* Functions function-call < Functions catch-return
    interpreter.executeBlock(declaration.body, environment);
*/
//> catch-return
    try {
      interpreter.executeBlock(body(), environment);
    } catch (Return returnValue) {
//> Classes early-return-this
      if (isInitializer) return closure.getAt(0, "this");

//< Classes early-return-this
      return returnValue.value;
    }
//< catch-return
//> Classes return-this

    if (isInitializer) return closure.getAt(0, "this");
//< Classes return-this
    return null;
  }
//< function-call
}
