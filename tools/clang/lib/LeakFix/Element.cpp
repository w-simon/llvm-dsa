#include "clang/Analysis/CFG.h"
#include "clang/LeakFix/Element.h"


Element::Element(CFGBlock* cfgBk,CFGBlock::iterator iter){
	this->elementBlock = cfgBk;
	this->elementIterator= iter;
	this->index = iter - cfgBk->begin();
		
  this->isMalloc = false;
	this->useSet= new set<int>;
	this->freeSet= new set<int>;
	this->id = new set<int>;
        
}

Element::Element(CFGBlock* cfgBk,CFGBlock::iterator iter,set<int> *id0){
	this->elementBlock = cfgBk;
	this->elementIterator= iter;
	this->index = iter - cfgBk->begin();
	this->isMalloc = false;
	this->useSet= new set<int>;
	this->freeSet= new set<int>;
    this->id= new set<int>;
	for(set<int>::iterator i = id0->begin();i!=id0->end();i++){
		this->id->insert(*i);
	}
}
Element::Element(const Element &e){
	    this->elementBlock = e.elementBlock;
		this->elementIterator = e.elementIterator;
		this->index= e.index;
		this->isMalloc = e.isMalloc;
		this->useSet= new set<int>;
		this->freeSet= new set<int>;
		for(set<int>::iterator i = e.useSet->begin();i!=e.useSet->end();i++){
              		this->useSet->insert(*i);
		}
		for(set<int>::iterator i = e.freeSet->begin();i!=e.freeSet->end();i++){
              		this->freeSet->insert(*i);
		}
    this->id= new set<int>;
	for(set<int>::iterator i = e.id->begin();i!=e.id->end();i++){
		this->id->insert(*i);
	}




}

Element &Element::operator=(const Element &e){
	    this->elementBlock = e.elementBlock;
		this->elementIterator = e.elementIterator;
		this->index= e.index;
		this->isMalloc = e.isMalloc;
		this->useSet= new set<int>;
		this->freeSet= new set<int>;
		for(set<int>::iterator i = e.useSet->begin();i!=e.useSet->end();i++){
              		this->useSet->insert(*i);
		}
		for(set<int>::iterator i = e.freeSet->begin();i!=e.freeSet->end();i++){
              		this->freeSet->insert(*i);
		}
    this->id= new set<int>;
	for(set<int>::iterator i = e.id->begin();i!=e.id->end();i++){
		this->id->insert(*i);
	}
  return *this;

}

Element::~Element(){
  delete this->useSet;
  delete this->freeSet;
  delete this->id;
}

Element::Element(){
	this->useSet= new set<int>;
	this->freeSet= new set<int>;
  this->id= new set<int>;
}

set<int>* Element::getID(){
	return this->id;
}

void Element::setID(set<int> &id0){
	 for(set<int>::iterator i = id0.begin();i!=id0.end();i++){
              		this->id->insert(*i);
	}
}
bool Element::operator== (const Element &e)const{
	return ((this->elementBlock->getBlockID()== e.elementBlock->getBlockID()) &&
						(this->elementIterator== e.elementIterator ));


}
bool Element::operator<(const Element &e)const
{

	if(this->elementBlock->getBlockID() == e.elementBlock->getBlockID()){
		return e.index < this->index;
	}
	else
		return e.elementBlock->getBlockID() < this->elementBlock->getBlockID();
			
}

CFGBlock* Element:: getBlock(){
		return this->elementBlock;
	}
int Element::getElementIndex(){
		return this->index;
	}
CFGBlock::iterator Element::getElementIterator(){
		return this->elementIterator;
	}
set<int>* Element::getUseSet(){
	return this->useSet;
}

bool Element::getMalloc(){
	return this->isMalloc;
}

void Element::setMalloc(){
  this->isMalloc = true;
}

set<int>* Element::getFreeSet(){

	return this->freeSet;
}


void Element::setUseSet(set<int> &set0){
	 for(set<int>::iterator i = set0.begin();i!=set0.end();i++){
              		this->useSet->insert(*i);
	}
}

void Element::setFreeSet(set<int> &set0){

	for(set<int>::iterator i = set0.begin();i!=set0.end();i++){
              		this->freeSet->insert(*i);
		}
}
bool Element::isUse(int id){
    for(set<int>::iterator it = this->useSet->begin(); it!= this->useSet->end();it++){

		if(*it == id)
			return true;
	}
	return false;
	
}
bool Element::isFree(int id){
     for(set<int>::iterator it = this->freeSet->begin(); it!= this->freeSet->end();it++){

		if(*it == id)
			return true;
	}
	return false;
	
}

void Element::print(){
  llvm::errs() << "Malloc set: ";
  for (set<int>::iterator I = id->begin(); I != id->end(); ++I){
    llvm::errs() << *I << " ";
  } 
  llvm::errs() << "\nUse set: ";
  for (set<int>::iterator I = useSet->begin(); I != useSet->end(); ++I){
    llvm::errs() << *I << " ";
  }
  llvm::errs() << "\nFree set: ";
  for (set<int>::iterator I = freeSet->begin(); I != freeSet->end(); ++I){
    llvm::errs() << *I << " ";
  }
  llvm::errs() << "\n";
}

