#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Custom.h"
#include "llvm/Summary/SummaryPass.h"
#include "dsa/DataStructure.h"
#include "llvm/Support/raw_ostream.h"
#include <time.h>
using namespace llvm;

char SummaryPass::ID = 0;

extern void markAlloc(Function *);
extern void markUse(Function *);
extern void markFree(Function *);
extern EquivBUDataStructures *buds;//The DSA BU pass for our use
extern EQTDDataStructures *tdds;//The DSA TD pass for our use
std::map<DSNode*, std::set<Value*> > nodeToValueMap;
std::map<DSNode*, std::set<DSNode*> > nodeToNodeMap;

SummaryPass::SummaryPass() : ModulePass(ID){
}

void constructNodeMap(Module &m){
  for (Module::iterator MI = m.begin(); MI != m.end(); ++MI){
    if (MI->isDeclaration()) continue;
    DSGraph *G = tdds->getDSGraph(*MI);
    for (Function::arg_iterator AI = MI->arg_begin(); AI != MI->arg_end(); ++AI){
      Argument *Arg = AI;
      DSNodeHandle &H = G->getNodeForValue(Arg);
      nodeToValueMap[H.getNode()].insert(Arg);

    }
    for (Function::iterator FI = MI->begin(); FI != MI->end(); ++FI){
      for (BasicBlock::iterator BI = FI->begin(); BI != FI->end(); ++BI){
        Instruction *Ins = BI;
        DSNodeHandle &H = G->getNodeForValue(Ins);
        nodeToValueMap[H.getNode()].insert(Ins);
      }
    }

    for (DSGraph::node_iterator NI = G->node_begin(); NI != G->node_end(); ++NI){
      for (DSNode::edge_iterator EI = NI->edge_begin(); EI != NI->edge_end(); ++EI){
        DSNodeHandle &H = EI->second;
        nodeToNodeMap[H.getNode()].insert(NI);
      }
    }
  }
}

void SummaryPass::getAnalysisUsage(AnalysisUsage &AU) const{
  AU.setPreservesAll();
  AU.addRequired<AllocIdentify>();
  AU.addRequired<EquivBUDataStructures>();
  AU.addRequired<EQTDDataStructures>();
}

bool SummaryPass::runOnModule(Module &m){
  llvm::errs() << "Running summary on the merged module...\n";
  M = &m;
  clock_t start, end;
  start = clock();
  ai = &getAnalysis<AllocIdentify>();
  eqbus = &getAnalysis<EquivBUDataStructures>();
  eqtd = &getAnalysis<EQTDDataStructures>();
  buds = eqbus;
  tdds = eqtd;
  DSGraph *GG = tdds->getGlobalsGraph();
  constructNodeMap(m);
  SummaryAlgorithm(markAlloc, &m);
  SummaryAlgorithm(markUse, &m);
  SummaryAlgorithm(markFree, &m);
  end = clock();
  summaryTime = ((double)(end- start)) / CLOCKS_PER_SEC;
  return false;
}
