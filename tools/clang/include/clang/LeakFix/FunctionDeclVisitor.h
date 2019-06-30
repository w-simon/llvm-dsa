#ifndef LEAKFIX_FUNCTIONDECLVISITOR_H
#define LEAKFIX_FUNCTIONDECLVISITOR_H
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTContext.h"
#include <set>

using clang::RecursiveASTVisitor;
using clang::ASTContext;
using clang::FunctionDecl;
using clang::Decl;

class FunctionDeclVisitor : public RecursiveASTVisitor<FunctionDeclVisitor>{
public:
  FunctionDeclVisitor(ASTContext *C) : Ctx(C){}
  bool VisitDecl(Decl *D){
    if (FunctionDecl *F = clang::dyn_cast<FunctionDecl>(D)){
      if (F->getBody()){
        functionDecls.insert(F);
      }
    }
    return true;
  }

  std::set<FunctionDecl *> functionDecls;
  ASTContext *Ctx;
};
#endif
