#ifndef LEAKFIX_NODERECOGNIZER_H
#define LEAKFIX_NODERECOGNIZER_H

#include "dsa/DSGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/GlobalVariable.h"
#include "clang/Analysis/CFG.h"
#include <map>
#include <set>
#include <utility>
using llvm::Function;
using llvm::DSGraph;
using llvm::DSNode;
using llvm::Value;
using llvm::GlobalVariable;
using clang::CFG;
using clang::Stmt;
using clang::CFGBlock;
using llvm::CallInst;
using llvm::GlobalValue;

struct AllocNode{
  std::pair<int, int> node;
  int ID;
  CallInst *callInst;
  AllocNode(std::pair<int, int> p, int i, CallInst* c=NULL) : node(p), ID(i), callInst(c){}
};

class NodeRecognizer{
public:
  Function *F;
  CFG *cfg;
  DSGraph *G;
  std::map<Stmt*, std::pair<CFGBlock*, CFGBlock::iterator> > stmtToNodeMap;
  std::map<DSNode*, std::set<Value*> *> DSNodeToValueMap;
  std::set<AllocNode*> mNodes;
  std::map<AllocNode*, std::set<std::pair<int, int> > *> fNodesMap;
  std::map<AllocNode*, std::set<std::pair<int, int> > *> uNodesMap;
  std::map<std::pair<CFGBlock*, CFGBlock::iterator>, std::pair<int,int> > pairMap;
  std::map<std::pair<int, int>, std::pair<CFGBlock*, CFGBlock::iterator> > blockMap;
  std::map<int, CFGBlock*> blockIDMap;
  static std::map<GlobalVariable*, Function*> handleGlobalMap;
  static std::map<Function*, std::set<llvm::GlobalVariable*> > invHandleGlobalMap;
  static std::set<Function*> globalFuncSet;
public:
  NodeRecognizer();
  NodeRecognizer(Function *, CFG*, DSGraph*);
  ~NodeRecognizer();
  void printPairMap();
  void printBlockMap();
  void recognizeLocal();
  static void recognizeGlobal(llvm::Module*);
  void printResult();
  void printToStmt(std::pair<int,int> idPair);
  void printDSNodeToValueMap();
  static bool pathExist(std::set<Value*> &, Function*);
private:
  void constructStmtToNodeMap();
  void constructDSNodeToValueMap();
  void traverse(DSNode*);
  static void constructGVSet(std::set<GlobalVariable*> &, llvm::Module*);
  static void analyzeInitial(std::set<Function*> &, llvm::GlobalVariable*, llvm::Module*);
  static void analyzeUseFree(std::set<Function*> &, std::set<Function*> &, llvm::GlobalVariable*, llvm::Module*);
  static void analyze(std::set<Function*> &, std::set<Function*> &, std::set<Function*> &, llvm::GlobalVariable*, llvm::Module*);
};
#endif
