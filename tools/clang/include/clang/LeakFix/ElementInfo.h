#ifndef CLANG_ELEMENTINFO_H
#define CLANG_ELEMENTINFO_H

#include "clang/Analysis/CFG.h"
#include <vector>
#include <set>


using clang::CFGBlock;
using std::vector;
using std::set;
using std::pair;

struct compare_p{
	bool operator()(const pair<CFGBlock*,int> &p1, const pair<CFGBlock*,int> &p2){
		
		
	     if(p1.first->getBlockID()== p2.first->getBlockID()){
			 return p2.second > p1.second;
		 }
		 else{
			 return p2.first->getBlockID() > p1.first->getBlockID();
		 }
	}
};
class ElementInfo{
public:
	ElementInfo(CFGBlock* cfgBk,CFGBlock::iterator iter);
	ElementInfo(CFGBlock* cfgBk,CFGBlock::iterator iter,int id);

	ElementInfo();
	ElementInfo(const ElementInfo &e);
    
    	CFGBlock* getBlock();
	
    	int getElementIndex();

	CFGBlock::iterator getElementIterator();
	

	bool operator== (const ElementInfo &e)const;
	bool operator<(const ElementInfo &e)const;
	

	
	void setID(int id);
	int getID();


private:
    	CFGBlock* elementBlock;
	CFGBlock::iterator elementIterator;
	int index;
	
	int id;//malloc id 1-n
	

};
#endif
