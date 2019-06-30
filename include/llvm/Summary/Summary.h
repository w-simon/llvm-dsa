#ifndef LLVM_SUMMARY_H
#define LLVM_SUMMARY_H
#include "llvm/IR/Instructions.h"
#include "dsa/DSGraph.h"
#include "dsa/DSNode.h"
#include <map>
#include <set>
#include <algorithm>

using std::map;
using std::set;
using std::vector;
using llvm::CallInst;
using namespace llvm;


struct AllocInfo{
  int ID;
  CallInst *callInst;
  bool must;
  int argNum; //return:-1, global: -2
  std::string *global;
  vector<int> *level;
  bool escaped;
  bool freed;

  AllocInfo();
  AllocInfo(CallInst *c, bool m=true, int an=-1, std::string *g=NULL, vector<int> *l=NULL);

  bool operator==(AllocInfo &ai){
    return callInst==ai.callInst && argNum == ai.argNum && level->size() == ai.level->size() && std::equal(level->begin(), level->end(), ai.level->begin());
  }
};

struct Summary{
  map<DSNode*, vector<AllocInfo*> *> allocMap;
  map<DSNode*, vector<CallInst*> *> useMap;
  map<DSNode*, vector<CallInst*> *> freeMap;
};


class LevelMapper{
  static void mapLevel(vector<int> *level);
};
#endif
