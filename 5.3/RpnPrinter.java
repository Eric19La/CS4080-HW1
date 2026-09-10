package com.craftinginterpreters.lox;

class RpnPrinter implements Expr.Visitor<String> {
  String print(Expr expr) {
    return expr.accept(this);
  }

  @Override
  public String visitBinaryExpr(Expr.Binary expr) {
    return expr.left.accept(this) + " " +
           expr.right.accept(this) + " " +
           expr.operator.lexeme;
  }

  @Override
  public String visitGroupingExpr(Expr.Grouping expr) {
    return expr.expression.accept(this);
  }

  @Override
  public String visitLiteralExpr(Expr.Literal expr) {
    if (expr.value == null) return "nil";
    return expr.value.toString();
  }

  @Override
  public String visitUnaryExpr(Expr.Unary expr) {
    String operator = expr.operator.lexeme;
    if (expr.operator.type == TokenType.MINUS) {
      // Can't reuse "-" for both unary and binary in RPN.
      operator = "~";
    }
    return expr.right.accept(this) + " " + operator;
  }

  // The RPN challenge only deals with plain arithmetic expressions
  @Override
  public String visitAssignExpr(Expr.Assign expr) {
    throw new UnsupportedOperationException("RpnPrinter does not support assignment expressions.");
  }

  @Override
  public String visitCallExpr(Expr.Call expr) {
    throw new UnsupportedOperationException("RpnPrinter does not support call expressions.");
  }

  @Override
  public String visitGetExpr(Expr.Get expr) {
    throw new UnsupportedOperationException("RpnPrinter does not support get expressions.");
  }

  @Override
  public String visitLogicalExpr(Expr.Logical expr) {
    throw new UnsupportedOperationException("RpnPrinter does not support logical expressions.");
  }

  @Override
  public String visitSetExpr(Expr.Set expr) {
    throw new UnsupportedOperationException("RpnPrinter does not support set expressions.");
  }

  @Override
  public String visitSuperExpr(Expr.Super expr) {
    throw new UnsupportedOperationException("RpnPrinter does not support super expressions.");
  }

  @Override
  public String visitThisExpr(Expr.This expr) {
    throw new UnsupportedOperationException("RpnPrinter does not support this expressions.");
  }

  @Override
  public String visitVariableExpr(Expr.Variable expr) {
    throw new UnsupportedOperationException("RpnPrinter does not support variable expressions.");
  }

  public static void main(String[] args) {
    Expr expression = new Expr.Binary(
        new Expr.Grouping(
            new Expr.Binary(
                new Expr.Literal(1),
                new Token(TokenType.PLUS, "+", null, 1),
                new Expr.Literal(2))),
        new Token(TokenType.STAR, "*", null, 1),
        new Expr.Grouping(
            new Expr.Binary(
                new Expr.Literal(4),
                new Token(TokenType.MINUS, "-", null, 1),
                new Expr.Literal(3))));

    System.out.println(new RpnPrinter().print(expression));
    // Should print: 1 2 + 4 3 - *
  }
}
