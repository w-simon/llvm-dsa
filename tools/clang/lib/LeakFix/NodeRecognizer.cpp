#include "clang/LeakFix/NodeRecognizer.h"
#include "dsa/DSGraph.h"
#include "dsa/DataStructure.h"
#include "clang/Basic/SourceManager.h"
#include "clang/AST/ASTContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/CFG.h"
#include "llvm/Summary/Summary.h"
#include <set>
#include <vector>
#include <utility>
#include <queue>
using namespace llvm;
using namespace std;
using clang::CFGBlock;
using clang::CFGElement;
using clang::CFGStmt;
using clang::VarDecl;
using clang::ASTContext;
using clang::SourceLocation;
using clang::SourceRange;
using clang::SourceManager;
using std::set;
using std::vector;
using std::pair;

extern int allocID;
extern map<Function*, Summary*> summaryMap;

extern std::map<Value*, Stmt*> lto_toStmtMapL;
extern std::map<Stmt*, Value*> lto_invStmtMapL;
extern std::map<Value*, VarDecl*> lto_toDeclMapL;
extern std::map<llvm::Value*, clang::FunctionDecl*> lto_toLLVMFuncMapL;
extern std::map<clang::FunctionDecl*, llvm::Value*> lto_toClangFuncMapL;
extern ASTContext *Contex;
set<DSNode*> globalSet;
set<DSNode*> visited;
extern EQTDDataStructures *eqtds;
extern std::map<DSNode*, set<Value*> > nodeToValueMap;
extern bool isOnlyHeap(DSNode *);
extern map<Function*, set<Function*> > callerMap;
extern map<Function*, set<Function*> > calleeMap;

std::map<GlobalVariable*, Function*> NodeRecognizer::handleGlobalMap;
std::map<Function*, std::set<llvm::GlobalVariable*> > NodeRecognizer::invHandleGlobalMap;
std::set<Function*> NodeRecognizer::globalFuncSet;

Stmt* getInsTrace(Instruction *Inst){
  if(lto_toStmtMapL[Inst] == NULL){
  }
  return lto_toStmtMapL[Inst];
}

NodeRecognizer::NodeRecognizer(){}

NodeRecognizer::NodeRecognizer(Function *f, CFG *c, DSGraph *g) : F(f), cfg(c), G(g){}
static void printLocation(Stmt *S){
  SourceManager &SM = Contex->getSourceManager();
  SourceRange SR = S->getSourceRange();
  SourceLocation locBegin = SR.getBegin();
  SourceLocation locEnd = SR.getEnd();
  llvm::errs() << "At Line ";
  llvm::errs() << SM.getSpellingLineNumber(locBegin);
  llvm::errs() << " Column ";
  llvm::errs() << SM.getSpellingColumnNumber(locBegin);
  llvm::errs() << ": ";
}
void NodeRecognizer::printToStmt(pair<int,int> idPair){
  if (blockMap.count(idPair) == 0){
    llvm::errs() << "Printing an empty block. \n";
    return;
  }
  pair<CFGBlock*, CFGBlock::iterator> p = blockMap[idPair];
  llvm::errs() << "Statement: ";
  if (p.second->getAs<CFGStmt>()){
    Stmt *S = const_cast<Stmt*>(p.second->castAs<CFGStmt>().getStmt());
    printLocation(S);
    S->dumpPretty(*Contex);
  }
  llvm::errs() << "\n";
}

//Construct stmt to node map, and pairMap. This is one-time work for each function
void NodeRecognizer::constructStmtToNodeMap(){
  for (CFG::iterator CI = cfg->begin(); CI != cfg->end(); ++CI){
    CFGBlock *block = *CI;
    blockIDMap[block->getBlockID()] = block;
    for (CFGBlock::iterator BI = block->begin(); BI != block->end(); ++BI){
      if (BI->getAs<CFGStmt>()){
        const Stmt *stmt = BI->castAs<CFGStmt>().getStmt();
        stmtToNodeMap[const_cast<Stmt*>(stmt)]=std::make_pair(block, BI);
      }
      int block_id = block->getBlockID();
      int index = BI - block->begin();
      pair<int,int> idPair(block_id,index);
      pair<CFGBlock*, CFGBlock::iterator> blockPair(block, BI);
      pairMap[blockPair] = idPair;
      blockMap[idPair] = blockPair;
    }
  }

}

void NodeRecognizer::printDSNodeToValueMap(){
  llvm::errs() << "\nPrinting DSNodeToValueMap...\n";
  for (std::map<DSNode*, std::set<Value*> *>::iterator I = DSNodeToValueMap.begin();
      I != DSNodeToValueMap.end(); ++I){
    llvm::errs() << "DSNode address: " << I->first << "\n";
    std::set<Value*> *s = I->second;
    if (s){
      for (std::set<Value*>::iterator SI = s->begin(); SI != s->end(); ++SI){
        llvm::errs() << "  Value: " << **SI << "\n";
      }
    }
    llvm::errs() << "\n";
  } 
}

//Construct DSNode to Value map. This is one-time work for each function
void NodeRecognizer::constructDSNodeToValueMap(){
  DSScalarMap &scalarMap = G->getScalarMap();
  for (DSScalarMap::global_iterator GI = scalarMap.global_begin(); GI != scalarMap.global_end(); ++GI){
    DSNodeHandle &H = G->getNodeForValue(*GI);
    GlobalValue *GV = const_cast<GlobalValue*>(*GI);
    Value *V = GV;
    string str = V->getName().str();
    //Avoid llvm-defined global (.*) pollute source code global
    if (str.length() > 0 && str[0] == '.') continue;
    DSNode *N = H.getNode();
    if (N){
      traverse(N);
    }
  }
}

void NodeRecognizer::traverse(DSNode* N ){
  if (!N) return;
  if (visited.find(N) != visited.end()) return;
  visited.insert(N);
  globalSet.insert(N);
  for (unsigned i = 0; i < N->getSize(); ++i){
    if (N->hasLink(i)){
      DSNodeHandle &handle = N->getLink(i);
      if (handle.getNode() && visited.count(handle.getNode()) == 0){
        traverse(handle.getNode());
      }
    }   
  }

}


//Recognize
void NodeRecognizer::recognizeLocal(){
  constructStmtToNodeMap();
  constructDSNodeToValueMap();
  for (Function::arg_iterator FA = F->arg_begin(); FA != F->arg_end(); ++FA){
    DSNodeHandle &H = G->getNodeForValue(FA);
    if (H.getNode()){
      traverse(H.getNode());
    }
  }

  for (DSGraph::retnodes_iterator RI = G->retnodes_begin(); RI != G->retnodes_end(); ++RI){
    DSNodeHandle &H = const_cast<DSNodeHandle&>(RI->second);
    if (H.getNode()){
      traverse(H.getNode());
    }
  }
  
  set<DSNode*> handledGVNodeSet;
  if (globalFuncSet.count(F) != 0){
    set<GlobalVariable*> &s = invHandleGlobalMap[F];
    for (set<GlobalVariable*>::iterator SI = s.begin(); SI != s.end(); ++SI){
      GlobalVariable *GV = *SI;
      DSNodeHandle &H = G->getNodeForValue(GV);
      if (H.getNode()){
        handledGVNodeSet.insert(H.getNode());
      }
    }
  }
  Summary *S = summaryMap[F];
  DSScalarMap &scalarMap = G->getScalarMap();
  for (DSGraph::node_iterator NI = G->node_begin(); NI != G->node_end(); ++NI){
    if (globalSet.count(NI) != 0 && handledGVNodeSet.count(NI) == 0) continue;
    vector<AllocInfo*> *V = S->allocMap[NI];
    if (!V) continue;
    vector<CallInst*> *useV = S->useMap[NI];
    vector<CallInst*> *freeV = S->freeMap[NI];
    for (vector<AllocInfo*>::iterator VI = V->begin(); VI != V->end(); ++VI){
      //Malloc
      CallInst *CI = (*VI)->callInst;
      int id = (*VI)->ID;
      Stmt *stmt = getInsTrace(CI);
      std::pair<CFGBlock*, CFGBlock::iterator> &p = stmtToNodeMap[stmt];
      if (pairMap.count(p) == 0){ continue;}
      AllocNode *allocNode = new AllocNode(pairMap[p], id, CI);
      mNodes.insert(allocNode);
      //Free
      if (freeV){
        for (vector<CallInst*>::iterator freeVI = freeV->begin(); freeVI != freeV->end(); ++freeVI){
          CallInst *freeInst = *freeVI;
          Stmt *freeStmt = getInsTrace(freeInst);
          if (!freeInst || !freeStmt) continue;
          std::pair<CFGBlock*, CFGBlock::iterator> freePair = stmtToNodeMap[freeStmt];
          if (!fNodesMap[allocNode]){
            fNodesMap[allocNode] = new set<pair<int, int> >;
          }  
          fNodesMap[allocNode]->insert(pairMap[freePair]);
        }
      }
      //Use
      if (useV){
        for (vector<CallInst*>::iterator useVI = useV->begin(); useVI != useV->end(); ++useVI){
          CallInst *useInst = *useVI;
          Stmt *useStmt = getInsTrace(useInst);
          if (!useInst || !useStmt || useStmt == stmt) continue;
          std::pair<CFGBlock*, CFGBlock::iterator> usePair = stmtToNodeMap[useStmt];
          if (!uNodesMap[allocNode]){
            uNodesMap[allocNode] = new set<pair<int, int> >;
          }  
          uNodesMap[allocNode]->insert(pairMap[usePair]);
        }
      }
      //Traverse the use for the node corresponding the alloc
      DSNode *N = G->getNodeForValue(CI).getNode();
      set<Value*> &s = nodeToValueMap[N];
      //Get the instruction of the DSHandle
      for (set<Value*>::iterator SI = s.begin(); SI != s.end(); ++SI){
        for (Value::use_iterator UI = (*SI)->use_begin(); UI != (*SI)->use_end(); ++UI){
          if (Instruction *Ins = dyn_cast<Instruction>(*UI)){
            Stmt *useStmt = getInsTrace(Ins);
            if (!useStmt || useStmt == stmt) continue;
            std::pair<CFGBlock*, CFGBlock::iterator> usePair = stmtToNodeMap[useStmt];
            if (!uNodesMap[allocNode]){
              uNodesMap[allocNode] = new set<pair<int, int> >;
            }  
            uNodesMap[allocNode]->insert(pairMap[usePair]);
          }

        }
      }
    }
  } 
}

void NodeRecognizer::constructGVSet(set<GlobalVariable*> &mappedGVSet, llvm::Module *M){
  for (llvm::Module::global_iterator GI = M->global_begin(); GI != M->global_end(); ++GI){
    if (lto_toDeclMapL.find(GI) != lto_toDeclMapL.end()){
      mappedGVSet.insert(GI);
    }
  }
}

void NodeRecognizer::recognizeGlobal(llvm::Module *M){
  set<GlobalVariable*> mappedGVSet;
  constructGVSet(mappedGVSet, M);
  for (set<GlobalVariable*>::iterator SI = mappedGVSet.begin(); SI != mappedGVSet.end(); ++SI){
    GlobalVariable *GV = *SI;
    set<Function*> initFuncSet;
    set<Function*> useFuncSet;
    set<Function*> freeFuncSet;
    analyzeInitial(initFuncSet, GV, M);
    analyzeUseFree(useFuncSet, freeFuncSet, GV, M);
    analyze(initFuncSet, useFuncSet, freeFuncSet, GV, M);
  }
}

void NodeRecognizer::analyzeInitial(std::set<Function*> &initFuncSet, GlobalVariable *GV, Module *M){
  set<Function*> toVisit;
  for (Module::iterator MI = M->begin(); MI != M->end(); ++MI){
    Function *F = MI;
    if (!F->isDeclaration())
      toVisit.insert(F);
  }
  
  map<Function*, set<Instruction*> > funcUseMap;
  for (Value::use_iterator UI = GV->use_begin(); UI != GV->use_end(); ++UI){
    if (Instruction *Ins = dyn_cast<Instruction>(*UI)){
      Function *useFunc = Ins->getParent()->getParent();
      funcUseMap[useFunc].insert(Ins);
    }
  }
  while (!toVisit.empty()){
    Function *F = *toVisit.begin();
    toVisit.erase(F);
    set<Value*> useSet;
    set<Instruction*> &s = funcUseMap[F];
    useSet.insert(s.begin(), s.end());
    for (Function::iterator FI = F->begin(); FI != F->end(); ++FI){
      for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
        Instruction *Ins = BI;
        if (CallInst *CI = dyn_cast<CallInst>(Ins)){
          Function *callee = CI->getCalledFunction();
          if (callee && initFuncSet.count(callee) != 0)
            useSet.insert(CI);
        }
      }
    }
    if (!NodeRecognizer::pathExist(useSet, F)){
      initFuncSet.insert(F);
      for (map<Function*, set<Function*> >::iterator MI = callerMap.begin(); MI != callerMap.end(); ++MI){
        set<Function*> &callerSet = callerMap[F];
        for (set<Function*>::iterator SI = callerSet.begin(); SI != callerSet.end(); ++SI){
          if (initFuncSet.count(*SI) == 0)
            toVisit.insert(*SI);
        }
      }
    }
  }
}

void NodeRecognizer::analyzeUseFree(std::set<Function*> &useFuncSet, std::set<Function*> &freeFuncSet, GlobalVariable *GV, Module *M){
  for (Module::iterator MI = M->begin(); MI != M->end(); ++MI){
    Function *F = MI;
    if (!F->isDeclaration()){
      DSGraph *G = eqtds->getDSGraph(*F);
      DSScalarMap &SM = G->getScalarMap();
      if (SM.count(GV) != 0){
        useFuncSet.insert(F);
      }
      Summary *S = summaryMap[F];
      DSNodeHandle &H = G->getNodeForValue(GV);
      DSNode *N = H.getNode();
      if (N && S->freeMap.find(N) != S->freeMap.end()){
        freeFuncSet.insert(F);
      }
    }
  }
}

void NodeRecognizer::analyze(std::set<Function*> &initFuncSet, std::set<Function*> &useFuncSet, std::set<Function*> &freeFuncSet, GlobalVariable *GV, Module *M){
  std::set<Function*> visited;
  std::set<Function*> allSet;
  map<Function*, set<Function*> > containMapForAllSet;
  map<Function*, set<Function*> > containMapForMidtarget;
  allSet.insert(useFuncSet.begin(), useFuncSet.end());
  allSet.insert(freeFuncSet.begin(), freeFuncSet.end());
  for (set<Function*>::iterator SI = allSet.begin(); SI != allSet.end(); ++SI){
    Function *curFunc = *SI;
    Function *targetFunc = *SI;
    stack<Function*> st;
    st.push(curFunc);
    while (!st.empty()){
      curFunc = st.top();
      st.pop();
      visited.insert(curFunc);
      set<Function*> callerSet = callerMap[curFunc];
      for (set<Function*>::iterator SI2 = callerSet.begin(); SI2 != callerSet.end(); ++SI2){
        Function* F = *SI2;
        if (visited.count(F) == 0)
          st.push(F);
        containMapForAllSet[F].insert(targetFunc);
      }
    }
  }

  set<Function*> midtargetSet;
  for (map<Function*, set<Function*> >::iterator MI = containMapForAllSet.begin(); MI != containMapForAllSet.end(); ++MI){
    if (MI->second.size() == allSet.size()){
      midtargetSet.insert(MI->first);
    }
  }

  for (set<Function*>::iterator SI = midtargetSet.begin(); SI != midtargetSet.end(); ++SI){
    Function *curFunc = *SI;
    Function *targetFunc = *SI;
    stack<Function*> st;
    st.push(curFunc);
    while (!st.empty()){
      curFunc = st.top();
      st.pop();
      visited.insert(curFunc);
      set<Function*> callerSet = callerMap[curFunc];
      for (set<Function*>::iterator SI2 = callerSet.begin(); SI2 != callerSet.end(); ++SI2){
        Function* F = *SI2;
        if (midtargetSet.count(F) == 0) continue;
        if (visited.count(F) == 0)
          st.push(F);
        containMapForMidtarget[F].insert(targetFunc);
      }
    }
  }
  
  Function *handledFunc = NULL;
  map<Function*, set<Function*> >::iterator MI = containMapForMidtarget.begin();
  if (MI != containMapForMidtarget.end()){
    handledFunc = MI->first;
    int funcSize = MI->second.size();
    ++MI;
    while (MI != containMapForMidtarget.end()){
      if (MI->second.size() < funcSize){
        handledFunc = MI->first;
        funcSize = MI->second.size();
      }
      ++MI;
    }
  }

  if (handledFunc){
    handleGlobalMap[GV] = handledFunc;
    invHandleGlobalMap[handledFunc].insert(GV);
    globalFuncSet.insert(handledFunc);
  }
}

bool NodeRecognizer::pathExist(set<Value*> &s, Function *Func){
  set<BasicBlock*> bbSet;
  for (set<Value*>::iterator VI = s.begin(); VI != s.end(); ++VI){
    Value *V = *VI;
    if (Instruction *Inst = dyn_cast<Instruction>(V)){
      bbSet.insert(Inst->getParent());
    }
  }
  stack<BasicBlock*> st;
  set<BasicBlock*> visited;
  for (Function::iterator FI = Func->begin(); FI != Func->end(); ++FI){
    BasicBlock *BB= FI;
    TerminatorInst *Term = BB->getTerminator();
    if (Term->getNumSuccessors() == 0){
      st.push(BB);
      visited.insert(BB);
    }
  }

  while (!st.empty()){
    BasicBlock *BB = st.top();
    st.pop();
    visited.insert(BB);
    for (pred_iterator PI = pred_begin(BB); PI != pred_end(BB); ++PI){
      BasicBlock *pred = *PI;
      int size = 0;
      for (pred_iterator PI2 = pred_begin(pred); PI2 != pred_end(pred); ++PI2){
        ++size;
      }
      //reaches entry
      if (size == 0){
        return true;
      }
      else if (bbSet.count(pred) != 0 || visited.count(pred) != 0){
        continue;
      }
      else{
        st.push(pred);
      }
    }
  }
  return false;
}

void NodeRecognizer::printResult(){
  llvm::errs() << "#######\nNodeRecognizer result for " << F->getName() << "\n";
  for (std::set<AllocNode*>::iterator AI = mNodes.begin(); AI != mNodes.end(); ++AI){
    int block_id = (*AI)->node.first;
    int index = (*AI)->node.second;
    llvm::errs() << "AllocNode: " << block_id << " " << index << " ID: " << (*AI)->ID << " " << *(*AI)->callInst << "\n";
    printToStmt(std::pair<int,int>(block_id, index));
    llvm::errs() << "Use set: \n";
    std::map<AllocNode*, std::set<std::pair<int,int> > *>::iterator s1 = uNodesMap.find(*AI);
    if (s1 != uNodesMap.end() && s1->second){
      for (std::set<std::pair<int,int> >::iterator SI = s1->second->begin(); SI != s1->second->end(); ++SI){
        int useblock_id = SI->first;
        int useindex = SI->second;
        llvm::errs() << useblock_id << " " << useindex << "\n";
        printToStmt(std::pair<int,int>(useblock_id, useindex));
      }
    }
    llvm::errs() << "Free set: \n";
    std::map<AllocNode*, std::set<std::pair<int,int> > *>::iterator s2 = fNodesMap.find(*AI);
    if (s2 != fNodesMap.end() && s2->second){
      for (std::set<std::pair<int,int> >::iterator SI2 = s2->second->begin(); SI2 != s2->second->end(); ++SI2){
        int useblock_id = SI2->first;
        int useindex = SI2->second;
        llvm::errs() << useblock_id << " " << useindex << "\n";
        printToStmt(std::pair<int,int>(useblock_id, useindex));
      }
    }
    llvm::errs() << "\n\n";
  }
}

void NodeRecognizer::printPairMap(){
  llvm::errs() << "PairMap: \n";
  for (std::map<std::pair<CFGBlock*, CFGBlock::iterator>, std::pair<int,int> >::iterator I = pairMap.begin(); I != pairMap.end(); ++I){
    llvm::errs() << "CFGBlock address: " << I->first.first << "\n";
    llvm::errs() << "CFGBlock iterator: " << &*I->first.second << "\n";
    llvm::errs() << "CFGBlock ID: " << I->second.first << "\n";
    llvm::errs() << "CFGElement ID: " << I->second.second << "\n\n";
  }
}

void NodeRecognizer::printBlockMap(){
  llvm::errs() << "BlockMap: \n";
  for (std::map<std::pair<int,int>, std::pair<CFGBlock*, CFGBlock::iterator> >::iterator I = blockMap.begin(); I != blockMap.end(); ++I){
    llvm::errs() << "CFGBlock ID: " << I->first.first << "\n";
    llvm::errs() << "CFGElement ID: " << I->first.second << "\n\n";
    llvm::errs() << "CFGBlock address: " << I->second.first << "\n";
    llvm::errs() << "CFGBlock iterator: " << &*I->second.second << "\n";
  }
}
NodeRecognizer::~NodeRecognizer(){
  
  for (std::set<AllocNode*>::iterator I2 = mNodes.begin(); I2 != mNodes.end(); ++I2){
    std::map<AllocNode*, std::set<std::pair<int, int> > *>::iterator I3 = fNodesMap.find(*I2);
    if (I3 != fNodesMap.end()){
      delete I3->second;
    }
    std::map<AllocNode*, std::set<std::pair<int, int> > *>::iterator I4 = uNodesMap.find(*I2);
    if (I4 != uNodesMap.end()){
      delete I4->second;
    }
    delete *I2;
  }

}
