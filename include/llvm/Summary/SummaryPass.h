#ifndef LLVM_SUMMARYPASS_H
#define LLVM_SUMMARYPASS_H

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "dsa/DataStructure.h"
#include "dsa/AllocatorIdentification.h"

class SummaryPass : public ModulePass{
  public:
    static char ID;
    SummaryPass();

    virtual bool runOnModule(Module &m);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual void SummaryAlgorithm(void (*)(Function *), Module *M);
    void analyzeWrappers(Module &M);
    void printSummary(llvm::Function *F);
    void printSummary();
    double summaryTime;

  private:
    AllocIdentify *ai;
    EquivBUDataStructures *eqbus;
    EQTDDataStructures *eqtd;
    Module *M;
};
#endif
