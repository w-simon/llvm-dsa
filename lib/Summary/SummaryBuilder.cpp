#include "llvm/Analysis/CallGraph.h"
#include "llvm/Summary/Summary.h"
#include "llvm/Summary/SummaryPass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "dsa/DataStructure.h"
#include <queue>
#include <map>
#include <set>
#include <stack>
#include <list>
using namespace llvm;
using std::string;
using std::queue;
using std::map;
using std::set;
using std::stack;
using std::list;

bool hasRun = false; //AnalyzeWrappers only run once.
int allocID = 0;
map<Function*, Summary*> summaryMap;

int count = 0;
set<Function *> workList;//workList: put all waiting functions into the queue
bool summaryChanged = false;//mark whether the function's summary has changed
map<Function*, set<Function*> > callerMap;//map from the callee to its callers
map<Function*, set<Function*> > calleeMap;//map from the caller to its callees
set<Function*> functionSet;
int allocCount = 0;//
EquivBUDataStructures *buds;//The DSA BU pass for our use
EQTDDataStructures *tdds;//The DSA TD pass for our use
extern CallGraph *cg;
map<Function*, set<CallInst*> > callInstMap;
map<int, AllocInfo*> allocInfoMap;

void putIntoAllocSummary(DSNode *N, AllocInfo *AI, map<DSNode*, vector<AllocInfo*>*> &M);
void putIntoFreeSummary(DSNode *N, CallInst *CI, map<DSNode*, vector<CallInst*>*> &M);
bool getAllTrue(vector<AllocInfo*> *A);
void traverseCallee(DSGraph *G, Value *V, DSCallSite *CSLI, int i, Summary *calleeSummary, Summary *S);

AllocInfo::AllocInfo() : ID(allocID), escaped(false), freed(false){
  allocInfoMap[allocID++] = this;
}

AllocInfo::AllocInfo(CallInst *c, bool m, int an, std::string *g, vector<int> *l) : ID(allocID), callInst(c), must(m), argNum(an), global(g), level(l), escaped(false), freed(false){
  allocInfoMap[allocID++] = this;
}


bool isOnlyHeap(DSNode *N){
  if (N){
    return N->isHeapNode() && !N->isGlobalNode() && !N->isAllocaNode() && !N->isUnknownNode() && !N->isCollapsedNode() && !N->isIncompleteNode() && !N->isDeadNode() && !N->isExternalNode() && !N->isIntToPtrNode() && !N->isPtrToIntNode() && !N->isVAStartNode();
  }
  else
    return false;
}
//Construct callerMap (one-time work)
static void constructCallerMap(/*DSCallGraph &callgraph*/){
  callerMap.clear();
  calleeMap.clear();
  for (CallGraph::iterator I = cg->begin(); I != cg->end(); ++I){
    CallGraphNode *CGNode = I->second;
    Function *F = CGNode->getFunction();
    if (!F || F->isDeclaration()) continue;
    for (CallGraphNode::iterator I2 = CGNode->begin(); I2 != CGNode->end(); ++I2){
      CallGraphNode *CGNodeCalled = I2->second;
      Function *FCalled = CGNodeCalled->getFunction();
      if (!FCalled || FCalled->isDeclaration()) continue;
      callerMap[FCalled].insert(F);
      calleeMap[F].insert(FCalled);
    }
  }
}

void SummaryPass::analyzeWrappers(Module &M){
  if (hasRun) return;

  std::set<std::string> TempAllocators;
  TempAllocators.insert( ai->allocators.begin(), ai->allocators.end());
  std::set<std::string>::iterator it;
  for(it = TempAllocators.begin(); it != TempAllocators.end(); ++it) {
    Function* F = M.getFunction(*it);
    if (!F) continue;

    for(Value::use_iterator ui = F->use_begin(), ue = F->use_end();
        ui != ue; ++ui) {
      if (CallInst* CI = dyn_cast<CallInst>(*ui)) {
        AllocInfo *AI = new AllocInfo(CI);
        Function *CallingFunction = CI->getParent()->getParent();
        callInstMap[CallingFunction].insert(CI);
        DSGraph *G = tdds->getDSGraph(*CallingFunction);
        DSNodeHandle &handle = G->getNodeForValue(CI);
        putIntoAllocSummary(handle.getNode(), AI, summaryMap[CallingFunction]->allocMap);
      }
    }
  }

  std::set<std::string> TempDeallocators;
  TempDeallocators.insert( ai->deallocators.begin(), ai->deallocators.end());
  for(it = TempDeallocators.begin(); it != TempDeallocators.end(); ++it) {
    Function* F = M.getFunction(*it);

    if(!F)
      continue;
    for(Value::use_iterator ui = F->use_begin(), ue = F->use_end();
        ui != ue; ++ui) {
      // iterate though all calls to malloc
      if (CallInst* CI = dyn_cast<CallInst>(*ui)) {
        // The function that calls malloc could be a potential allocator
        Function *CallingFunction = CI->getParent()->getParent();
        Value *arg = CI->getArgOperand(0);
        DSGraph *G = tdds->getDSGraph(*CallingFunction);
        DSNodeHandle &handle = G->getNodeForValue(arg);
        putIntoFreeSummary(handle.getNode(), CI, summaryMap[CallingFunction]->freeMap);
      }
    }
  }

}


//The overall controlling function, calling markAlloc, markUse, and markFree through funPtr
void SummaryPass::SummaryAlgorithm(void (*funPtr)(Function*), Module *M){
  workList.clear();

  DSCallGraph &callgraph = const_cast<DSCallGraph&>(eqtd->getCallGraph());
  constructCallerMap(/*callgraph*/);
  //First, add all functions to worklist
  for (llvm::Module::iterator I = M->begin(),
      E = M->end(); I != E; ++I)
    if (!I->isDeclaration())
    {
      llvm::Function *F = I;
      workList.insert(F);
      functionSet.insert(F);
      //Meanwhile, give each function a fresh summary object
      if (summaryMap.count(F) == 0)
        summaryMap.insert(std::make_pair(F, new Summary()));
    }

  analyzeWrappers(*M);
  hasRun = true;
  //Second, calculate summary from the worklist. If changed, then add the function's callers to worklist
  for (set<Function*>::iterator I = workList.begin();
      I != workList.end(); ++I)
    if (!(*I)->isDeclaration())
    {
      llvm::Function *F = *I;
      summaryChanged = false;
      funPtr(F);
      if (summaryChanged){
        //Summary has changed. Put all callers of F to workList
        set<Function *> callerSet = callerMap[F];
        for (set<Function*>::iterator SI = callerSet.begin(); SI != callerSet.end(); ++SI){
          if (workList.count(*SI) == 0){
          }
          workList.insert(*SI);
        }
      }
    }
}

//Build alloc summary
void markAlloc(Function *F){
  DSGraph *G = tdds->getDSGraph(*F);
  Summary *S = summaryMap[F];
  assert(S && "Summary is NULL!");

  //CALCULATE the summary
  std::list<DSCallSite> &callSiteList = G->getFunctionCalls();
  for (std::list<DSCallSite>::iterator CSLI = callSiteList.begin(); CSLI != callSiteList.end(); ++CSLI){
    svset<const Function*> funcSet;
    buds->getAllCallees(*CSLI, funcSet);
    for (svset<const Function*>::iterator SI = funcSet.begin(); SI != funcSet.end(); ++SI){
      Function *callee = const_cast<Function*>(*SI);
      DSGraph *calleeGraph = tdds->getDSGraph(*callee);
      if (Summary *calleeSummary = summaryMap[callee]){
        int i = 0;
        for (Function::arg_iterator ArgI = callee->arg_begin(), E = callee->arg_end();
            ArgI != E; ++ArgI){
          if (!ArgI->getType()->isPointerTy()) return;
          DSCallSite *callSite = &*CSLI;
          traverseCallee(calleeGraph, ArgI, callSite, i, calleeSummary, S);
          ++i;
        }
      }
    }
  }

}

void putIntoAllocSummary(DSNode *N, AllocInfo *AI, map<DSNode*, vector<AllocInfo*>*> &M){
  if (isOnlyHeap(N)){
    if (!M[N]){
      M[N] = new vector<AllocInfo*>;
    }
    bool exist = false;
    for (vector<AllocInfo*>::iterator VI = M[N]->begin(); VI != M[N]->end(); ++VI){
      if (*VI == AI){
        exist = true;
        break;
      }
    }
    if (!exist){
      M[N]->push_back(AI);
      summaryChanged = true;
    }
  }
}

void putIntoFreeSummary(DSNode *N, CallInst *CI, map<DSNode*, vector<CallInst*>*> &M){
  if (isOnlyHeap(N)){
    if (!M[N]){
      M[N] = new vector<CallInst*>;
    }
    bool exist = false;
    for (vector<CallInst*>::iterator I = M[N]->begin(); I != M[N]->end(); ++I){
      if (*I == CI){
        exist = true;
        break;
      }
    }
    if (!exist){
      M[N]->push_back(CI);
      summaryChanged = true;
    }
  }
}

//return whether all of the elements in this vector are all true. Only true can we notify the function call is a *must* allocation
bool getAllTrue(vector<AllocInfo*> *A){
  for (vector<AllocInfo*>::iterator VI = A->begin(); VI != A->end(); ++VI){
    if ((*VI)->must == false){
      return false;
    }
  }
  return true;
}

//Build use summary
void markUse(Function *F){
  DSGraph *G = tdds->getDSGraph(*F);
  Summary *S = summaryMap[F];
  assert(S && "Summary is NULL!");

  //CALCULATE the summary
  for (Function::iterator FI = F->begin(); FI != F->end(); ++FI){
    for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
      if (CallInst *CI = dyn_cast<CallInst>(BI)){
        Function *callee = CI->getCalledFunction();
        if (!callee || functionSet.count(callee) == 0) continue;
        DSGraph *calleeGraph = tdds->getDSGraph(*callee);
        if (callee->getName() != "free" && callee->getName() != "malloc"){
          int i = 0;
          for (Function::arg_iterator ArgI = callee->arg_begin(), E = callee->arg_end();
              ArgI != E; ++ArgI){
            if (!ArgI->getType()->isPointerTy()) continue;;
            DSNodeHandle &formalHandle = calleeGraph->getNodeForValue(ArgI);
            if (i >= CI->getNumArgOperands()) continue;
            DSNodeHandle &actualHandle = G->getNodeForValue(CI->getArgOperand(i));
            if (!actualHandle.getNode()) continue;

            map<const DSNode*, DSNodeHandle> NodeMap;
            DSGraph::computeNodeMapping(formalHandle, actualHandle, NodeMap, false);
            for (map<const DSNode*, DSNodeHandle>::iterator NMI = NodeMap.begin(); NMI != NodeMap.end(); ++NMI){
              DSNode *Node = const_cast<DSNode*>(NMI->first);
              if (NMI->second.getNode() && (Node->isReadNode() || Node->isModifiedNode())){
                  putIntoFreeSummary(NMI->second.getNode(), CI, S->useMap);
              }
            }
          }
          ++i;
        }
      }
    }
  }
}

//Build free summary
void markFree(Function *F){
  DSGraph *G = tdds->getDSGraph(*F);
  Summary *S = summaryMap[F];
  assert(S && "Summary is NULL!");

  //CALCULATE the free summary
  std::list<DSCallSite> &callSiteList = G->getFunctionCalls();
  for (std::list<DSCallSite>::iterator CSLI = callSiteList.begin(); CSLI != callSiteList.end(); ++CSLI){
    svset<const Function*> funcSet;
    buds->getAllCallees(*CSLI, funcSet);
    //Traverse the called functions
    for (svset<const Function*>::iterator SI = funcSet.begin(); SI != funcSet.end(); ++SI){
      Function *callee = const_cast<Function*>(*SI);
      if (callee->getName() == "free"){
        Instruction *I = CSLI->getCallSite().getInstruction();
        if (CallInst *CI = dyn_cast<CallInst>(I)){
          //This is different from markAlloc, because it uses the argHandle of free(arg)
          //instead of finding the handle of a malloc() instruction
          if (CSLI->getNumPtrArgs() > 0){
            DSNodeHandle &argHandle = CSLI->getPtrArg(0);
            putIntoFreeSummary(argHandle.getNode(), CI, S->freeMap);
          }
        }
      }
      //Otherwise, query its summary, if any
      else if (callee->getName() != "malloc"){
        if (summaryMap.count(callee) != 0)
        if (Summary *calleeSummary = summaryMap[callee]){
          map<DSNode*, vector<CallInst*> *> &calleeFreeMap = calleeSummary->freeMap;
          DSGraph *calleeGraph = tdds->getDSGraph(*callee);
          int i = 0;
          for (Function::arg_iterator ArgI = callee->arg_begin(), E = callee->arg_end();
              ArgI != E; ++ArgI){
            if (!ArgI->getType()->isPointerTy()) continue;
            DSNodeHandle &formalHandle = calleeGraph->getNodeForValue(ArgI);
            if (i >= CSLI->getNumPtrArgs()) continue;
            DSNodeHandle &actualHandle = CSLI->getPtrArg(i);
            if (!actualHandle.getNode()) continue;
            map<const DSNode*, DSNodeHandle> NodeMap;
            DSGraph::computeNodeMapping(formalHandle, actualHandle, NodeMap, false);
            for (map<const DSNode*, DSNodeHandle>::iterator NMI = NodeMap.begin(); NMI != NodeMap.end(); ++NMI){
              DSNode *Node = const_cast<DSNode*>(NMI->first);
              vector<CallInst*> *freeVec = calleeFreeMap[Node];
              if (NMI->second.getNode() && freeVec && !freeVec->empty()){
                Instruction *I = CSLI->getCallSite().getInstruction();
                if (CallInst *CI = dyn_cast<CallInst>(I))
                  putIntoFreeSummary(NMI->second.getNode(), CI, S->freeMap);
              }
            }
            ++i;
          }
        }
      }
    }
  }


  for (Function::iterator FI = F->begin(); FI != F->end(); ++FI){
    for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
      Instruction *Ins = BI;
      if (CallInst *CI = dyn_cast<CallInst>(Ins)){
        if (Function *Callee = CI->getCalledFunction()){
          if (Callee->getName() == "exit"){
            for (map<DSNode*, vector<AllocInfo*> *>::iterator I = summaryMap[F]->allocMap.begin(); I != summaryMap[F]->allocMap.end(); ++I){
              putIntoFreeSummary(I->first, CI, summaryMap[F]->freeMap);
            }
          }
        }
      }
    }
  }
}


void traverseCallee(DSGraph *calleeGraph, Value *V, DSCallSite *CSLI, int i, Summary *calleeSummary, Summary *S){
  DSNodeHandle &formalHandle = calleeGraph->getNodeForValue(V);
  if (i >= CSLI->getNumPtrArgs()) return;
  DSNodeHandle &actualHandle = CSLI->getPtrArg(i);
  if (!actualHandle.getNode()) return;
  map<const DSNode*, DSNodeHandle> NodeMap;
  DSGraph::computeNodeMapping(formalHandle, actualHandle, NodeMap, false);
  for (map<const DSNode*, DSNodeHandle>::iterator NMI = NodeMap.begin(); NMI != NodeMap.end(); ++NMI){
    DSNode *Node = const_cast<DSNode*>(NMI->first);
    vector<AllocInfo*> *Vec = calleeSummary->allocMap[Node];
    if (NMI->second.getNode() && Vec && !Vec->empty()){
      bool isAllTrue = getAllTrue(calleeSummary->allocMap[Node]);
      stack<DSNode*> stk;
      vector<int> *level;
      set<DSNode*> s;
      Instruction *Ins = CSLI->getCallSite().getInstruction();
      if (CallInst *CallIns = dyn_cast<CallInst>(Ins)){
        AllocInfo *AI = new AllocInfo(CallIns, isAllTrue, i, NULL, level);
        putIntoAllocSummary(NMI->second.getNode(), AI, S->allocMap);
      }
    }
  }
}

void SummaryPass::printSummary(){
  for (Module::iterator MI = M->begin(); MI != M->end(); ++MI){
    if (!MI->isDeclaration())
      printSummary(MI);
  }
}

void SummaryPass::printSummary(llvm::Function *F){
  llvm::errs() << "^^^^^^^\nPrinting Summary Result for " << F->getName() << "...\n";
  for (Function::iterator FI = F->begin(); FI != F->end(); ++FI){
    for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
      Instruction *Ins = BI;
      llvm::errs() << "Ins: " << *Ins << "\n";
      if (PHINode *PHI = dyn_cast<PHINode>(Ins)){
        Value *V0 = PHI->getIncomingValue(0);
        Value *V1 = PHI->getIncomingValue(1);
        llvm::errs() << "Op: " << *V0 << " " << *V1 << "\n";
        if (UndefValue *UV = dyn_cast<UndefValue>(V0))
          llvm::errs() << "V0 UNDEF\n";
        if (UndefValue *UV = dyn_cast<UndefValue>(V1))
          llvm::errs() << "V1 UNDEF\n";
      }
    }
  }
  Summary *S = summaryMap[F];
  DSGraph *G = tdds->getDSGraph(*F);
  for (DSGraph::node_iterator NI = G->node_begin(); NI != G->node_end(); ++NI){
    llvm::errs() << "Node: " << &*NI << "\n";
    llvm::errs() << "  Alloc: \n";
    vector<AllocInfo*> *allocMap = S->allocMap[NI];
    if (allocMap){
      for (vector<AllocInfo*>::iterator AI = allocMap->begin(); AI != allocMap->end(); ++AI){
        llvm::errs() << "  ID: " << (*AI)->ID << " " <<  *((*AI)->callInst) << "\n";
      }
    }
    llvm::errs() << "  Use: \n";
    vector<CallInst*> *useMap = S->useMap[NI];
    if (useMap){
      for (vector<CallInst*>::iterator UI = useMap->begin(); UI != useMap->end(); ++UI){
        llvm::errs() << **UI << "\n";
      }
    }
    llvm::errs() << "  Free: \n";
    vector<CallInst*> *freeMap = S->freeMap[NI];
    if (freeMap){
      for (vector<CallInst*>::iterator UI = freeMap->begin(); UI != freeMap->end(); ++UI){
        llvm::errs() << **UI << "\n";
      }
    }
  }
  llvm::errs() << "\n";
}
