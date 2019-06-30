/*************************************************************************
  > File Name: Fix.h
  > Author: ma6174
  > Mail: ma6174@163.com 
  > Created Time: Fri 15 Aug 2014 04:28:11 PM CST
 ************************************************************************/
#ifndef LEAKFIX_FIX_H
#define LEAKFIX_FIX_H

#include "clang/LeakFix/VariableAnalyzer.h"
#include<iostream>
#include"clang/LeakFix/ElementInfo.h"
#include"clang/Analysis/CFG.h"
#include"clang/LeakFix/Element.h"
#include"clang/AST/Decl.h"
#include "clang/LeakFix/GraphAnalyzer.h"
#include<map>
#include<string>
#include "clang/LeakFix/NodeRecognizer.h"

using std::map;
using std::set;
using std::string;

using clang::VarDecl;
using clang::CFG;
using clang::CFGBlock;
using clang::Stmt;
using clang::FileEntry;

struct structNode{

  set<int> fixSet;
  string var;

  pair<int,int> earBlockEdge;
  pair<int,int> earElementEdge;
  bool isBlockEdge;
  bool friend operator==(const structNode & s1,const structNode &s2){
    return((s1.fixSet == s2.fixSet)&&(s1.var==s2.var)&&(s1.earBlockEdge==s2.earBlockEdge)&&
        (s1.earElementEdge==s2.earElementEdge)&&(s1.isBlockEdge==s2.isBlockEdge));
  }
};
struct compare_cfg2{
  CFGBlock* block;
  bool operator()(const CFGBlock* b1,const CFGBlock* b2)const{
    return b2->getBlockID() < b1->getBlockID();


  }
};
struct PrintInfo{
  string var;
  bool isBlockEdge;
  int first;
  int second;
  PrintInfo(string v, bool i, int f, int s) : var(v), isBlockEdge(i), first(f), second(s){}
  bool friend operator==(const PrintInfo & p1,const PrintInfo & p2){
	  return ((p1.first==p2.first)&&(p1.second == p2.second)&&(p1.var.compare(p2.var)==0));
  }
};

class PrintComp{
  public:
    bool operator()(const PrintInfo &pi1, const PrintInfo &pi2) const{
      if (pi1.var.compare(pi2.var) < 0)
        return true;
      if (pi1.var.compare(pi2.var) > 0)
        return false;
      if (pi1.isBlockEdge < pi2.isBlockEdge)
        return true;
      if (pi1.isBlockEdge > pi2.isBlockEdge)
        return false;
      if (pi1.first < pi2.first)
        return true;
      if (pi1.first > pi2.first)
        return false;
      if (pi1.second < pi2.second)
        return true;
      if (pi1.second > pi2.second)
        return false;
      return false;
    }
};
struct FixElement{
	
	string var;
  string rep;
	int lineNumber;
	int columNumber;
  int type = 0;//type 1: ret, rewrite and free; type 2: tempVar, rewrite but not free
	int lineEndNumber;
  int columEndNumber;
  bool isFront;
    FixElement(string v, int i, int j, int t=0, int l=0, int e=0) : var(v), rep(v), lineNumber(i), columNumber(j), type(t), lineEndNumber(l), columEndNumber(e), isFront(false){}

};

class FixComp{
  public:
    bool operator()(const FixElement &fe1,const FixElement &fe2) const{
      if(fe1.lineNumber<fe2.lineNumber)
        return true;
      if(fe1.lineNumber>fe2.lineNumber)
        return false;
      if(fe1.columNumber<fe2.columNumber)
        return true;
      if(fe1.columNumber>fe2.columNumber)
        return false;
      return false;
    }
};
class Fix{
  public:
    Fix(CFG *cfg0, NodeRecognizer *nr,  GraphAnalyzer *ga, VariableAnalyzer *va);
    ~Fix();
    bool isEntry(CFGBlock *block);
    bool isExit(CFGBlock *block);
    // the function get the malloc set of each edge
    void getFixEdge();

    //should initVisitFirst
    void forward();
    void forwardProcess(CFGBlock *block);
    void processElement(CFGBlock *block);

    void initMap();

    //init IN and OUT
    void initStructIN();

    void initVisit();

    void unionVector(vector<structNode> &v);
    bool includeNode(vector<structNode> v,structNode n);
    vector<structNode> meetNotNull(vector<structNode> in,vector<structNode> out);
    bool canMeet(structNode s1,structNode s2);

    ElementInfo getElementInfo(int id);
    void printAnalyzedResult();
    void printExitResult();
    void printResult();
    void printBlock(clang::CFGBlock *block);
    void printBlocks();
    void printFixLocation(structNode&);
    void printStructInfo();
    void printCmdResult();
    void printBlockMap();


    void analyze();
    multiset<FixElement,FixComp> toFixSet();
    string getFileName();
    const clang::FileEntry *getFileEntry(const Stmt *);
    pair<pair<int,int>,bool> printAssist(int,int,bool,bool&);


  private:
    //should change the constructor to init the elementinfoList;
    map<int,ElementInfo> *elementInfoList;

    //IN should init by the Graphanalyzer->analyze(); mallocList_IN --> IN  mallocList_OUT -->OUT
    map<pair<int,int>,set<int> > *IN;


    map<pair<int,int>,set<int> > *OUT;


    CFG *cfg;
    set<CFGBlock*,compare_cfg2> *visit;
	set<PrintInfo,PrintComp> *printed;
	multiset<FixElement,FixComp> * toFix;
    std::string file_name;

    //must init the two map
    map<pair<int,int>,map<set<int>,string> > *blockEdgeMap;
    map<pair<int,int>,map<set<int>,string> > *elementEdgeMap;
    map<pair<int,int>,vector<structNode> > *structIN;
    map<pair<int,int>, vector<structNode> > *structOUT;

    NodeRecognizer *NR;
    GraphAnalyzer *GA;
    VariableAnalyzer *VA;	
  




};

#endif
