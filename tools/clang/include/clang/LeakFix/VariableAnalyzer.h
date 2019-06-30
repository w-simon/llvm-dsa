#ifndef LEAKFIX_VARIABLEANALYZER_H
#define LEAKFIX_VARIABLEANALYZER_H


#include "clang/LeakFix/NodeRecognizer.h"
#include "clang/Analysis/CFG.h"
#include "clang/AST/Decl.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/LoopInfo.h"
//#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "dsa/DSGraph.h"
#include <set>
#include <string>
#include <queue>
#include <map>
#include <utility>
using clang::CFGBlock;
using clang::VarDecl;
using llvm::Function;
using llvm::DSGraph;
using llvm::DSNode;
using llvm::Value;
using llvm::BasicBlock;
using llvm::Instruction;
using llvm::LoopInfo;
using std::pair;
using std::vector;
using std::map;
using std::set;
using std::queue;
using std::string;

class VariableAnalyzer{
public:
  typedef pair<CFGBlock*, CFGBlock::iterator> Element; 
  typedef map<Element, set<VarDecl*> > VarMap;
  typedef set<int*> AllocSet;
  enum PPType{in,out};
  typedef pair<BasicBlock *, BasicBlock::iterator> ProgramNode;
  typedef pair<ProgramNode, PPType> ProgramPoint;
  class RDComp{
    public:
      bool operator()(const ProgramPoint &pp1, const ProgramPoint &pp2){
        BasicBlock *b1 = pp1.first.first;
        BasicBlock *b2 = pp2.first.first;
        Instruction *i1 = &*pp1.first.second;
        Instruction *i2 = &*pp2.first.second;
        if (b1 < b2)
          return true;
        if (b1 == b2 && i1 < i2)
          return true;
        if (b1 == b2 && i1 == i2 && pp1.second < pp2.second)
          return true;
        return false;
      }
  };

  typedef map<ProgramPoint, set<Value*> *, RDComp> RDResult; 
public:
  VariableAnalyzer(NodeRecognizer *nr);
  VariableAnalyzer();
  ~VariableAnalyzer();

  //RDAnalysis:analyze
  void analyzeRD();
  void analyzeInit();
  void printRDResult();
  void printValueSet(set<Value*> *s);
  void pushSuccsToWorkList(queue<ProgramPoint> *workList, BasicBlock *BB, BasicBlock::iterator BI);

  bool canFix(set<int> *s, CFGBlock* in, CFGBlock::iterator inIter, CFGBlock* out, CFGBlock::iterator outIter, map<set<int>, std::string> &varMap);
private:
  //Judge whether CurNode is reachable to N
  bool reachable(DSNode* CurNode, DSNode* N, vector<int> &level);
  void clearExit();
  void traverse(DSNode *, set<DSNode*> &, DSNode*, map<set<int>, std::string> &, set<int> &, std::string, bool &);

  //According to level, parse the source representation of I
  std::string parseLevel(Value* I, vector<int> &level);
  //RDAnalysis: initialize
  void initializeRD(queue<ProgramPoint> *);
  //RDAnalysis:flow
  void flowRD(queue<ProgramPoint> *workList);
  void loopClear();
  bool checkAddTempVar(set<int>*);
  void innerBlockUpdate(pair<BasicBlock*, Instruction*> p, Instruction *Src, queue<pair<BasicBlock*, Instruction*> > &workList, set<BasicBlock*> &visited, LoopInfo *li);
  void analyzeNode(DSNode*, AllocNode*, Function*);
  void handleTopLevel(DSNode*, AllocNode*, Function*);
  void checkAddressTaken(DSNode*, AllocNode*, Function*);
  void analyzeArg(CallInst*, AllocNode*, Function*);
  void analyzeRet(CallInst*, AllocNode*, Function*);
  set<set<Value*>*> buildEqualSet(DSNode*, AllocNode*, Function*);
  bool shareSame(llvm::GetElementPtrInst *, llvm::GetElementPtrInst *, map<Value*, set<Value*>*> &);

private:
  Function *F;
  DSGraph *G;
  set<AllocSet*> analyzed;
  std::map<int, bool> allocMapped;
  RDResult RDResultMap;
  NodeRecognizer *NR;
  LoopInfo *li;
  set<Function*> initAnalyzed;
  set<Function*> initConfirmed;
  set<Instruction*> initInst;
public:
  set<AllocNode*> undefSet;
};

#endif
