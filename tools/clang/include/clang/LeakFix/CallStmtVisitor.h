#ifndef LEAKFIX_CALLSTMTVISITOR_H
#define LEAKFIX_CALLSTMTVISITOR_H
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Analysis/CFG.h"
#include "clang/AST/PrettyPrinter.h"
#include <set>

using clang::StmtVisitor;
using clang::ASTContext;
using clang::FunctionDecl;
using clang::Decl;
using clang::Expr;
using clang::CallExpr;
using clang::PrintingPolicy;

class CallStmtVisitor : public StmtVisitor<CallStmtVisitor>{
public:
  CallStmtVisitor(ASTContext *C) : Ctx(C){}
  void VisitExpr(Expr *E){
    if (CallExpr *CE = llvm::dyn_cast<CallExpr>(E)){
      FunctionDecl *FD = CE->getDirectCallee();
      if (FD){
        if (!topNameFound){
          topName = FD->getName();
          topNameFound = true;
        }
        nameVector.push_back(FD->getName().str());
      }
    }
    for (Stmt::child_iterator CI = E->child_begin(); CI != E->child_end(); ++CI){
      Visit(*CI);
    }
  }

  ASTContext *Ctx;
  std::vector<std::string> nameVector;
  std::string topName;
  bool topNameFound = false;
};
class CallStmtReplaceVisitor : public StmtVisitor<CallStmtReplaceVisitor>{
public:
  CallStmtReplaceVisitor(ASTContext *C, Stmt *S, std::string varName, std::string funcName) : Ctx(C), tempVar(varName), repFuncName(funcName){
    string tmp;
    llvm::raw_string_ostream os(tmp);
    S->printPretty(os, 0, PrintingPolicy(Ctx->getLangOpts()));
    stmtStr = os.str();
  }
  void VisitExpr(Expr *E){
    if (CallExpr *CE = llvm::dyn_cast<CallExpr>(E)){
      FunctionDecl *FD = CE->getDirectCallee();
      if (FD){
        if (repFuncName == FD->getName()){
          clang::QualType T = CE->getType();
          string type = T.getAsString();
          string s;
          llvm::raw_string_ostream os1(s);
          os1 << type << tempVar << " = ";
          CE->printPretty(os1, 0, PrintingPolicy(Ctx->getLangOpts()));
          os1 << "; "; 

          string tmp;
          llvm::raw_string_ostream os(tmp);
          CE->printPretty(os, 0, PrintingPolicy(Ctx->getLangOpts()));
          std::string exprString = os.str();
          std::size_t index = stmtStr.find(exprString);
          assert(index != string::npos && "Expr not included in the statement!");
          std::size_t length = exprString.length();
          result = os1.str() + stmtStr.substr(0, index) + tempVar + stmtStr.substr(index + length) + "; ";
          return;
        }
      }
    }
    for (Stmt::child_iterator CI = E->child_begin(); CI != E->child_end(); ++CI){
      Visit(*CI);
    }
  }

  ASTContext *Ctx;
  Stmt *S;
  std::string stmtStr;
  std::string repFuncName;
  std::string tempVar;
  std::string result;
};
#endif
