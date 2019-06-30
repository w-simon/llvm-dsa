#ifndef CLOOP_H
#define CLOOP_H

#include<iostream>
#include"clang/Analysis/CFG.h"
#include"clang/AST/Decl.h"
#include<map>
#include<set>

using std::vector;
using std::set;

using clang::CFG;
using clang::CFGBlock;
class CLoop{
	private:
		CLoop * ParentLoop;
		vector<CLoop *> subLoops;
		set<CFGBlock* > basicBlocks;
		set<CFGBlock *> exitingBlock;
		CFGBlock * header;
	public:
	CLoop():ParentLoop(0){}
    ~CLoop(){
		for(size_t i=0, e = subLoops.size();i!= e;++i){

			delete subLoops[i];
		}
	}

	unsigned int getLoopDepth() const {
		unsigned int D=1;
		for(const CLoop * curLoop = ParentLoop; curLoop;
				curLoop = curLoop->ParentLoop){
            ++D;
		}
		return D;
	}
	CLoop *getParentLoop() const{
		return ParentLoop;
	}
     void setParentLoop(CLoop *L){
		 ParentLoop = L;
	 }
	CFGBlock * getHeader()const{

		return header;
	}
	void setHeader(CFGBlock * h){

		header = h;
	}
	bool isHeader(CFGBlock *block){
        return block == header;

	}
	bool isLoopBlock(CFGBlock *block){
         
          if(basicBlocks.find(block)!=basicBlocks.end()){
			  return true;
		  }
		  return false;
	}
	bool reviseOne(){
	for(set<CFGBlock *>::iterator i = basicBlocks.begin();i!=basicBlocks.end();i++){
		for(CFGBlock::succ_iterator pi = (*i)->succ_begin();pi!=(*i)->succ_end();pi++){
			if(!isLoopBlock(*pi)){	
				for(CFGBlock::succ_iterator si = (*pi)->succ_begin();si!=(*pi)->succ_end();si++){
					if(*pi != *si){
						if(isLoopBlock(*si)){
							basicBlocks.insert(*pi);
							return true;
						}
					}
				}
			}
		}
	
	}
	return false;
	}
	void reviseLoop(){
           while(reviseOne()){}
	}
     	bool contains(const CLoop *L) const{
		 if(L==this) return true;
		 if(L==0) return false;
		 return contains(L->getParentLoop());
	 }
	 vector<CLoop*> &getSubLoops(){
		 return subLoops;
	 }

	 void setSubLoops(vector<CLoop *> &v){
		 subLoops = v;
	 }
	 bool empty() const{
		 return subLoops.empty();
	 }
	 set<CFGBlock*> getBlocks() const{
		 return basicBlocks;
	 }
     
	 void setBlocks(set<CFGBlock *> &cb){
          basicBlocks = cb;
		 
	 }
	 set<CFGBlock *> getExitingBlocks() const{
		 return exitingBlock;
	 }
	 void setExitingBlocks(set<CFGBlock *> &ecb){
         exitingBlock = ecb;
	 }
	 unsigned int getBumBlocks() const {
		 return basicBlocks.size();
	 }
	 typedef vector<CLoop *>::const_iterator iterator;
	 iterator begin() const{
		 return subLoops.begin();
	 }

	 iterator end() const {

		 return subLoops.end();
	 }

	 typedef set<CFGBlock *>::const_iterator block_iterator;
	 block_iterator block_begin() const {
		 return basicBlocks.begin();
	 }

	 block_iterator block_end() const {
		 return basicBlocks.end();
	 }
};
#endif
