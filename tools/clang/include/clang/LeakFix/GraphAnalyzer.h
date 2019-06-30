#ifndef LEAKFIX_GRAPHANALYZER_H
#define LEAKFIX_GRAPHANALYZER_H

#include "clang/LeakFix/NodeRecognizer.h"
#include "clang/Analysis/CFG.h"
#include "clang/LeakFix/ElementInfo.h"
#include "clang/LeakFix/Element.h"
#include"clang/LeakFix/FixInfo.h"
#include "clang/LeakFix/EdgeOfBlock.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Basic/SourceManager.h"
#include "clang/LeakFix/CLoopInfo.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>
#include<map>
#include<list>


using clang::CFG;
using clang::CFGElement;
using clang::CFGBlock;
using clang::ASTUnit;
using clang::SourceLocation;
using clang::FullSourceLoc;
using clang::SourceManager;
using std::map;
using std::pair;
using std::list;
using llvm::raw_ostream;
struct compare_int{
  bool operator()(const int &a, const int &b)const{
    return b>a;

  }
};
struct compare_cfg{
  CFGBlock* block;
  bool operator()(const CFGBlock* b1,const CFGBlock* b2)const{
    return b2->getBlockID() < b1->getBlockID();


  }
};
class GraphAnalyzer{

  public:
    //if there is no element in a CFGBlock creat a empty one for the block which index       set 0  
    map<pair<int,int >, Element >* ElementList;
    map<pair<int,int>, set<int> > *mallocList_INF;
    map<pair<int,int>, set<int> > *mallocList_OUTF;
    map<pair<int,int>, set<int> > *IN;
    map<pair<int,int>, set<int> > *OUT;
	

  public:
    GraphAnalyzer(CFG *cfg, ASTUnit *unit, NodeRecognizer *nr, CLoopInfo * cl);
    ~GraphAnalyzer();
    void analyze();
    void printElementList();
    void initVisit();
    void printResult();
    void printUseResult();

    //this function is to initElementList but you should initElement in the constructor through parameter
    void initElementList();

    //init the Map first when ananlysis
    void initMap();

    //should initVisit first
    void forward();
    void forwardProcess(CFGBlock* block);
    void processElement(CFGBlock* block);

    //should initVisit first
    void forwardFree();
    void forwardProcessFree(CFGBlock* block);
    void processElementFree(CFGBlock* block);

    //should initVisit first
    void backwardFree();
    void backwardProcessFree(CFGBlock* block);
    void backwardProcessElementFree(CFGBlock* block);

    //should initVisit first
    void backward();
    void backwardProcess(CFGBlock *block);
    void backwardElementProF(CFGBlock *block);

    //the dataflow algrithm
    void dataflow();

    bool isEntry(CFGBlock * block);
    bool isExit(CFGBlock* block );

    void printMap();
    void printSet(raw_ostream &os, set<int> &set0);

    //add block
    void addBlockToCFG();
    void addBlock(CFGBlock *block_in,CFGBlock *block_out);

	//handle loop information
	
	void printLoopInfo();
	void initLoopVisit(CLoop * l);
	void initUsedMap(CLoop *l);

    void forward(CLoop *l);
    void forwardProcess(CFGBlock* block,CLoop *l);
    void processElement(CFGBlock* block,CLoop *l);

    //should initVisit first
    void forwardFree(CLoop *l);
    void forwardProcessFree(CFGBlock* block, CLoop *l);
    void processElementFree(CFGBlock* block, CLoop *l);

    //should initVisit first
    void backwardFree(CLoop *l);
    void backwardProcessFree(CFGBlock* block, CLoop *l);
    void backwardProcessElementFree(CFGBlock* block, CLoop *l);

    //should initVisit first
    void backward(CLoop *l);
    void backwardProcess(CFGBlock *block, CLoop *l);
    void backwardElementProF(CFGBlock *block, CLoop *l);

	void loopDataflow(CLoop *l);
	void analyzeLoop(CLoop *l);
   void visitLoop(CLoop *l);

  private:
	
    CFG *cfg;
    ASTUnit * astUnit;

    CLoopInfo * cli;



    map<pair<int,int>, set<int> > *freeList_INB;
    map<pair<int,int>, set<int> > *freeList_OUTB;




    map<pair<int ,int>, set<int > >* useList_OUTB;
    map<pair<int,int>, set<int > >*  useList_INB;

    map<pair<int,int>,  set<int> >  *freeList_INF;
    map<pair<int,int>,  set<int> >  *freeList_OUTF;







    set<CFGBlock*,compare_cfg> *visit;
    NodeRecognizer *NR;

};
#endif
