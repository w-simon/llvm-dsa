#ifndef LEAKFIX_ELEMENT_H
#define LEAKFIX_ELEMENT_H

#include "clang/Analysis/CFG.h"
#include <vector>
#include <set>


using clang::CFGBlock;
using std::vector;
using std::set;
using std::pair;
using std::iterator;


class Element{
public:
	Element(CFGBlock* cfgBk,CFGBlock::iterator iter);
	Element(CFGBlock* cfgBk,CFGBlock::iterator iter,set<int> *id0);

	Element();
	Element(const Element &e);
  Element &operator=(const Element &e);
	~Element();
  void print();
    
    CFGBlock* getBlock();
	
    int	getElementIndex();

	CFGBlock::iterator getElementIterator();
	bool getMalloc();
  void setMalloc();

	bool operator== (const Element &e)const;
	bool operator<(const Element &e)const;
	set<int>* getUseSet();
	set<int>* getFreeSet();
	bool isUse(int id);
	bool isFree(int id);

	void setUseSet(set<int> &set);
	void setFreeSet(set<int> &set);
	void setID(set<int> &id);
	set<int>* getID();


private:
    CFGBlock* elementBlock;
	CFGBlock::iterator elementIterator;
	int index;
	bool isMalloc;
	set<int> *id;//malloc id 1-n
	set<int> *useSet;
	set<int> *freeSet;
	

};

#endif
