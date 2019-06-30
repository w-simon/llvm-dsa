#ifndef LLVM_CUSTOM_H
#define LLVM_CUSTOM_H
#include "llvm/Analysis/CallGraph.h"
#include "dsa/AddressTakenAnalysis.h"
#include "dsa/AllocatorIdentification.h"
#include "dsa/CallTargets.h"
#include "dsa/DataStructure.h"
#include "dsa/DSCallGraph.h"
#include "dsa/DSGraph.h"
#include "dsa/DSGraphTraits.h"
#include "dsa/DSNode.h"
#include "dsa/DSSupport.h"
#include "dsa/EntryPointAnalysis.h"
#include "dsa/keyiterator.h"
#include "dsa/stl_util.h"
#include "dsa/super_set.h"
#include "dsa/svset.h"
#include "dsa/TypeSafety.h"
#include "llvm/Analysis/LoopInfo.h"
//#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "llvm/Summary/SummaryPass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/AliasAnalysis.h"

using namespace llvm;


/*class Hello : public FunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    Hello();

    virtual bool runOnFunction(Function &F);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    int printHello();
};*/

namespace llvm {
  struct BasicAliasAnalysis;
  struct PromotePass;

  AddressTakenAnalysis* createAddressTakenAnalysisPass();
  LocalDataStructures* createLocalDataStructuresPass();
  LoopInfo *createLoopInfoPass();
  AllocIdentify *createAllocIdentifyPass();
  StdLibDataStructures* createStdLibDataStructuresPass();
  BUDataStructures *createBUDataStructuresPass();
  CompleteBUDataStructures *createCompleteBUDataStructuresPass();
  EquivBUDataStructures *createEquivBUDataStructuresPass();
  TDDataStructures *createTDDataStructuresPass();
  EQTDDataStructures *createEQTDDataStructuresPass();
  TargetLibraryInfo *createTargetLibraryInfoPass();
  AliasAnalysis *createAliasAnalysisPass();
  BasicAliasAnalysis *createNewBasicAliasAnalysisPass();
  //Hello *createHelloPass();
  DominatorTree *createNewDominatorTreePass();
  PromotePass *createNewPromotePassPass();
  SummaryPass *createSummaryPass();
  CallGraph *createCallGraphPass();
}
#endif
