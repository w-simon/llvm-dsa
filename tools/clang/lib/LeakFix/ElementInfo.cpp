#include "clang/Analysis/CFG.h"
#include "clang/LeakFix/ElementInfo.h"


ElementInfo::ElementInfo(CFGBlock* cfgBk,CFGBlock::iterator iter){
		this->elementBlock = cfgBk;
		this->elementIterator= iter;
		this->index = iter - cfgBk->begin();
		this->id=0;

        
	}

ElementInfo::ElementInfo(CFGBlock* cfgBk,CFGBlock::iterator iter,int id0){
		this->elementBlock = cfgBk;
		this->elementIterator= iter;
		this->index = iter - cfgBk->begin();
		this->id=id0;
		
	}
ElementInfo::ElementInfo(const ElementInfo &e){
	    this->elementBlock = e.elementBlock;
	   this->elementIterator = e.elementIterator;
		this->index= e.index;
		




}

ElementInfo::ElementInfo(){}
int ElementInfo::getID(){
	return this->id;
}

void ElementInfo::setID(int id0){
	this->id=id0;
}
bool ElementInfo::operator== (const ElementInfo &e)const{
	return (this->id == e.id);


}
bool ElementInfo::operator<(const ElementInfo &e)const
{

	
	return e.id < this->id;
	
			
}
CFGBlock* ElementInfo:: getBlock(){
		return this->elementBlock;
	}
int ElementInfo::getElementIndex(){
		return this->index;
	}
CFGBlock::iterator ElementInfo::getElementIterator(){
		return this->elementIterator;
	}



