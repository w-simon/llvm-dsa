#include "clang/AST/ASTContext.h"
#include "clang/LeakFix/VariableAnalyzer.h"
#include "clang/LeakFix/NodeRecognizer.h"
#include "clang/LeakFix/CallStmtVisitor.h"
#include "llvm/Analysis/LoopInfo.h"
//#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Summary/Summary.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "dsa/DataStructure.h"
#include "dsa/DSGraph.h"
#include "dsa/DSNode.h"
#include <string>
#include <set>
#include <queue>
#include <vector>
#include <iterator>
#include <map>
#include <utility>
#include <algorithm>
#include "llvm/IR/Constants.h"

using namespace llvm;
using namespace std;
using namespace clang;

extern int allocID;
extern map<Function*, Summary*> summaryMap;

extern std::map<Instruction*, Stmt*> lto_toStmtMapL;
extern std::map<Stmt*, Instruction*> lto_invStmtMapL;
extern std::map<Value*, VarDecl*> lto_toDeclMap;
extern std::map<llvm::Value*, clang::FunctionDecl*> lto_toLLVMFuncMapL;
extern std::map<clang::FunctionDecl*, llvm::Value*> lto_toClangFuncMapL;
extern ASTContext *Contex;
extern std::map<VarDecl*, set<Value*> > lto_invDeclMapL;
extern BasicAliasAnalysis *baa;
extern std::map<int, AllocInfo*> allocInfoMap;

extern map<Function*, set<CallInst*> > callInstMap;
extern map<Value*, string> lto_toStringMap;
extern map<string, Stmt*> lto_strToCalledStmtMap;
extern int tempCounter;
extern std::map<string, string> lto_strToCalledFuncNameMap;
map<Function*, map<DSNode*, bool> > analyzedMap;
extern std::map<DSNode*, set<Value*> > nodeToValueMap;
extern std::map<DSNode*, set<DSNode*> > nodeToNodeMap;
extern EQTDDataStructures *eqtds;


int RDCount = 0;


Instruction *getTrace(Stmt* S){
  Value *V = lto_invStmtMapL[S];
  if (!V) return NULL;
  Instruction *Inst = dyn_cast<Instruction>(V);
  if (!Inst) return NULL;
  assert(Inst && "Stmt to Value trace: value not an instruction.\n In File: VariableAnalyzer.cpp");

  return Inst;
}
string getInvVarTrace(Value *Var){
  if (lto_toDeclMap.count(Var) != 0 && lto_toDeclMap[Var]){
    return lto_toDeclMap[Var]->getName();
  }
  else if (lto_toStringMap.count(Var) != 0){
    return lto_toStringMap[Var];
  }
  else{
    return string("");
  }
}

bool calledUnique(Stmt* stmt, string functionName){
  CallStmtVisitor CSV(Contex);
  CSV.Visit(stmt);
  if (functionName == CSV.topName)
    return false;

  if (count(CSV.nameVector.begin(), CSV.nameVector.end(), functionName) == 1){
    return true;
  }
  else
    return false;
}

void addTrace(CallInst *callInst, Stmt *S, std::string funcName){
  string str;
  raw_string_ostream os(str);
  os << "leakfix_temp_" << tempCounter++;
  lto_toStringMap[callInst] = os.str();
  lto_strToCalledStmtMap[os.str()] = S;
  lto_strToCalledFuncNameMap[os.str()] = funcName;
}

bool VariableAnalyzer::checkAddTempVar(set<int> *s){
  for (set<int>::iterator SI = s->begin(); SI != s->end(); ++SI){
    int id = *SI;
    if (allocMapped[id])
      continue;
    AllocInfo *AI = allocInfoMap[id];
    CallInst *callInst = AI->callInst;
    Stmt* stmt = lto_toStmtMapL[callInst];
    Function *calledFunc = callInst->getCalledFunction();
    if (calledFunc && calledUnique(stmt, calledFunc->getName())){
      addTrace(callInst, stmt, calledFunc->getName());
    }
    allocMapped[id] = true; 
  }
  return true;
}


bool VariableAnalyzer::canFix(set<int> *s, CFGBlock* inBB, CFGBlock::iterator inIter, CFGBlock* outBB, CFGBlock::iterator outIter, map<set<int>, std::string> &varMap){
  bool fixable = false;
  Summary *S = summaryMap[F];

  checkAddTempVar(s);

  for (DSGraph::node_iterator NI = G->node_begin(); NI != G->node_end(); ++NI){
    vector<AllocInfo*> *allocSet = S->allocMap[NI];
    if (!allocSet) continue;
    set<int> subset;
    bool allIn = true;
    //Ensure each member is in the "s"
    for (vector<AllocInfo*>::iterator SI = allocSet->begin(); SI != allocSet->end(); ++SI){
      int id = (*SI)->ID;
      if (s->count(id) == 0){
        allIn = false;
        break;
      }
      subset.insert(id);
    }
    //Okey. This node is legal for free
    if (allIn){
      Stmt *inStmt = NULL;
      Stmt *outStmt = NULL;
      Instruction *inIns = NULL;
      Instruction *outIns = NULL;
      if (inIter->getAs<CFGStmt>()){
        inStmt = const_cast<Stmt*>(inIter->castAs<CFGStmt>().getStmt());
        //Use trace to get the IR Instruction
        inIns = getTrace(inStmt);
        if (inStmt && inIns){
          int clangSize = outBB->succ_size();
          if (clangSize == 0){
            bool sizeEqual = false;
            BasicBlock *IRBB = inIns->getParent();
            if (succ_begin(IRBB) == succ_end(IRBB))
              sizeEqual = true;
            for (succ_iterator SI = succ_begin(inIns->getParent()); SI != succ_end(inIns->getParent()); ++SI){
              if (succ_begin(*SI) == succ_end(*SI)){
                sizeEqual = true;
              }
            }
            if (!sizeEqual) return false;
          }
        }
      }
      if (outIter->getAs<CFGStmt>()){
        outStmt = const_cast<Stmt*>(outIter->castAs<CFGStmt>().getStmt());
        //Use trace to get the IR Instruction
        outIns = getTrace(outStmt);
      }
      vector<Value*> *result = NULL;
      if (inIns && outIns){
        BasicBlock *BB = inIns->getParent();
        BasicBlock::iterator insIter = inIns;
        ProgramNode pn = make_pair(BB, insIter); 
        ProgramPoint pp = pair<ProgramNode, PPType>(pn, out);
        set<Value*> *result1 = RDResultMap[pp];
        BasicBlock *outsBB = outIns->getParent();
        BasicBlock::iterator outsIter = outIns;
        ProgramNode pnOUT = make_pair(outsBB, outsIter); 
        ProgramPoint ppOUT = pair<ProgramNode, PPType>(pnOUT, in);
        set<Value*> *result2 = RDResultMap[ppOUT];
        result = new vector<Value*>(result1->size() + result2->size());
        set_intersection(result1->begin(), result1->end(), result2->begin(), result2->end(), result->begin());
      }
      else if (inIns){
        //Get the ProgramPoint
        BasicBlock *BB = inIns->getParent();
        BasicBlock::iterator insIter = inIns;
        ProgramNode pn = make_pair(BB, insIter); 
        ProgramPoint pp = pair<ProgramNode, PPType>(pn, out);
        set<Value*> *result1 = RDResultMap[pp];
        result = new vector<Value*>(result1->size());
        copy(result1->begin(), result1->end(), result->begin());
      }
      else if (outIns){
        BasicBlock *outsBB = outIns->getParent();
        BasicBlock::iterator outsIter = outIns;
        ProgramNode pnOUT = make_pair(outsBB, outsIter); 
        ProgramPoint ppOUT = pair<ProgramNode, PPType>(pnOUT, in);
        set<Value*> *result2 = RDResultMap[ppOUT];
        result = new vector<Value*>(result2->size());
        copy(result2->begin(), result2->end(), result->begin());
      }
      else continue;
      //Traverse the usable values, checking whether it's reachable to the DSNode
      std::string str;
      set<DSNode*> visited;
      for (vector<Value*>::iterator I = result->begin(); I != result->end(); ++I){
        if (*I){
          Value* V = *I;
          string VD = getInvVarTrace(V);
          if (VD != ""){
            DSNodeHandle &handle = G->getNodeForValue(V);
            DSNode *HN = handle.getNode();
            if (!HN) continue;
            else if (HN == NI){
              varMap.insert(make_pair(subset, VD));
              fixable = true;
              break;
            }
            else{
              traverse(HN, visited, NI, varMap, subset, VD, fixable);
            }
          }
        }
      }
      delete result;
    }
  }
  return fixable;
}

void VariableAnalyzer::traverse(DSNode *HN, set<DSNode*> &visited, DSNode *N, map<set<int>, std::string> &varMap, set<int> &subset, string VD, bool &fixable){
  if (!HN) return;
  if (visited.count(HN) != 0) return;
  visited.insert(HN);
  for (DSNode::edge_iterator EI = HN->edge_begin(); EI != HN->edge_end(); ++EI){
    DSNodeHandle &handle2 = EI->second;
    DSNode *HN2 = handle2.getNode();
    if (HN2 == N){
      varMap.insert(make_pair(subset, VD));
      fixable = true;
      return;
    }
    else{
      traverse(HN2, visited, N, varMap, subset, VD, fixable);
      if (fixable) return;
    }
  }
}

bool VariableAnalyzer::reachable(DSNode* CurNode, DSNode* N, vector<int> &level){
  if (CurNode == NULL || N == NULL) return false;
  if (CurNode == N) return true;
  for (unsigned i = 0; i < CurNode->getSize(); ++i){
    if (CurNode->hasLink(i)){
      level.push_back(i);
      if (reachable(CurNode->getLink(i).getNode(), N, level)){
        return true;
      }
      level.pop_back();
    }
  }
  return false;
}

std::string VariableAnalyzer::parseLevel(Value* I, vector<int> &level){
  VarDecl *Var = lto_toDeclMap[I];
  if (!Var) return string("");
  std::string str = Var->getName();
  clang::Type *clangTy = const_cast<clang::Type*>(Var->getType().getTypePtr());
  for (vector<int>::iterator VI = level.begin(); VI != level.end(); ++VI){
    if (clangTy->isRecordType()){
      if (RecordDecl *RDecl = clangTy->castAs<RecordType>()->getDecl()){
        RecordDecl::field_iterator FI = RDecl->field_begin();
        for (int i = 0; i < *VI; ++I){
          ++FI;
        }
        std::string fieldStr(FI->getName().str());
        str = "(*(" + str  + "))." + fieldStr;
      }
    }
    else if (clangTy->isPointerType()){
      str = "(*(" + str + "))";
    }
  }
  return str;
}

void VariableAnalyzer::analyzeInit(){
  set<AllocNode*> s = NR->mNodes;
  for (set<AllocNode*>::iterator SI = s.begin(); SI != s.end(); ++SI){
    CallInst *CI = (*SI)->callInst;
    DSNodeHandle &H = G->getNodeForValue(CI);
    DSNode *N = H.getNode();
    AllocNode *AN = *SI;
    initAnalyzed.clear();
    initConfirmed.clear();
    initInst.clear();
    analyzeNode(N, AN, F);
    
  }
  /*for (Function::iterator FI0 = F->begin(); FI0 != F->end(); ++FI0){
    for (BasicBlock::iterator BI0 = FI0->begin(); BI0 != FI0->end(); ++BI0){
      if (isa<LoadInst>(BI0) || isa<CallInst>(BI0) || isa<GetElementPtrInst>(BI0)){
        BasicAliasAnalysis::Location L1 = BasicAliasAnalysis::Location(BI0, BasicAliasAnalysis::UnknownSize);
        llvm::errs() << *BI0 << "\n";
        for (Function::iterator FI = F->begin(); FI != F->end(); ++FI){
          for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
            BasicAliasAnalysis::Location L2 = BasicAliasAnalysis::Location(BI, BasicAliasAnalysis::UnknownSize);
            if (!notDifferentParent(L1.Ptr, L2.Ptr)) continue;
            if (baa->alias(L1, L2) == AliasAnalysis::MustAlias){
              llvm::errs() << "  " << *BI << "\n";
            }

          }
        }
      }
    }
  }*/
}

void VariableAnalyzer::checkAddressTaken(DSNode *N, AllocNode *AN, Function *Func){
  handleTopLevel(N, AN, Func);
  if (undefSet.count(AN) == 0){
    set<set<Value*> *> equalSet = buildEqualSet(N, AN, Func);
    set<set<Value*> *> containSet;
    map<set<Value*> *, set<pair<Function*,DSNode*> > > argNodeMap;
    map<set<Value*> *, set<Instruction*> > argInstMap;
    set<Value*> reachableValueSet;
    //filter
    for (set<set<Value*> *>::iterator EI = equalSet.begin(); EI != equalSet.end(); ++EI){
      set<Value*> *subset = *EI;
      set<Value*> useSet;
      for (set<Value*>::iterator setI = subset->begin(); setI != subset->end(); ++setI){
        Value *V = *setI;
        for (Value::use_iterator UI = V->use_begin(); UI != V->use_end(); ++UI){
          if (StoreInst *store = dyn_cast<StoreInst>(*UI)){
            if (store->getPointerOperand() == V)
              useSet.insert(store);
          }
        }
      }
      bool contains = false;
      for (set<Value*>::iterator SI = subset->begin(); SI != subset->end(); ++SI){
        if (CallInst *CI = dyn_cast<CallInst>(*SI)){
          contains = true;
        }
      }
      for (set<Value*>::iterator SI = subset->begin(); SI != subset->end(); ++SI){
        DSGraph *graph = eqtds->getDSGraph(*Func);
        DSNode *curNode = graph->getNodeForValue(*SI).getNode();
        if (curNode){
          for (set<DSNode*>::iterator NI = nodeToNodeMap[curNode].begin(); NI != nodeToNodeMap[curNode].end(); ++NI){
            DSNode *transNode = *NI;
            for (set<Value*>::iterator VI = nodeToValueMap[transNode].begin(); VI != nodeToValueMap[transNode].end(); ++VI){
              Value *V = *VI;
              for (Value::use_iterator UI = V->use_begin(); UI != V->use_end(); ++UI){
                if (CallInst *CI = dyn_cast<CallInst>(*UI)){
                  Function *callee = CI->getCalledFunction();
                  if (!callee || callee->isDeclaration()){
                    undefSet.insert(AN);
                    return;
                  }
                  for (unsigned i = 0; i < CI->getNumArgOperands(); ++i){
                    Value *Arg = CI->getArgOperand(i);
                    if (V == Arg){
                      DSGraph *calleeGraph = eqtds->getDSGraph(*callee);
                      Argument *Param = callee->getArgumentList().begin();
                      DSNodeHandle &formalHandle = calleeGraph->getNodeForValue(Param);
                      DSNodeHandle &actualHandle = G->getNodeForValue(V);
                      if (!formalHandle.getNode() || !actualHandle.getNode()){
                        continue;
                      }
                      map<const DSNode*, DSNodeHandle> NodeMap;
                      DSGraph::computeNodeMapping(actualHandle, formalHandle, NodeMap, false);
                      DSNodeHandle &DH = NodeMap[N];
                      if (DH.getNode()){
                        argNodeMap[subset].insert(make_pair(callee,DH.getNode()));
                        argInstMap[subset].insert(CI);
                        contains = true;
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      //handle non-funcs
      if (contains){
        containSet.insert(subset);
      }
      else if (!contains && NodeRecognizer::pathExist(useSet, Func)){
        undefSet.insert(AN);
        return;
      }
    }
    //for funcs
    for (set<set<Value*> *>::iterator EI = containSet.begin(); EI != containSet.end(); ++EI){
      set<Value*> *subset = *EI;
      set<Value*> useSet;
      for (set<Value*>::iterator setI = subset->begin(); setI != subset->end(); ++setI){
        Value *V = *setI;
        for (Value::use_iterator UI = V->use_begin(); UI != V->use_end(); ++UI){
          if (StoreInst *store = dyn_cast<StoreInst>(*UI)){
            if (store->getPointerOperand() == V)
              useSet.insert(store);
          }
        }
      }
      for (set<Value*>::iterator SI = subset->begin(); SI != subset->end(); ++SI){
        if (CallInst *CI = dyn_cast<CallInst>(*SI)){
          analyzeRet(CI, AN, Func);
          if (undefSet.count(AN) == 0) return;
          useSet.insert(CI);
        }
      }
      set<pair<Function*, DSNode*> > s = argNodeMap[*EI];
      set<Instruction*> CISet = argInstMap[*EI];
      for (set<Instruction*>::iterator CII = CISet.begin(); CII != CISet.end(); ++CII){
        useSet.insert(*CII);
      }
      for (set<pair<Function*, DSNode*> >::iterator I = s.begin(); I != s.end(); ++I){
        Function *callee = I->first;
        DSNode *DN = I->second;
        analyzeNode(DN, AN, callee);
        if (undefSet.count(AN) == 0) return;
      }
      if (NodeRecognizer::pathExist(useSet, Func)){
        undefSet.insert(AN);
        return;
      }
      
    }
  }
}

void VariableAnalyzer::analyzeNode(DSNode *N, AllocNode *AN, Function *Func){
  if (!N) return;
  if (nodeToNodeMap.find(N) == nodeToNodeMap.end()){
    handleTopLevel(N, AN, Func);
  }
  else{
    checkAddressTaken(N, AN, Func);
  }
}

void VariableAnalyzer::handleTopLevel(DSNode *N, AllocNode *AN, Function *Func){
  map<DSNode*, set<Value*> >::iterator MI = nodeToValueMap.find(N);
  set<Value*> &s = MI->second;
  for (set<Value*>::iterator SI = s.begin(); SI != s.end(); ++SI){
    if (PHINode *PHI = dyn_cast<PHINode>(*SI)){
      Value *V0 = PHI->getIncomingValue(0);
      Value *V1 = PHI->getIncomingValue(1);
      if (isa<UndefValue>(V0) || isa<UndefValue>(V1)){
        undefSet.insert(AN);
      }
    }
    else if (CallInst *CI = dyn_cast<CallInst>(*SI)){
      analyzeRet(CI, AN, Func);
      
    }
  }
}

void VariableAnalyzer::analyzeArg(CallInst *CI, AllocNode *AN, Function *Func){
  Function *callee = CI->getCalledFunction();
  if (!callee) return;
  if (initAnalyzed.count(callee) != 0){
    undefSet.insert(AN);
    return;
  }
  else{
    initAnalyzed.insert(callee);
  }
  if (!callee || (callee->isDeclaration() && callee->getName() != "malloc")) undefSet.insert(AN);
  else if (!callee->isDeclaration()){
    DSGraph *calleeGraph = eqtds->getDSGraph(*callee);
    DSNodeHandle &ret = calleeGraph->getReturnNodes()[callee];
    DSNode *RN = ret.getNode();
    analyzeNode(RN, AN, callee);
  }
}

void VariableAnalyzer::analyzeRet(CallInst *CI, AllocNode *AN, Function *Func){
  Function *callee = CI->getCalledFunction();
  if (!callee) return;
  if (initAnalyzed.count(callee) != 0){
    undefSet.insert(AN);
    return;
  }
  else{
    initAnalyzed.insert(callee);
  }
  if (!callee || (callee->isDeclaration() && callee->getName() != "malloc")) undefSet.insert(AN);
  else if (!callee->isDeclaration()){
    DSGraph *calleeGraph = eqtds->getDSGraph(*callee);
    DSNodeHandle &ret = calleeGraph->getReturnNodes()[callee];
    DSNode *RN = ret.getNode();
    analyzeNode(RN, AN, callee);
  }
}

set<set<Value*>*> VariableAnalyzer::buildEqualSet(DSNode* N, AllocNode* AN, Function* Func){
  set<DSNode*> nodeSet;
  map<DSNode*, set<set<Value*>*> > resultMap;
  map<Value*, set<Value*>* > valueBelongMap;
  map<DSNode*, DSNode*> succMap;
  set<DSNode*> visited;

  nodeSet.insert(N);
  visited.insert(N); 
  DSNode *succ = N;
  //construct succMap
  stack<DSNode *> s;
  s.push(N);
  nodeSet.insert(N);
  while (!s.empty()){
    DSNode *succ = s.top();
    s.pop();
    if (nodeToNodeMap.find(succ) != nodeToNodeMap.end()){
      set<DSNode*> &predSet = nodeToNodeMap[succ];
      for (set<DSNode*>::iterator SI = predSet.begin(); SI != predSet.end(); ++SI){
        DSNode *pred = *SI;
        if (visited.count(pred) == 0){
          succMap[pred] = succ;
          s.push(pred);
          visited.insert(pred);
          nodeSet.insert(pred);
        }
      }
    }
  }
  //initialize 
  for (set<DSNode*>::iterator SI = nodeSet.begin(); SI != nodeSet.end(); ++SI){
    DSNode *curNode = *SI;
    set<set<Value*>*> result;
    if (nodeToValueMap.find(curNode) != nodeToValueMap.end()){
      set<Value*> &valueSet = nodeToValueMap[curNode];
      set<Value*> valueVisited;
      for (set<Value*>::iterator VI = valueSet.begin(); VI != valueSet.end(); ++VI){
        if (valueVisited.count(*VI) == 0){
          set<Value*> *s = new set<Value*>();
          s->insert(*VI);
          valueBelongMap.insert(make_pair(*VI, s));
          valueVisited.insert(*VI);
          for (set<Value*>::iterator VI2 = valueSet.begin(); VI2 != valueSet.end(); ++VI2){
            if (valueVisited.count(*VI2) != 0) continue;
            if (*VI2 == *VI) continue;
            //BasicAliasAnalysis::Location L1 = BasicAliasAnalysis::Location(*VI, BasicAliasAnalysis::UnknownSize);
            AliasAnalysis::Location L1 = AliasAnalysis::Location(*VI, AliasAnalysis::UnknownSize);
            //BasicAliasAnalysis::Location L2 = BasicAliasAnalysis::Location(*VI2, BasicAliasAnalysis::UnknownSize);
            AliasAnalysis::Location L2 = AliasAnalysis::Location(*VI2, AliasAnalysis::UnknownSize);
            if (!notDifferentParent(L1.Ptr, L2.Ptr)) continue;
            if (baa->alias(L1, L2) == AliasAnalysis::MustAlias){
              s->insert(*VI2);
              valueVisited.insert(*VI2);
            }
          }
          result.insert(s);
        }
      }
    }
  }
  //analyze
  set<DSNode*> toVisit;
  toVisit.insert(nodeSet.begin(), nodeSet.end());

  while (!toVisit.empty()){
    DSNode *curNode = *toVisit.begin();
    toVisit.erase(curNode);
    set<set<Value*>*> curResult = resultMap[curNode];
    //analyze curResult
    for (set<set<Value*>*>::iterator SI = curResult.begin(); SI != curResult.end(); ++SI){
      set<Value*> *s = *SI;
      for (set<Value*>::iterator SI2 = s->begin(); SI2 != s->end(); ++SI2){
        Value *V = *SI2;
        if (GetElementPtrInst *GEPInst = dyn_cast<GetElementPtrInst>(V)){
          for (set<set<Value*>*>::iterator I = curResult.begin(); I != curResult.end(); ++I){
            set<Value*> *s2 = *I;
            if (s == s2) continue;
            for (set<Value*>::iterator I2 = s2->begin(); I2 != s2->end(); ++I2){
              Value *V2 = *I2;
              if (GetElementPtrInst *GEPInst2 = dyn_cast<GetElementPtrInst>(V2)){
                if (shareSame(GEPInst, GEPInst2, valueBelongMap)){
                  s->insert(s2->begin(), s2->end());
                  for (set<Value*>::iterator I3 = s2->begin(); I3 != s2->end(); ++I3){
                    valueBelongMap[*I3] = s;
                  }
                  toVisit.insert(curNode);
                  break;
                }
              }
            }
          }
        }
        if (LoadInst *loadInst = dyn_cast<LoadInst>(V)){
          for (set<set<Value*>*>::iterator I = curResult.begin(); I != curResult.end(); ++I){
            set<Value*> *s2 = *I;
            if (s == s2) continue;
            for (set<Value*>::iterator I2 = s2->begin(); I2 != s2->end(); ++I2){
              Value *V2 = *I2;
              if (LoadInst *loadInst2 = dyn_cast<LoadInst>(V2)){
                set<Value*> *belong1 = valueBelongMap[loadInst];
                set<Value*> *belong2 = valueBelongMap[loadInst2];
                if (belong1!=belong2){
                  s->insert(s2->begin(), s2->end());
                  for (set<Value*>::iterator I3 = s2->begin(); I3 != s2->end(); ++I3){
                    valueBelongMap[*I3] = s;
                  }
                  toVisit.insert(curNode);
                  break;
                }
              }
            }
          }
        }
      }
    }
    
  }
  return resultMap[N];
}

bool VariableAnalyzer::shareSame(GetElementPtrInst *GEPInst, GetElementPtrInst *GEPInst2, map<Value*, set<Value*>*> &valueBelongMap){
  Value *V1 = GEPInst->getPointerOperand();
  Value *V2 = GEPInst2->getPointerOperand();
  set<Value*> *s1 = valueBelongMap[V1];
  set<Value*> *s2 = valueBelongMap[V2];
  if (s1!=s2) {return false;}
  for (GetElementPtrInst::op_iterator op1 = GEPInst->idx_begin(); op1 != GEPInst->idx_end(); ++op1){
    for (GetElementPtrInst::op_iterator op2 = GEPInst->idx_begin(); op2 != GEPInst->idx_end(); ++op2){
      if (op1 != op2)
        return false;
    }
  }
  return true;
}

void VariableAnalyzer::analyzeRD(){
  //Remember whether the value has been put into an instruction set
  map<Value*, bool> inSet;
  DSScalarMap &scalarMap = G->getScalarMap();
  //Traverse all values in DSGraph, and construct a nested set.
  //Each set element contains instructions with same srcVar
  for (DSScalarMap::iterator DI = scalarMap.begin(); DI != scalarMap.end(); ++DI){
    Value *V = const_cast<Value*>(DI->first);
    if (inSet.count(V)==0 || !inSet[V]){
      inSet[V] = true;
    }
  }

  //TODO need fix it
#if 0
  //Begin RD Analysis
  DominatorTree *dt = new DominatorTree();
  dt->runOnFunction(*F);
  li = new LoopInfo();
  li->LI.Analyze(dt->getBase());
  queue<ProgramPoint> *workList = new queue<ProgramPoint>;
  initializeRD(workList);
  flowRD(workList);
  delete workList;
  clearExit();

  loopClear();
  delete li;
  delete dt;
#endif
}

void VariableAnalyzer::initializeRD(queue<ProgramPoint> *workList){
  for (llvm::Function::iterator FI = F->begin(); FI != F->end(); ++FI){
    for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
      BasicBlock *BB = FI;
      ProgramNode pn = make_pair(BB, BI);
      ProgramPoint pp = make_pair(pn, in);
      set<Value*> *s = new set<Value*>;
      RDResultMap[pp] = s; 
      workList->push(pp);

      pp = make_pair(pn, out);
      s = new set<Value*>;
      RDResultMap[pp] = s; 
      workList->push(pp);
    }
  }
}

void VariableAnalyzer::pushSuccsToWorkList(queue<ProgramPoint> *workList, BasicBlock *BB, BasicBlock::iterator BI){
}

void VariableAnalyzer::flowRD(queue<ProgramPoint> *workList){
  while (!workList->empty()){
    ProgramPoint pp = workList->front();
    workList->pop();
    ProgramNode pn = pp.first;
    PPType pptype = pp.second;
    BasicBlock *BB = pn.first;
    BasicBlock::iterator BI = pn.second;
    Instruction *Ins = BI;
    //From in to out: must be a node
    if (pptype == in){
      ProgramPoint ppout = make_pair(pn, out);
      set<Value*> *tempResult = new set<Value*>(*RDResultMap[pp]);
      set<Value*> *origResult = RDResultMap[ppout];
      bool changed = false;
      Value *curValue = BI;
      VarDecl* SrcVar = lto_toDeclMap[curValue];
      set<Value*> &target = lto_invDeclMapL[SrcVar];
      //KILL
      for (set<Value*>::iterator KSI = tempResult->begin(); KSI != tempResult->end(); ){
        if (target.count(*KSI) != 0){
          tempResult->erase(KSI++);
          changed = true;
        }
        else{
          ++KSI;
        }
      }
      //GEN
      if (isa<CallInst>(curValue) || (isa<BitCastInst>(curValue) && lto_toDeclMap.count(curValue) != 0 && lto_toDeclMap[curValue]))
        tempResult->insert(curValue);
      if (!std::equal(tempResult->begin(), tempResult->end(),
            origResult->begin())){
        RDResultMap[ppout] = tempResult;
        workList->push(ppout);
        delete origResult;
      }
      else{
        delete tempResult;
      }
    }
    //From out to in: must be an edge with possible convergence
    else if (pptype == out){
      set<Value*> *tempResult;
      BasicBlock::iterator e = BB->end();
      --e;
      //This is the end of the block, next node is in another block
      if (BI == e){
        for (succ_iterator succ = succ_begin(BB); succ != succ_end(BB); ++succ){
          tempResult = new set<Value*>(*RDResultMap[pp]);
          BasicBlock *succBB = *succ;
          ProgramNode succPN = make_pair(succBB, succBB->begin());
          ProgramPoint succPP = make_pair(succPN, in);
          set<Value*> *origResult = RDResultMap[succPP];

          //DELETE NULL
          if (BranchInst *BRInst = dyn_cast<BranchInst>(Ins)){
            if (BRInst->isConditional()){
              Value *CV = BRInst->getCondition();
              if (ICmpInst *II = dyn_cast<ICmpInst>(CV)){
                if ((succBB->getName().str().find("end") == -1) &&
                    ((BRInst->getSuccessor(0) == succBB && II->getPredicate() == CmpInst::ICMP_EQ)
                     || (BRInst->getSuccessor(1) == succBB && II->getPredicate() == CmpInst::ICMP_NE))){
                  Value *LHS = II->getOperand(0);
                  Value *RHS = II->getOperand(1);
                  if (Constant *C = dyn_cast<Constant>(LHS)){
                    if (C->isNullValue()){
                      tempResult->erase(RHS);
                    }     
                  }     
                  if (Constant *C = dyn_cast<Constant>(RHS)){
                    if (C->isNullValue()){
                      tempResult->erase(LHS);
                    }     
                  }     
                }     
              }     
            }     
          }     


          //Look for other preds, for convergence
          for (pred_iterator pred = pred_begin(*succ); pred != pred_end(*succ); ++pred){
            if (*pred == BB) continue;
            BasicBlock *predBB = *pred;
            BasicBlock::iterator it = predBB->end();
            --it;
            ProgramNode predPN = make_pair(predBB, it);
            ProgramPoint predPP = make_pair(predPN, out);

            set<Value*> *predResult = RDResultMap[predPP];
            for (set<Value*>::iterator compI = predResult->begin(); compI != predResult->end();++compI){
              tempResult->insert(*compI);
            }
          }

          
          //After convergence, check equivalence of temp result and original result 
          if (!std::equal(tempResult->begin(), tempResult->end(),
                origResult->begin())){
            //Not equal, update and add to worklist
            ProgramNode pn1 = make_pair(*succ, (*succ)->begin());
            ProgramPoint pp1 = make_pair(pn1, in);
            RDResultMap[pp1] = tempResult;
            workList->push(pp1);
            delete origResult;
          }
          else{
            //Equal. tempResult is redundant. Delete it
            delete tempResult;
          }
        }
        //Is back edge
        if (li->isLoopHeader(BB)){
          Loop *l = li->getLoopFor(BB);
          for (succ_iterator succ = succ_begin(BB); succ != succ_end(BB); ++succ){
            BasicBlock *succBB = *succ;
            if (l->contains(succBB)){
              ProgramNode succPN = make_pair(succBB, succBB->begin());
              ProgramPoint succPP = make_pair(succPN, in);
              set<Value*> *origResult = RDResultMap[succPP];
              for (set<Value*>::iterator RI = origResult->begin(); RI != origResult->end(); ){
                if (Instruction *In = dyn_cast<Instruction>(*RI)){
                  if (l->contains(In)){
                    origResult->erase(RI++);
                  }
                  else ++RI;
                }
                else{
                  ++RI;
                }
              }
            }
          }
        }
      }
      //This node and the next node is in the same block, just copy paste
      else{
        tempResult = new set<Value*>(*RDResultMap[pp]);
        BasicBlock::iterator tmp = BI;
        ProgramNode succPN = make_pair(BB, ++BI);
        ProgramPoint succPP = make_pair(succPN, in);
        set<Value*> *origResult = RDResultMap[succPP];
        if (!std::equal(tempResult->begin(), tempResult->end(),
              origResult->begin())){
          //Not equal, update and add to worklist
          RDResultMap[succPP] = tempResult;
          workList->push(succPP);
          delete origResult;
        }
        else{
          delete tempResult;
        }
      }

    }
  }
}

void VariableAnalyzer::clearExit(){
  for (llvm::Function::iterator FI = F->begin(); FI != F->end(); ++FI){
    bool isExit = false;
    for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
      if (CallInst *CI = dyn_cast<CallInst>(BI)){
        Function *F = CI->getCalledFunction();
        if (F && F->getName().str() == "exit"){
          isExit =true;
          break;
        }
      }
    }
    if (isExit){
      BasicBlock *BB= FI;
      for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
        ProgramNode pn = make_pair(BB, BI);
        ProgramPoint pp = make_pair(pn, in);
        RDResultMap[pp]->clear();
        pp = make_pair(pn, out);
        RDResultMap[pp]->clear();
      }
    }
  }
}

void VariableAnalyzer::loopClear(){
  set<CallInst*> &callInstSet = callInstMap[F];
  for (set<CallInst*>::iterator I = callInstSet.begin(); I != callInstSet.end(); ++I){
    Instruction *Ins = *I;
    //First determine the baseline pointer
    Instruction *baseInst = NULL;
    for (map<Value*, VarDecl*>::iterator SI = lto_toDeclMap.begin(); SI != lto_toDeclMap.end(); ++SI){
      if (Instruction *VDecl = dyn_cast<Instruction>(SI->first)){
        BasicAliasAnalysis::Location L1 = BasicAliasAnalysis::Location(VDecl, BasicAliasAnalysis::UnknownSize);
        //AliasAnalysis::Location L1 = AliasAnalysis::Location(VDecl, AliasAnalysis::UnknownSize);
        BasicAliasAnalysis::Location L2 = BasicAliasAnalysis::Location(Ins, BasicAliasAnalysis::UnknownSize);
        //AliasAnalysis::Location L2 = AliasAnalysis::Location(Ins, AliasAnalysis::UnknownSize);
        if (!notDifferentParent(L1.Ptr, L2.Ptr)) continue;
        if (baa->alias(L1, L2) == AliasAnalysis::MustAlias){
          baseInst = VDecl;
          break;
        }
      }
    }
    if (!baseInst) continue;
    BasicBlock *BB = baseInst->getParent();
    Loop *L = li->getLoopFor(BB);
    if (li->getLoopDepth(BB) == 0) continue;
    queue<pair<BasicBlock*, Instruction*> > workList;
    workList.push(make_pair(BB, Ins));
    set<BasicBlock*> visited;
    while (!workList.empty()){
      pair<BasicBlock*, Instruction*> p3 = workList.front();
      BB = p3.first;
      Ins = p3.second;
      workList.pop();
      innerBlockUpdate(p3, baseInst, workList, visited, li);
      if (li->isLoopHeader(BB)){
        continue;
      }
      else{
        for (pred_iterator pred = pred_begin(BB); pred != pred_end(BB); ++pred){
          BasicBlock *predBB = *pred;
          if (visited.count(predBB) != 0) continue;
          if (li->getLoopDepth(predBB) == li->getLoopDepth(BB)){
            workList.push(make_pair(predBB, &predBB->back()));
          }
        }
      }
    }
  }
}

void VariableAnalyzer::innerBlockUpdate(pair<BasicBlock*, Instruction*> p, Instruction *Src, queue<pair<BasicBlock*, Instruction*> > &workList, set<BasicBlock*> &visited, LoopInfo *li){
  VarDecl *VD = lto_toDeclMap[Src];
  if (!VD) return;
  set<Value*> *valueSet = &lto_invDeclMapL[VD];

  BasicBlock *BB = p.first;
  Instruction *Ins = p.second; 
  if (visited.count(BB) != 0) return;
  visited.insert(BB);
  for (BasicBlock::iterator I = BB->begin(); &*I != Ins; ++I){
    ProgramNode pn = make_pair(BB, I);
    ProgramPoint pp = make_pair(pn, in);
    set<Value*> *s = RDResultMap[pp];
    for (set<Value*>::iterator SI = s->begin(); SI != s->end();){
      if (valueSet->count(*SI) != 0){
        s->erase(SI++);
      }
      else{
        ++SI;
      }
    }
    pp = make_pair(pn, out);
    s = RDResultMap[pp];
    for (set<Value*>::iterator SI = s->begin(); SI != s->end();){
      if (valueSet->count(*SI) != 0){
        s->erase(SI++);
      }
      else{
        ++SI;
      }
    }
  }
}

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
void VariableAnalyzer::printRDResult(){
  map<string, set<Stmt*> > inmap;
  map<string, set<Stmt*> > outmap;
  //llvm::errs() << "@@@@@@@\nPrinting RD Result for " << F->getName() << "...\n";
  /*for (llvm::Function::iterator FI = F->begin(); FI != F->end(); ++FI){
    for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
      llvm::errs() << "Value: " << *BI << "\n";
      Stmt *S = lto_toStmtMapL[BI];
      if (S){
        printLocation(S);
        S->dumpPretty(*Contex);
        llvm::errs() << "\n";
      }
      ProgramNode pn = make_pair(FI, BI);
      ProgramPoint pp = make_pair(pn, in);
      llvm::errs() << "ProgramPoint: in: " << *pn.second << " Address: " << pn.second << "\n";
      printValueSet(RDResultMap[pp]);
      for (set<Value*>::iterator I = RDResultMap[pp]->begin(); I != RDResultMap[pp]->end(); ++I){
        if (lto_toDeclMap.count(*I) != 0 && lto_toDeclMap[*I]){
          string name = lto_toDeclMap[*I]->getName();
          inmap[name].insert(S);
        }
      }
      pp = make_pair(pn, out);
      llvm::errs() << "ProgramPoint: out: " << *pn.second << " Address: " << pn.second << "\n";
      printValueSet(RDResultMap[pp]);
      for (set<Value*>::iterator I = RDResultMap[pp]->begin(); I != RDResultMap[pp]->end(); ++I){
        if (lto_toDeclMap.count(*I) != 0 && lto_toDeclMap[*I]){
          string name = lto_toDeclMap[*I]->getName();
          outmap[name].insert(S);
        }
      } 
    }
  }
  llvm::errs() << "\n";*/
  llvm::errs() << "$$$$$$\nPrinting name result for " << F->getName() << "\n";
  for (map<string, set<Stmt*> >::iterator MI = outmap.begin(); MI != outmap.end(); ++MI){
    llvm::errs() << MI->first << "\n";
    set<Stmt*> &s = MI->second;
    for (set<Stmt*>::iterator SI = s.begin(); SI != s.end(); ++SI){
      Stmt *S = *SI;
      if (S){
        printLocation(S);
        S->dumpPretty(*Contex);
        llvm::errs() << "\n";
      }
    }
  }
}
void VariableAnalyzer::printValueSet(set<Value*> *s){
  if (!s) return;
  for (set<Value*>::iterator I = s->begin(); I != s->end(); ++I){
    llvm::errs() << "   IR: " << **I << "\n";

    if (lto_toDeclMap.count(*I) != 0 && lto_toDeclMap[*I]){
      llvm::errs() << "   VarDecl: " << lto_toDeclMap[*I]->getName() << "\n";
    }
  }   
}

VariableAnalyzer::VariableAnalyzer(NodeRecognizer *nr) : F(nr->F), G(nr->G), NR(nr){
}

VariableAnalyzer::VariableAnalyzer() {}
VariableAnalyzer::~VariableAnalyzer(){
  for (llvm::Function::iterator FI = F->begin(); FI != F->end(); ++FI){
    for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
      ProgramNode pn = make_pair(FI, BI);
      ProgramPoint pp = make_pair(pn, in);
      delete RDResultMap[pp];
      pp = make_pair(pn, out);
      delete RDResultMap[pp];
    }
  }
}
