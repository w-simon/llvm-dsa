#ifndef CLOOP_INFO_H
#define CLOOP_INFO_H
#include"clang/LeakFix/CLoop.h"
#include<iostream>
#include<map>

using clang::CFG;
using clang::CFGBlock;
using std::vector;
using std::map;

class CLoopInfo{

	map<CFGBlock*, CLoop*> BBMap;
	vector<CLoop *> topLevelLoops;
public:
	CLoopInfo(){

	}
	~CLoopInfo(){
		releaseMemory();
	}
    unsigned int topLevelSize(){
		return topLevelLoops.size();
	}
	unsigned int BBMapSize(){

		return BBMap.size();
	}
	void releaseMemory(){
		for(vector<CLoop *>::iterator I= topLevelLoops.begin(); I!=topLevelLoops.end();I++){
			delete *I;
		}
		BBMap.clear();
		topLevelLoops.clear();
	}

	void setTopLevelLoops(vector<CLoop *> &v){
		topLevelLoops =v;
	}

	void setMap(map<CFGBlock*, CLoop*> &m){

		BBMap = m;
	}
	typedef vector<CLoop *>::const_iterator iterator;

	iterator begin() const {
		return topLevelLoops.begin();
	}

	iterator end() const {
		return topLevelLoops.end();
	}

	bool empty() const {
		return topLevelLoops.empty();
	}
    CLoop * getLoopFor(CFGBlock *B)  {
         if(BBMap.find(B)!= BBMap.end()){
			 return BBMap[B];
		 }
		 llvm::errs()<<"get Loop for NULL ocur!\n";
		 return NULL;


	}
    
	CLoop *operator[](CFGBlock *BB)  {
		return getLoopFor(BB);
	}

	unsigned int getLoopDepth(CFGBlock *BB) {

		CLoop *L = getLoopFor(BB);
		return L? L->getLoopDepth():0;
	}

};
#endif

