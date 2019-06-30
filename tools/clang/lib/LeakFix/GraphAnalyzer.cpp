#include "clang/LeakFix/GraphAnalyzer.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Expr.h"
#include "clang/Basic/LangOptions.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/FileManager.h"
#include "clang/AST/ASTContext.h"
#include "clang/LeakFix/CLoop.h"
#include "clang/LeakFix/CLoopInfo.h"
#include <map>
#include <iostream>
using namespace clang;
using namespace std;
extern ASTContext *Contex;
extern FunctionDecl *FD;

void GraphAnalyzer::printLoopInfo(){
	clang::LangOptions lo;
	//this->cfg->dump(lo,true);
	for(CLoopInfo::iterator i = cli->begin();i!=cli->end();i++){
		bool isAbandon =false;
		for(CLoop::block_iterator j=(*i)->block_begin();j!=(*i)->block_end();j++){
			if((*j) == NULL){

			}
			if((*j)->getParent()!=cfg){
                isAbandon = true;
				break;
			}
			if((*i)->isHeader(*j)){

			}
			if((*i)->isLoopBlock(*j)){
			}
		}

		if(!isAbandon){
		   analyzeLoop(*i);}
	
	
	}
}
void GraphAnalyzer::visitLoop(CLoop *l){
		for(CLoop::block_iterator j=l->block_begin();j!=l->block_end();j++){
			if((*j) == NULL){

				llvm::errs()<<"block is null ^@^\n";
			}
			llvm::errs()<<"block: ";
			llvm::errs()<<(*j)->getBlockID();
			if((*j)->getParent()!=cfg){
				llvm::errs()<<"block is from other cfg ^@^\n";
			}
			if(l->isHeader(*j)){

				llvm::errs()<<"is Header\n";
			}
			if(l->isLoopBlock(*j)){
				llvm::errs()<<"is loop block";
			}
			llvm::errs()<<"\n";
		}

}
void GraphAnalyzer::analyzeLoop(CLoop *l){
	initUsedMap(l);
	initLoopVisit(l);
 
    forward(l);

    initLoopVisit(l);
    forwardFree(l);

    initLoopVisit(l);
    backward(l);


    initLoopVisit(l);
    backwardFree(l);
	loopDataflow(l);

    for(CLoop::iterator i=l->begin();i!=l->end();i++){
		analyzeLoop(*i);
	}
}

void GraphAnalyzer::loopDataflow(CLoop *l){

  CFGBlock * block;
  unsigned int block_id;
  unsigned int size;
  int index;
  set<int> mallocINSet;
  set<int> mallocOUTSet;
  set<int> INSet;
  set<int> OUTSet;

  set<int> UseINSet;
  set<int> UseOUTSet;

  set<int> freeINSet;
  set<int> freeOUTSet;

  set<int> freeINSetB;
  set<int> freeOUTSetB;

 
  map<pair<int,int>,set<int> >::iterator mapIter1;
  for(CLoop::block_iterator i= l->block_begin(); i!=l->block_end(); ++i){

    block = *i;
    if(isEntry(block))continue;

    block_id = block->getBlockID();
    size = block->size();
    for(clang::CFGBlock::iterator iter = block->begin(); iter!=block->end();iter++)
    {	


      index = iter- block->begin();
      pair<int,int> idPair(block_id,index);

      mapIter1 = this->useList_INB->find(idPair);
      UseINSet = mapIter1->second;

      mapIter1 = this->useList_OUTB->find(idPair);
      UseOUTSet = mapIter1->second;

      mapIter1 = this->IN->find(idPair);
      mallocINSet = mapIter1->second;

      mapIter1= this->OUT->find(idPair);
      mallocOUTSet = mapIter1->second;

      mapIter1 = this->freeList_INF->find(idPair);
      freeINSet = mapIter1->second;

      mapIter1= this->freeList_OUTF->find(idPair);
      freeOUTSet = mapIter1->second;

      mapIter1 = this->freeList_INB->find(idPair);
      freeINSetB = mapIter1->second;

      mapIter1= this->freeList_OUTB->find(idPair);
      freeOUTSetB = mapIter1->second;

      INSet = mallocINSet;
      OUTSet = mallocOUTSet;
      for(set<int>::iterator i =freeINSetB.begin(); i !=freeINSetB.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }
      for(set<int>::iterator i =freeINSet.begin(); i !=freeINSet.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }
      for(set<int>::iterator i =UseINSet.begin(); i !=UseINSet.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }

      //out
      for(set<int>::iterator i =freeOUTSetB.begin(); i !=freeOUTSetB.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      for(set<int>::iterator i =freeOUTSet.begin(); i !=freeOUTSet.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      for(set<int>::iterator i =UseOUTSet.begin(); i !=UseOUTSet.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      mapIter1 = this->mallocList_INF->find(idPair);
      mallocINSet = mapIter1->second;

      mapIter1= this->mallocList_OUTF->find(idPair);
      mallocOUTSet = mapIter1->second;
	  for(set<int>::iterator i= mallocINSet.begin();i!=mallocINSet.end();i++){
		  INSet.insert(*i);
	  }
	  for(set<int>::iterator i= mallocOUTSet.begin();i!=mallocOUTSet.end();i++){
		  OUTSet.insert(*i);
	  }
	  
      pair<pair<int,int>, set<int> >  elePair(idPair,INSet);
      pair<pair<int,int>, set<int> > elePair0(idPair,OUTSet);

      mapIter1 = this->mallocList_INF->find(idPair);
      if(mapIter1!=this->mallocList_INF->end()){

        this->mallocList_INF->erase(mapIter1);

      }else{
      }

      this->mallocList_INF->insert(elePair);

      mapIter1 = this->mallocList_OUTF->find(idPair);
      if(mapIter1!=this->mallocList_OUTF->end()){

        this->mallocList_OUTF->erase(mapIter1);

      }else{
      }

      this->mallocList_OUTF->insert(elePair0);

    }
    if(size==0){
      pair<int,int> idPair(block_id,0);
      mapIter1 = this->useList_INB->find(idPair);
      UseINSet = mapIter1->second;

      mapIter1 = this->useList_OUTB->find(idPair);
      UseOUTSet = mapIter1->second;

      mapIter1 = this->IN->find(idPair);
      mallocINSet = mapIter1->second;

      mapIter1= this->OUT->find(idPair);
      mallocOUTSet = mapIter1->second;

      mapIter1 = this->freeList_INF->find(idPair);
      freeINSet = mapIter1->second;

      mapIter1= this->freeList_OUTF->find(idPair);
      freeOUTSet = mapIter1->second;

      mapIter1 = this->freeList_INB->find(idPair);
      freeINSetB = mapIter1->second;

      mapIter1= this->freeList_OUTB->find(idPair);
      freeOUTSetB = mapIter1->second;

      INSet = mallocINSet;
      OUTSet = mallocOUTSet;
      for(set<int>::iterator i =freeINSetB.begin(); i !=freeINSetB.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }
      for(set<int>::iterator i =freeINSet.begin(); i !=freeINSet.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }
      for(set<int>::iterator i =UseINSet.begin(); i !=UseINSet.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }

      //out
      for(set<int>::iterator i =freeOUTSetB.begin(); i !=freeOUTSetB.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      for(set<int>::iterator i =freeOUTSet.begin(); i !=freeOUTSet.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      for(set<int>::iterator i =UseOUTSet.begin(); i !=UseOUTSet.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      mapIter1 = this->mallocList_INF->find(idPair);
      mallocINSet = mapIter1->second;

      mapIter1= this->mallocList_OUTF->find(idPair);
      mallocOUTSet = mapIter1->second;
	  for(set<int>::iterator i= mallocINSet.begin();i!=mallocINSet.end();i++){
		  INSet.insert(*i);
	  }
	  for(set<int>::iterator i= mallocOUTSet.begin();i!=mallocOUTSet.end();i++){
		  OUTSet.insert(*i);
	  }
      pair<pair<int,int>, set<int> >  elePair(idPair,INSet);
      pair<pair<int,int>, set<int> > elePair0(idPair,OUTSet);
     mapIter1 = this->mallocList_INF->find(idPair);
     if(mapIter1!=this->mallocList_INF->end()){

       this->mallocList_INF->erase(mapIter1);

     }else{
     }

     this->mallocList_INF->insert(elePair);

     mapIter1 = this->mallocList_OUTF->find(idPair);
     if(mapIter1!=this->mallocList_OUTF->end()){

       this->mallocList_OUTF->erase(mapIter1);

     }else{
     }

     this->mallocList_OUTF->insert(elePair0);
    }

  }
}
void GraphAnalyzer::backward(CLoop *l){
  CFGBlock* cb;

  set<CFGBlock*,compare_cfg>::reverse_iterator i=visit->rbegin();
  if(i!=this->visit->rend()){
    cb = (*i);
    this->visit->erase(cb);
    if(isEntry(cb)||isExit(cb)){
      backward(l);
    }
    else{
      backwardProcess(cb,l);
      backward(l);
    }

  }

}
void GraphAnalyzer::backwardProcess(CFGBlock * block, CLoop *l){
  if(isEntry(block))return;
  unsigned int id = block->getBlockID();
  unsigned int size = block->size();//if no elemnt we will use the created single element 
  if(size!=0)size--;

  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->useList_OUTB->find(pair<int,int>(id,size));
  set<int> freeUseOutSet = mapIter->second;
  set<int>  newFreeUseOutSet;



  mapIter = this->useList_INB->find(pair<int,int>(id,size));
  set<int>  freeUseINSet = mapIter->second;
  clang::LangOptions langOptions;

  for(clang::CFGBlock::const_succ_iterator i= block->succ_begin(); i!=block->succ_end(); ++i){
    if (!*i){
      continue;
    }
	if(l->isHeader(*i))continue;
	if(!l->isLoopBlock(*i))continue;
    int succID=(*i)->getBlockID();

    set<int> succINSet;
    mapIter = this->useList_INB->find(pair<int,int>(succID,0));
    succINSet= mapIter->second;
    for(set<int>::iterator iter= succINSet.begin();iter!= succINSet.end();iter++){

      newFreeUseOutSet.insert(*iter);
    }


  }
  mapIter = this->useList_OUTB->find(pair<int,int>(id,size));

  this->useList_OUTB->erase(mapIter);

  pair<int,int> idPair(id,size);

  pair<pair<int,int>, set<int> > elePair(idPair,newFreeUseOutSet);
  this->useList_OUTB->insert(elePair);

  if(block->size()==0){
    if(newFreeUseOutSet!=freeUseINSet){
	  if(!l->isHeader(block)){
		  for(clang::CFGBlock::const_pred_iterator i=block->pred_begin(); i!=block->pred_end(); i++){
			if(!l->isLoopBlock(*i))continue;
			this->visit->insert(*i);
		}
	  }
      mapIter = this->useList_INB->find(pair<int,int>(id,size));

      this->useList_INB->erase(mapIter);

      pair<int,int> idPair(id,size);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeUseOutSet);
      this->useList_INB->insert(elePair);

    }

  }
  else{
    backwardElementProF(block,l);

  }



}
void GraphAnalyzer::backwardElementProF(CFGBlock *block,CLoop *l){
  unsigned int id = block->getBlockID();
  unsigned int size= block->size();
  size--;
  set<int> toAddElement;
  bool change=false;

  map<pair<int,int>,set<int> >::iterator mapIter;
  mapIter = this->useList_OUTB->find(pair<int,int>(id,size));
  set<int>  freeUseOutSet = mapIter->second;


  mapIter = this->useList_INB->find(pair<int,int>(id,size));
  set<int>  freeUseINSet = mapIter->second;
  set<int>  newFreeUseOUTSet;

  map<pair<int,int>,Element>::iterator elementIter;
  elementIter = this->ElementList->find(pair<int,int>(id,size));
  Element visitElement = elementIter->second;

  set<int> * freeSet = visitElement.getFreeSet();
  set<int> * useSet = visitElement.getUseSet();
  set<int> * killSet= visitElement.getID();

  for(set<int>::iterator i= useSet->begin(); i!=useSet->end();i++){
    freeUseOutSet.insert(*i);
  }
  for(set<int>::iterator i=killSet->begin(); i!=killSet->end();i++){
    freeUseOutSet.erase(*i);
  }
  if(freeUseINSet!=freeUseOutSet){
    change=true;
    mapIter = this->useList_INB->find(pair<int,int>(id,size));

    this->useList_INB->erase(mapIter);

    pair<int,int> idPair(id,size);

    pair<pair<int,int>, set<int> > elePair(idPair,freeUseOutSet);
    this->useList_INB->insert(elePair);

  }
  // 1-size()-2 elements

  if(block->size()>1){
    for(int i=block->size()-2; i>=0;i--){
      mapIter = this->useList_INB->find(pair<int,int>(id,i));
      freeUseINSet = mapIter->second;

      mapIter = this->useList_INB->find(pair<int,int>(id,i+1));
      newFreeUseOUTSet  = mapIter->second;

      mapIter = this->useList_OUTB->find(pair<int,int>(id,i));


      this->useList_OUTB->erase(mapIter);
      pair<int,int> idPair(id,i);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeUseOUTSet);
      this->useList_OUTB->insert(elePair);


      elementIter = this->ElementList->find(pair<int,int>(id,i));
      visitElement = elementIter->second;


      useSet = visitElement.getUseSet();
      killSet = visitElement.getID();

      for(set<int>::iterator i= useSet->begin(); i!=useSet->end();i++){
        newFreeUseOUTSet.insert(*i);
      }
      for(set<int>::iterator i= killSet->begin();i!= killSet->end(); i++){
        newFreeUseOUTSet.erase(*i);
      }

      if(newFreeUseOUTSet!= freeUseINSet){
        change = true;
        mapIter = this->useList_INB->find(pair<int,int>(id,i));

        this->useList_INB->erase(mapIter);

        pair<int,int> idPair(id,i);

        pair<pair<int,int>, set<int> > elePair(idPair,newFreeUseOUTSet);
        this->useList_INB->insert(elePair);


      }
    }
  }
  if(change&& (!l->isHeader(block))){
    for(clang::CFGBlock::const_pred_iterator i=block->pred_begin(); i!=block->pred_end(); i++){
	  if(!l->isLoopBlock(*i))continue;
      this->visit->insert(*i);
    }
  }


}
void GraphAnalyzer::backwardFree(CLoop *l){
  CFGBlock* cb;

  set<CFGBlock*,compare_cfg>::reverse_iterator i=visit->rbegin();
  if(i!=this->visit->rend()){
    cb = (*i);
    this->visit->erase(cb);
    if(isEntry(cb)||isExit(cb)){
      backwardFree(l);
    }
    else{
      backwardProcessFree(cb,l);
      backwardFree(l);
    }

  }
}
void GraphAnalyzer::backwardProcessFree(CFGBlock * block,CLoop *l){
  if(isEntry(block))return;
  unsigned int id = block->getBlockID();
  unsigned int size = block->size();//if no elemnt we will use the created single element 
  if(size!=0)size--;

  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->freeList_OUTB->find(pair<int,int>(id,size));
  set<int> freeOutSet = mapIter->second;
  set<int>  newFreeOutSet;



  mapIter = this->freeList_INB->find(pair<int,int>(id,size));
  set<int>  freeINSet = mapIter->second;

  for(clang::CFGBlock::const_succ_iterator i= block->succ_begin(); i!=block->succ_end(); ++i){
    if (!*i) continue;
	if(l->isHeader(*i))continue;
	if(!l->isLoopBlock(*i))continue;
    int succID=(*i)->getBlockID();

    set<int> succINSet;
    mapIter = this->freeList_INB->find(pair<int,int>(succID,0));
    succINSet= mapIter->second;
    for(set<int>::iterator iter= succINSet.begin();iter!= succINSet.end();iter++){

      newFreeOutSet.insert(*iter);
    }


  }
  mapIter = this->freeList_OUTB->find(pair<int,int>(id,size));

  this->freeList_OUTB->erase(mapIter);

  pair<int,int> idPair(id,size);

  pair<pair<int,int>, set<int> > elePair(idPair,newFreeOutSet);
  this->freeList_OUTB->insert(elePair);

  if(block->size()==0){
    if(newFreeOutSet!=freeINSet){
	  if(!l->isHeader(block)){
		  
		for(clang::CFGBlock::const_pred_iterator i=block->pred_begin(); i!=block->pred_end(); i++){
			if(!l->isLoopBlock(*i))continue;
			this->visit->insert(*i);
		}
	  }
      mapIter = this->freeList_INB->find(pair<int,int>(id,size));

      this->freeList_INB->erase(mapIter);

      pair<int,int> idPair(id,size);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeOutSet);
      this->freeList_INB->insert(elePair);
	  
    
	}

  }
  else{
    backwardProcessElementFree(block,l);

  }


}
void GraphAnalyzer::backwardProcessElementFree(CFGBlock * block,CLoop *l){
  unsigned int id = block->getBlockID();
  unsigned int size= block->size();
  size--;
  set<int> toAddElement;
  bool change=false;

  map<pair<int,int>,set<int> >::iterator mapIter;
  mapIter = this->freeList_OUTB->find(pair<int,int>(id,size));
  set<int>  freeOutSet = mapIter->second;


  mapIter = this->freeList_INB->find(pair<int,int>(id,size));
  set<int>  freeINSet = mapIter->second;
  set<int>  newFreeOUTSet;

  map<pair<int,int>,Element>::iterator elementIter;
  elementIter = this->ElementList->find(pair<int,int>(id,size));
  Element visitElement = elementIter->second;

  set<int> * freeSet = visitElement.getFreeSet();
  set<int> * useSet = visitElement.getUseSet();
  set<int> * killSet= visitElement.getID();
  for(set<int>::iterator i= freeSet->begin(); i!=freeSet->end();i++){
    freeOutSet.insert(*i);
  }

  if(freeINSet!=freeOutSet){
    change=true;
    mapIter = this->freeList_INB->find(pair<int,int>(id,size));

    this->freeList_INB->erase(mapIter);

    pair<int,int> idPair(id,size);

    pair<pair<int,int>, set<int> > elePair(idPair,freeOutSet);
    this->freeList_INB->insert(elePair);

  }
  // 1-size()-2 elements

  if(block->size()>1){
    for(int i=block->size()-2; i>=0;i--){
      mapIter = this->freeList_INB->find(pair<int,int>(id,i));
      freeINSet = mapIter->second;

      mapIter = this->freeList_INB->find(pair<int,int>(id,i+1));
      newFreeOUTSet  = mapIter->second;

      mapIter = this->freeList_OUTB->find(pair<int,int>(id,i));


      this->freeList_OUTB->erase(mapIter);
      pair<int,int> idPair(id,i);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeOUTSet);
      this->freeList_OUTB->insert(elePair);


      elementIter = this->ElementList->find(pair<int,int>(id,i));
      visitElement = elementIter->second;

      freeSet = visitElement.getFreeSet();
      useSet = visitElement.getUseSet();
      killSet = visitElement.getID();
      for(set<int>::iterator i= freeSet->begin(); i!=freeSet->end();i++){
        newFreeOUTSet.insert(*i);
      }


      if(newFreeOUTSet!= freeINSet){

        change = true;
        mapIter = this->freeList_INB->find(pair<int,int>(id,i));	
        this->freeList_INB->erase(mapIter);
        pair<int,int> idPair(id,i);
        pair<pair<int,int>, set<int> > elePair(idPair,newFreeOUTSet);
        this->freeList_INB->insert(elePair);
      }
    }
  }
  if(change&& (!l->isHeader(block))){
    for(clang::CFGBlock::const_pred_iterator i=block->pred_begin(); i!=block->pred_end(); i++){
	  if(!l->isLoopBlock(*i))continue;
      this->visit->insert(*i);
    }


  }
}
//should init visit first
void GraphAnalyzer::forwardFree(CLoop *l){
  CFGBlock* cb;

  set<CFGBlock*,compare_cfg>::iterator i=this->visit->begin();
  if(i!=this->visit->end()){
    cb = (*i);
    this->visit->erase(i);
    if(isEntry(cb)||isExit(cb)){
      forwardFree(l);
    }
    else{
      forwardProcessFree(cb,l);
      forwardFree(l);
    }
  }
}
void GraphAnalyzer::forwardProcessFree(CFGBlock *block,CLoop *l){

  unsigned int id = block->getBlockID();
  unsigned int index = 0;//if no elemnt we will use the created single element 


  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->freeList_INF->find(pair<int,int>(id,0));
  set<int>  freeInSet = mapIter->second;
  set<int>  newFreeInSet;


  mapIter = this->freeList_OUTF->find(pair<int,int>(id,0));
  set<int> freeOutSet = mapIter->second;
  set<int> newFreeOutSet;

  //caculate new malloc IN
  if(!l->isHeader(block)){
	  
  for(clang::CFGBlock::const_pred_iterator i= block->pred_begin(); i!=block->pred_end(); ++i){
	if(!l->isLoopBlock(*i))continue;
    int predID=(*i)->getBlockID();

    int size= (*i)->size();
    if(size!=0)size--;

    set<int> predOutSet;
    mapIter =  this->freeList_OUTF->find(pair<int,int>(predID,size));
    predOutSet= mapIter->second;
    for(set<int>::iterator iter= predOutSet.begin();iter!= predOutSet.end();iter++){

      newFreeInSet.insert(*iter);
    }

  }
  }
  mapIter = this->freeList_INF->find(pair<int,int>(id,0));

  this->freeList_INF->erase(mapIter);

  pair<int,int> idPair(id,0);

  pair<pair<int,int>, set<int> > elePair(idPair,newFreeInSet);
  this->freeList_INF->insert(elePair);
  if(block->size()==0){
    newFreeOutSet = newFreeInSet;
    if(newFreeOutSet != freeOutSet ){

      for(clang::CFGBlock::const_succ_iterator i=block->succ_begin(); i!=block->succ_end(); i++){
        if (!*i) continue;
		if(l->isHeader(*i))continue;
		if(!l->isLoopBlock(*i))continue;
        this->visit->insert(*i);
      }


      mapIter = this->freeList_OUTF->find(pair<int,int>(id,0));
      this->freeList_OUTF->erase(mapIter);

      pair<int,int> idPair(id,0);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeOutSet);
      this->freeList_OUTF->insert(elePair);

    }


  }
  else{
    processElementFree(block,l);
  }




}
void GraphAnalyzer::processElementFree(CFGBlock *block, CLoop *l){

  unsigned int id = block->getBlockID();
  unsigned int size= block->size();

  bool change=false;

  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->freeList_INF->find(pair<int,int>(id,0));
  set<int> freeInSet = mapIter->second;


  mapIter = this->freeList_OUTF->find(pair<int,int>(id,0));
  set<int> freeOutSet = mapIter->second;
  set<int> newFreeOutSet;

  map<pair<int,int>,Element>::iterator elementIter;
  elementIter = this->ElementList->find(pair<int,int>(id,0));
  Element visitElement = elementIter->second;

  set<int> * freeSet = visitElement.getFreeSet();


  for(set<int>::iterator i= freeSet->begin(); i!=freeSet->end();i++){
    freeInSet.insert(*i);
  }

  if(freeInSet!=freeOutSet){
    change=true;
    mapIter = this->freeList_OUTF->find(pair<int,int>(id,0));
    this->freeList_OUTF->erase(mapIter);

    pair<int,int> idPair(id,0);

    pair<pair<int,int>, set<int> > elePair(idPair,freeInSet);
    this->freeList_OUTF->insert(elePair);


  }

  if(size>1){
    for(int i=1;i<size;i++){
      mapIter = this->freeList_OUTF->find(pair<int,int>(id,i));
      freeOutSet = mapIter->second;

      mapIter = this->freeList_OUTF->find(pair<int,int>(id,i-1));
      newFreeOutSet  = mapIter->second;

      mapIter = this->freeList_INF->find(pair<int,int>(id,i));


      this->freeList_INF->erase(mapIter);
      pair<int,int> idPair(id,i);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeOutSet);
      this->freeList_INF->insert(elePair);
      freeInSet = newFreeOutSet;

      elementIter = this->ElementList->find(pair<int,int>(id,i));
      visitElement = elementIter->second;

      freeSet = visitElement.getFreeSet();			 

      for(set<int>::iterator i= freeSet->begin(); i!=freeSet->end();i++){
        freeInSet.insert(*i);
      }


      if(freeInSet!=freeOutSet){
        change=true;
        mapIter = this->freeList_OUTF->find(pair<int,int>(id,i));
        this->freeList_OUTF->erase(mapIter);

        pair<int,int> idPair(id,i);

        pair<pair<int,int>, set<int> > elePair(idPair,freeInSet);
        this->freeList_OUTF->insert(elePair);


      }

    }

  }

  if(change){
    for(clang::CFGBlock::const_succ_iterator i=block->succ_begin(); i!=block->succ_end(); i++){
      if (!*i) continue;
	  if(l->isHeader(*i))continue;
	  if(!l->isLoopBlock(*i))continue;
      this->visit->insert(*i);
    }


  }


}
void GraphAnalyzer::forward(CLoop *l){
  CFGBlock* cb;

  set<CFGBlock*,compare_cfg>::iterator i=visit->begin();

  if(i!=this->visit->end()){
    cb = (*i);
    this->visit->erase(i);
    if(isEntry(cb)||isExit(cb)){
      forward(l);
    }
    else{
      forwardProcess(cb,l);
      forward(l);
    }

  }

}
void GraphAnalyzer::forwardProcess(CFGBlock *block,CLoop * l){
  if(isExit(block))return;


  unsigned int id = block->getBlockID();
  unsigned int index = 0;//if no elemnt we will use the created single element 


  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->IN->find(pair<int,int>(id,0));
  set<int>  mallocInSet = mapIter->second;
  set<int>  newMallocInSet;


  mapIter = this->OUT->find(pair<int,int>(id,0));
  set<int> mallocOutSet = mapIter->second;
  set<int> newMallocOutSet;

  //caculate new malloc IN
  if(!l->isHeader(block)){
	  
	for(clang::CFGBlock::const_pred_iterator i= block->pred_begin(); i!=block->pred_end(); ++i){
		if(!l->isLoopBlock(*i))continue;
		int predID=(*i)->getBlockID();

		int size= (*i)->size();
		if(size!=0)size--;

		set<int> predOutSet;
		mapIter =  this->OUT->find(pair<int,int>(predID,size));
		predOutSet= mapIter->second;
		for(set<int>::iterator iter= predOutSet.begin();iter!= predOutSet.end();iter++){

			newMallocInSet.insert(*iter);
		}

	}
  }
  mapIter = this->IN->find(pair<int,int>(id,0));

  this->IN->erase(mapIter);

  pair<int,int> idPair(id,0);

  pair<pair<int,int>, set<int> > elePair(idPair,newMallocInSet);
  this->IN->insert(elePair);
  if(block->size()==0){
    newMallocOutSet = newMallocInSet;
    if(newMallocOutSet != mallocOutSet ){

      for(clang::CFGBlock::const_succ_iterator i=block->succ_begin(); i!=block->succ_end(); i++){
        if (!*i) continue;
		if(l->isHeader(*i))continue;
		if(!(l->isLoopBlock(*i)))continue;
        this->visit->insert(*i);
      }

      mapIter = this->OUT->find(pair<int,int>(id,0));
      this->OUT->erase(mapIter);

      pair<int,int> idPair(id,0);

      pair<pair<int,int>, set<int> > elePair(idPair,newMallocOutSet);
      this->OUT->insert(elePair);

    }


  }
  else{
    processElement(block,l);
  }



}
void GraphAnalyzer::processElement(CFGBlock * block, CLoop *l){
  unsigned int id = block->getBlockID();
  unsigned int size= block->size();

  bool change=false;

  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->IN->find(pair<int,int>(id,0));
  set<int> mallocInSet = mapIter->second;


  mapIter = this->OUT->find(pair<int,int>(id,0));
  set<int> mallocOutSet = mapIter->second;
  set<int> newMallocOutSet;

  map<pair<int,int>,Element>::iterator elementIter;
  elementIter = this->ElementList->find(pair<int,int>(id,0));
  Element visitElement = elementIter->second;

  set<int> * freeSet = visitElement.getFreeSet();
  set<int> * isMallocSet = visitElement.getID();

  for(set<int>::iterator i= isMallocSet->begin(); i!=isMallocSet->end();i++){
    mallocInSet.insert(*i);
  }

  if(mallocInSet!=mallocOutSet){
    change=true;
    mapIter = this->OUT->find(pair<int,int>(id,0));
    this->OUT->erase(mapIter);

    pair<int,int> idPair(id,0);

    pair<pair<int,int>, set<int> > elePair(idPair,mallocInSet);
    this->OUT->insert(elePair);


  }

  if(size>1){
    for(int i=1;i<size;i++){
      mapIter = this->OUT->find(pair<int,int>(id,i));
      mallocOutSet = mapIter->second;

      mapIter = this->OUT->find(pair<int,int>(id,i-1));
      newMallocOutSet  = mapIter->second;

      mapIter = this->IN->find(pair<int,int>(id,i));


      this->IN->erase(mapIter);
      pair<int,int> idPair(id,i);

      pair<pair<int,int>, set<int> > elePair(idPair,newMallocOutSet);
      this->IN->insert(elePair);
      mallocInSet = newMallocOutSet;

      elementIter = this->ElementList->find(pair<int,int>(id,i));
      visitElement = elementIter->second;

      freeSet = visitElement.getFreeSet();			 
      set<int> * isMallocSet = visitElement.getID();
      for(set<int>::iterator i= isMallocSet->begin(); i!=isMallocSet->end();i++){
        mallocInSet.insert(*i);
      }


      if(mallocInSet!=mallocOutSet){
        change=true;
        mapIter = this->OUT->find(pair<int,int>(id,i));
        this->OUT->erase(mapIter);

        pair<int,int> idPair(id,i);

        pair<pair<int,int>, set<int> > elePair(idPair,mallocInSet);
        this->OUT->insert(elePair);


      }

    }

  }

  if(change){
    for(clang::CFGBlock::const_succ_iterator i=block->succ_begin(); i!=block->succ_end(); i++){
      if (!*i) continue;
	  if(l->isHeader(*i))continue;
	  if(!l->isLoopBlock(*i))continue;
      this->visit->insert(*i);
    }
  }
}
void GraphAnalyzer::initLoopVisit(CLoop *l){
	this->visit->clear();
	for(CLoop::block_iterator iter= l->block_begin();iter!= l->block_end();iter++){
		this->visit->insert(*iter);
	}

}

void GraphAnalyzer::initUsedMap(CLoop *l){
  CFGBlock * block;
  unsigned int block_id;
  unsigned int size;
  int index;
  set<int> mallocSet;
  set<int> freeUseSet;
  map<pair<int,int>, set<int> >::iterator mapIter;
  for(CLoop::block_iterator i= l->block_begin(); i!= l->block_end();i++){


    block = *i;

    block_id = block->getBlockID();
    size = block->size();



    for(clang::CFGBlock::iterator iter = block->begin(); iter!=block->end();iter++)
    {	


      index = iter- block->begin();
      pair<int,int> idPair(block_id,index);
      mapIter = this->IN->find(idPair);
      if(mapIter!= this->IN->end())
        this->IN->erase(mapIter);

      mapIter = this->OUT->find(idPair);
      if(mapIter!= this->OUT->end())
        this->OUT->erase(mapIter);

      mapIter = this->freeList_INB->find(idPair);
      if(mapIter!= this->freeList_INB->end())
        this->freeList_INB->erase(mapIter);

      mapIter = this->freeList_OUTB->find(idPair);
      if(mapIter!= this->freeList_OUTB->end())
        this->freeList_OUTB->erase(mapIter);

      mapIter = this->useList_INB->find(idPair);
      if(mapIter!= this->useList_INB->end())
        this->useList_INB->erase(mapIter);

      mapIter = this->useList_OUTB->find(idPair);
      if(mapIter!= this->useList_OUTB->end())
        this->useList_OUTB->erase(mapIter);

      mapIter = this->freeList_INF->find(idPair);
      if(mapIter!= this->freeList_INF->end())
        this->freeList_INF->erase(mapIter);

      mapIter = this->freeList_OUTF->find(idPair);
      if(mapIter!= this->freeList_OUTF->end())
        this->freeList_OUTF->erase(mapIter);

      pair<pair<int,int>, set<int> > elePair(idPair,mallocSet);

      this->IN->insert(elePair);
      this->OUT->insert(elePair);
      this->freeList_INB->insert(elePair);
      this->freeList_OUTB->insert(elePair);

      pair<pair<int,int>,set<int> > elePair0(idPair,freeUseSet);
      this->useList_OUTB->insert(elePair0);
      this->useList_INB->insert(elePair0);
      this->freeList_INF->insert(elePair0);
      this->freeList_OUTF->insert(elePair0);

    }
    if(size==0){
      pair<int,int> idPair(block_id,0);
      mapIter = this->IN->find(idPair);
      if(mapIter!= this->IN->end())
        this->IN->erase(mapIter);

      mapIter = this->OUT->find(idPair);
      if(mapIter!= this->OUT->end())
        this->OUT->erase(mapIter);

      mapIter = this->freeList_INB->find(idPair);
      if(mapIter!= this->freeList_INB->end())
        this->freeList_INB->erase(mapIter);

      mapIter = this->freeList_OUTB->find(idPair);
      if(mapIter!= this->freeList_OUTB->end())
        this->freeList_OUTB->erase(mapIter);

      mapIter = this->useList_INB->find(idPair);
      if(mapIter!= this->useList_INB->end())
        this->useList_INB->erase(mapIter);

      mapIter = this->useList_OUTB->find(idPair);
      if(mapIter!= this->useList_OUTB->end())
        this->useList_OUTB->erase(mapIter);

      mapIter = this->freeList_INF->find(idPair);
      if(mapIter!= this->freeList_INF->end())
        this->freeList_INF->erase(mapIter);

      mapIter = this->freeList_OUTF->find(idPair);
      if(mapIter!= this->freeList_OUTF->end())
        this->freeList_OUTF->erase(mapIter);

      pair<pair<int,int>, set<int> > elePair(idPair,mallocSet);

      this->IN->insert(elePair);
      this->OUT->insert(elePair);
      // the none loop result is store in the mallocList_IN/Out so is replaced with IN/OUT
      // this->mallocList_INF->insert(elePair);
      // this->mallocList_OUTF->insert(elePair);
      this->freeList_INB->insert(elePair);
      this->freeList_OUTB->insert(elePair);

      pair<pair<int,int>,set<int> > elePair0(idPair,freeUseSet);
      this->useList_OUTB->insert(elePair0);
      this->useList_INB->insert(elePair0);
      this->freeList_INF->insert(elePair0);
      this->freeList_OUTF->insert(elePair0);
    }

  }


}
void GraphAnalyzer::printElementList(){
  llvm::errs() << "Printing ElementList...\n";
  for (map<pair<int,int >, Element >::iterator I = ElementList->begin(); I != ElementList->end(); ++I){
    NR->printToStmt(I->first);
    llvm::errs() << "\n";
    I->second.print();
  }
}

void GraphAnalyzer::analyze(){
  printLoopInfo();
  initVisit();
  forward();

  initVisit();
  forwardFree();

  initVisit();
  backward();

  initVisit();
  backwardFree();


  dataflow();
  //printUseResult();
}

void GraphAnalyzer::addBlock(CFGBlock *block_in,CFGBlock *block_out){
  CFGBlock *block_add;
  block_add = this->cfg->createBlock();
  //block_add->addSuccessor(block_out,cfg->getBumpVectorContext());
  block_add->addSuccessor(CFGBlock::AdjacentBlock(block_out, true), cfg->getBumpVectorContext());

  //block_in->addSuccessor(block_add,cfg->getBumpVectorContext());
  block_in->addSuccessor(CFGBlock::AdjacentBlock(block_add, true), cfg->getBumpVectorContext());

}

void GraphAnalyzer::addBlockToCFG(){
  vector<CFGBlock*> v1;
  vector<CFGBlock*> v2;

  CFGBlock * block;
  unsigned int succ_num;
  for(CFG::iterator i=cfg->begin();i!=cfg->end();i++){
    succ_num =0;
    block = (*i);
    for(CFGBlock::const_succ_iterator succIter=block->succ_begin();succIter!=block->succ_end();succIter++){
      if (!*succIter) continue;
      succ_num++;

    }
    for(CFGBlock::const_succ_iterator succIter1 = block->succ_begin(); succIter1 != block->succ_end(); succIter1++){
      if (!*succIter1) continue;
      CFGBlock *block_succ;
      block_succ = (*succIter1);
      if (!block_succ) continue;
      int pred_num =0;
      for(CFGBlock::const_pred_iterator predIter=block_succ->pred_begin();predIter!=block_succ->pred_end();predIter++){
        pred_num++;

      }
      if((succ_num > 1)&&(pred_num > 1)){
        v1.push_back(block);
        v2.push_back(block_succ);

      }



    }



  }
  unsigned int size = v1.size();
  for(int i=0;i<size;i++){
    addBlock(v1[i],v2[i]);
  }

}

void GraphAnalyzer::printSet(raw_ostream &os, set<int> &set0){
  os<<"{ ";
  for(set<int>::iterator i=set0.begin();i!=set0.end();i++){
    os<<(*i)<<" ";

  }
  os<<"} ";
}

void GraphAnalyzer::printResult(){
  llvm::errs() << "=======\nPrinting GraphAnalyzer result for " << FD->getName() << "..\n";
  for(clang::CFG::iterator i= cfg->begin(); i!=cfg->end(); ++i){
    CFGBlock *block = *i;
    int block_id = block->getBlockID();
    int size = block->size();
    if (block == &cfg->getExit()){
      llvm::errs() << "Exit.\n";
      pair<int,int> idPair(block_id, 0);
      map<pair<int,int>, set<int> >::iterator mapIter1;
      mapIter1 = this->mallocList_INF->find(idPair);
      if(mapIter1 != this->mallocList_INF->end()){
        set<int> in_set = mapIter1->second;
        llvm::errs() << "IN: " ;
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";
      }
      mapIter1 = this->mallocList_OUTF->find(idPair);
      if(mapIter1 != this->mallocList_OUTF->end()){
        set<int> in_set = mapIter1->second;
        llvm::errs() << "OUT: " ;
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";
      }
    }
    for(clang::CFGBlock::iterator iter = block->begin(); iter!=block->end();iter++)
    {	
      int index = iter- block->begin();
      pair<int,int> idPair(block_id, index);
      NR->printToStmt(pair<int,int>(block_id, index));
      map<pair<int,int>, set<int> >::iterator mapIter1;
      mapIter1 = this->mallocList_INF->find(idPair);
      if(mapIter1 != this->mallocList_INF->end()){
        set<int> in_set = mapIter1->second;

        llvm::errs() << "IN: ";
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }


      mapIter1 = this->mallocList_OUTF->find(idPair);
      if(mapIter1 != this->mallocList_OUTF->end()){
        set<int> in_set = mapIter1->second;

        llvm::errs() << "OUT: ";
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }
    }
  }
}
void GraphAnalyzer::printUseResult(){
  llvm::errs() << "=======\nPrinting GraphAnalyzer use result for " << FD->getName() << "..\n";
  for(clang::CFG::iterator i= cfg->begin(); i!=cfg->end(); ++i){
    CFGBlock *block = *i;
    int block_id = block->getBlockID();
    int size = block->size();
    if (block == &cfg->getExit()){
      llvm::errs() << "Exit.\n";
      pair<int,int> idPair(block_id, 0);
      map<pair<int,int>, set<int> >::iterator mapIter1;
      mapIter1 = this->useList_INB->find(idPair);
      if(mapIter1 != this->useList_INB->end()){
        set<int> in_set = mapIter1->second;
        llvm::errs() << "IN: " ;
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";
      }
      mapIter1 = this->useList_OUTB->find(idPair);
      if(mapIter1 != this->useList_OUTB->end()){
        set<int> in_set = mapIter1->second;
        llvm::errs() << "OUT: " ;
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";
      }
    }
    for(clang::CFGBlock::iterator iter = block->begin(); iter!=block->end();iter++)
    {	
      int index = iter- block->begin();
      pair<int,int> idPair(block_id, index);
      NR->printToStmt(pair<int,int>(block_id, index));
      map<pair<int,int>, set<int> >::iterator mapIter1;
      mapIter1 = this->useList_INB->find(idPair);
      if(mapIter1 != this->useList_INB->end()){
        set<int> in_set = mapIter1->second;

        llvm::errs() << "IN: ";
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }


      mapIter1 = this->useList_OUTB->find(idPair);
      if(mapIter1 != this->useList_OUTB->end()){
        set<int> in_set = mapIter1->second;

        llvm::errs() << "OUT: ";
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }
    }
  }
}
void GraphAnalyzer::printMap(){
  llvm::errs()<<"print Map------------------------------------------\n";

  CFGBlock * block;
  unsigned int block_id;
  unsigned int size;
  int index;
  set<int> mallocSet;
  set<int> freeUseSet;
  set<int> freeSet;
  map<pair<int,int>,set<int> >::iterator mapIter1;

  for(clang::CFG::iterator i= cfg->begin(); i!=cfg->end(); ++i){

    llvm::errs()<<(*i)->getBlockID()<<" block------------------------------------------\n";

    block = *i;


    block_id = block->getBlockID();
    size = block->size();



    for(clang::CFGBlock::iterator iter = block->begin(); iter!=block->end();iter++)
    {	


      index = iter- block->begin();
      pair<int,int> idPair(block_id,index);
      llvm::errs()<<"< "<<block_id<<","<<index<<" >\n";
      NR->printToStmt(idPair);
      llvm::errs() << "\n";
      mapIter1 = this->mallocList_INF->find(idPair);

      if(mapIter1 != this->mallocList_INF->end()){
        llvm::errs()<<"malloc inSet:";
        set<int> in_set = mapIter1->second;

        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }

      mapIter1 = this->useList_INB->find(idPair);
      if(mapIter1 != this->useList_INB->end()){
        llvm::errs()<<"use inSet:";
        set<int> in_set = mapIter1->second;

        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";


      }else{

        llvm::errs()<<"not found\n";
      }

      mapIter1 = this->freeList_INF->find(idPair);

      if(mapIter1 != this->freeList_INF->end()){
        llvm::errs()<<"free f:";
        set<int> in_set = mapIter1->second;

        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }


      mapIter1 = this->freeList_INB->find(idPair);

      if(mapIter1 != this->freeList_INB->end()){
        llvm::errs()<<"free B:";
        set<int> in_set = mapIter1->second;
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }



      //OUT
      //
      mapIter1 = this->mallocList_OUTF->find(idPair);
      if(mapIter1 != this->mallocList_OUTF->end()){
        llvm::errs()<<"malloc outSet:";
        set<int> out_set = mapIter1->second;
        printSet(llvm::errs(),out_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }

      mapIter1 = this->useList_OUTB->find(idPair);
      if(mapIter1 != this->useList_OUTB->end()){
        llvm::errs()<<"use outSet:";
        set<int> out_set = mapIter1->second;
        printSet(llvm::errs(),out_set);
        llvm::errs()<<"\n";
      }else{

        llvm::errs()<<"not found\n";
      }

      mapIter1 = this->freeList_OUTF->find(idPair);
      if(mapIter1 != this->freeList_OUTF->end()){
        llvm::errs()<<"free outSet f:";
        set<int> out_set = mapIter1->second;
        printSet(llvm::errs(),out_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }

      mapIter1 = this->freeList_OUTB->find(idPair);
      if(mapIter1 != this->freeList_OUTB->end()){
        llvm::errs()<<"free outSet b:";
        set<int> out_set = mapIter1->second;
        printSet(llvm::errs(),out_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }


    }
    if(size==0){
      index = 0;
      pair<int,int> idPair(block_id,0);
      llvm::errs()<<"< "<<block_id<<","<<index<<" >\n";
      mapIter1 = this->mallocList_INF->find(idPair);

      if(mapIter1 != this->mallocList_INF->end()){
        llvm::errs()<<"malloc inSet:";
        set<int> in_set = mapIter1->second;

        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }

      mapIter1 = this->useList_INB->find(idPair);
      if(mapIter1 != this->useList_INB->end()){
        llvm::errs()<<"use inSet:";
        set<int> in_set = mapIter1->second;
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";
      }else{

        llvm::errs()<<"not found\n";
      }

      mapIter1 = this->freeList_INF->find(idPair);

      if(mapIter1 != this->freeList_INF->end()){
        llvm::errs()<<"free f:";
        set<int> in_set = mapIter1->second;
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }
      mapIter1 = this->freeList_INB->find(idPair);

      if(mapIter1 != this->freeList_INB->end()){
        llvm::errs()<<"free B:";
        set<int> in_set = mapIter1->second;
        printSet(llvm::errs(),in_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }


      //OUT

      mapIter1 = this->mallocList_OUTF->find(idPair);
      if(mapIter1 != this->mallocList_OUTF->end()){
        llvm::errs()<<"maloc outSet:";
        set<int> out_set = mapIter1->second;
        printSet(llvm::errs(),out_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }
      mapIter1 = this->useList_OUTB->find(idPair);
      if(mapIter1 != this->useList_OUTB->end()){

        llvm::errs()<<"use outSet:";
        set<int> out_set = mapIter1->second;
        printSet(llvm::errs(),out_set);
        llvm::errs()<<"\n";
      }else{

        llvm::errs()<<"not found\n";
      }

      mapIter1 = this->freeList_OUTF->find(idPair);
      if(mapIter1 != this->freeList_OUTF->end()){
        llvm::errs()<<"free outSet f:";
        set<int> out_set = mapIter1->second;
        printSet(llvm::errs(),out_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }

      mapIter1 = this->freeList_OUTB->find(idPair);
      if(mapIter1 != this->freeList_OUTB->end()){
        llvm::errs()<<"free outSet b:";
        set<int> out_set = mapIter1->second;
        printSet(llvm::errs(),out_set);
        llvm::errs()<<"\n";

      }else{

        llvm::errs()<<"not found\n";
      }



    }

  }


}
void GraphAnalyzer::dataflow(){

  CFGBlock * block;
  unsigned int block_id;
  unsigned int size;
  int index;
  set<int> mallocINSet;
  set<int> mallocOUTSet;
  set<int> INSet;
  set<int> OUTSet;

  set<int> UseINSet;
  set<int> UseOUTSet;

  set<int> freeINSet;
  set<int> freeOUTSet;

  set<int> freeINSetB;
  set<int> freeOUTSetB;


  map<pair<int,int>,set<int> >::iterator mapIter1;
  for(clang::CFG::iterator i= cfg->begin(); i!=cfg->end(); ++i){

    block = *i;
    if(isEntry(block))continue;

    block_id = block->getBlockID();
    size = block->size();
    for(clang::CFGBlock::iterator iter = block->begin(); iter!=block->end();iter++)
    {	


      index = iter- block->begin();
      pair<int,int> idPair(block_id,index);

      mapIter1 = this->useList_INB->find(idPair);
      UseINSet = mapIter1->second;

      mapIter1 = this->useList_OUTB->find(idPair);
      UseOUTSet = mapIter1->second;

      mapIter1 = this->mallocList_INF->find(idPair);
      mallocINSet = mapIter1->second;

      mapIter1= this->mallocList_OUTF->find(idPair);
      mallocOUTSet = mapIter1->second;

      mapIter1 = this->freeList_INF->find(idPair);
      freeINSet = mapIter1->second;

      mapIter1= this->freeList_OUTF->find(idPair);
      freeOUTSet = mapIter1->second;

      mapIter1 = this->freeList_INB->find(idPair);
      freeINSetB = mapIter1->second;

      mapIter1= this->freeList_OUTB->find(idPair);
      freeOUTSetB = mapIter1->second;

      INSet = mallocINSet;
      OUTSet = mallocOUTSet;
      for(set<int>::iterator i =freeINSetB.begin(); i !=freeINSetB.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }
      for(set<int>::iterator i =freeINSet.begin(); i !=freeINSet.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }
      for(set<int>::iterator i =UseINSet.begin(); i !=UseINSet.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }

      //out
      for(set<int>::iterator i =freeOUTSetB.begin(); i !=freeOUTSetB.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      for(set<int>::iterator i =freeOUTSet.begin(); i !=freeOUTSet.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      for(set<int>::iterator i =UseOUTSet.begin(); i !=UseOUTSet.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      pair<pair<int,int>, set<int> >  elePair(idPair,INSet);
      pair<pair<int,int>, set<int> > elePair0(idPair,OUTSet);


      mapIter1 = this->mallocList_INF->find(idPair);
      if(mapIter1!=this->mallocList_INF->end()){

        this->mallocList_INF->erase(mapIter1);

      }else{
      }

      this->mallocList_INF->insert(elePair);

      mapIter1 = this->mallocList_OUTF->find(idPair);
      if(mapIter1!=this->mallocList_OUTF->end()){

        this->mallocList_OUTF->erase(mapIter1);

      }else{
      }

      this->mallocList_OUTF->insert(elePair0);

    }
    if(size==0){
      pair<int,int> idPair(block_id,0);
      mapIter1 = this->useList_INB->find(idPair);
      UseINSet = mapIter1->second;

      mapIter1 = this->useList_OUTB->find(idPair);
      UseOUTSet = mapIter1->second;

      mapIter1 = this->mallocList_INF->find(idPair);
      mallocINSet = mapIter1->second;

      mapIter1= this->mallocList_OUTF->find(idPair);
      mallocOUTSet = mapIter1->second;

      mapIter1 = this->freeList_INF->find(idPair);
      freeINSet = mapIter1->second;

      mapIter1= this->freeList_OUTF->find(idPair);
      freeOUTSet = mapIter1->second;

      mapIter1 = this->freeList_INB->find(idPair);
      freeINSetB = mapIter1->second;

      mapIter1= this->freeList_OUTB->find(idPair);
      freeOUTSetB = mapIter1->second;

      INSet = mallocINSet;
      OUTSet = mallocOUTSet;
      for(set<int>::iterator i =freeINSetB.begin(); i !=freeINSetB.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }
      for(set<int>::iterator i =freeINSet.begin(); i !=freeINSet.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }
      for(set<int>::iterator i =UseINSet.begin(); i !=UseINSet.end(); i++){
        if(mallocINSet.find(*i) != mallocINSet.end()){  
          INSet.erase(*i);
        }
      }

      //out
      for(set<int>::iterator i =freeOUTSetB.begin(); i !=freeOUTSetB.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      for(set<int>::iterator i =freeOUTSet.begin(); i !=freeOUTSet.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      for(set<int>::iterator i =UseOUTSet.begin(); i !=UseOUTSet.end(); i++){
        if(mallocOUTSet.find(*i) != mallocOUTSet.end()){  
          OUTSet.erase(*i);
        }
      }
      pair<pair<int,int>, set<int> >  elePair(idPair,INSet);
      pair<pair<int,int>, set<int> > elePair0(idPair,OUTSet);
      mapIter1 = this->mallocList_INF->find(idPair);
      if(mapIter1!=this->mallocList_INF->end()){

        this->mallocList_INF->erase(mapIter1);

      }else{
      }

      this->mallocList_INF->insert(elePair);

      mapIter1 = this->mallocList_OUTF->find(idPair);
      if(mapIter1!=this->mallocList_OUTF->end()){

        this->mallocList_OUTF->erase(mapIter1);

      }else{
      }

      this->mallocList_OUTF->insert(elePair0);
    }

  }
}
void GraphAnalyzer::backwardFree(){
  CFGBlock* cb;

  set<CFGBlock*,compare_cfg>::reverse_iterator i=visit->rbegin();
  if(i!=this->visit->rend()){
    cb = (*i);
    this->visit->erase(cb);
    if(isEntry(cb)||isExit(cb)){
      backwardFree();
    }
    else{
      backwardProcessFree(cb);
      backwardFree();
    }

  }
}

void GraphAnalyzer::backwardProcessFree(CFGBlock * block){
  if(isEntry(block))return;
  unsigned int id = block->getBlockID();
  unsigned int size = block->size();//if no elemnt we will use the created single element 
  if(size!=0)size--;

  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->freeList_OUTB->find(pair<int,int>(id,size));
  set<int> freeOutSet = mapIter->second;
  set<int>  newFreeOutSet;



  mapIter = this->freeList_INB->find(pair<int,int>(id,size));
  set<int>  freeINSet = mapIter->second;

  for(clang::CFGBlock::const_succ_iterator i= block->succ_begin(); i!=block->succ_end(); ++i){
    if (!*i) continue;
    int succID=(*i)->getBlockID();

    set<int> succINSet;
    mapIter = this->freeList_INB->find(pair<int,int>(succID,0));
    succINSet= mapIter->second;
    for(set<int>::iterator iter= succINSet.begin();iter!= succINSet.end();iter++){

      newFreeOutSet.insert(*iter);
    }


  }
  mapIter = this->freeList_OUTB->find(pair<int,int>(id,size));

  this->freeList_OUTB->erase(mapIter);

  pair<int,int> idPair(id,size);

  pair<pair<int,int>, set<int> > elePair(idPair,newFreeOutSet);
  this->freeList_OUTB->insert(elePair);

  if(block->size()==0){
    if(newFreeOutSet!=freeINSet){
      for(clang::CFGBlock::const_pred_iterator i=block->pred_begin(); i!=block->pred_end(); i++){
        this->visit->insert(*i);
      }
      mapIter = this->freeList_INB->find(pair<int,int>(id,size));

      this->freeList_INB->erase(mapIter);

      pair<int,int> idPair(id,size);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeOutSet);
      this->freeList_INB->insert(elePair);

    }

  }
  else{
    backwardProcessElementFree(block);

  }


}
void GraphAnalyzer::backwardProcessElementFree(CFGBlock * block){
  unsigned int id = block->getBlockID();
  unsigned int size= block->size();
  size--;
  set<int> toAddElement;
  bool change=false;

  map<pair<int,int>,set<int> >::iterator mapIter;
  mapIter = this->freeList_OUTB->find(pair<int,int>(id,size));
  set<int>  freeOutSet = mapIter->second;


  mapIter = this->freeList_INB->find(pair<int,int>(id,size));
  set<int>  freeINSet = mapIter->second;
  set<int>  newFreeOUTSet;

  map<pair<int,int>,Element>::iterator elementIter;
  elementIter = this->ElementList->find(pair<int,int>(id,size));
  Element visitElement = elementIter->second;

  set<int> * freeSet = visitElement.getFreeSet();
  set<int> * useSet = visitElement.getUseSet();
  set<int> * killSet= visitElement.getID();
  for(set<int>::iterator i= freeSet->begin(); i!=freeSet->end();i++){
    freeOutSet.insert(*i);
  }

  if(freeINSet!=freeOutSet){
    change=true;
    mapIter = this->freeList_INB->find(pair<int,int>(id,size));

    this->freeList_INB->erase(mapIter);

    pair<int,int> idPair(id,size);

    pair<pair<int,int>, set<int> > elePair(idPair,freeOutSet);
    this->freeList_INB->insert(elePair);

  }
  // 1-size()-2 elements

  if(block->size()>1){
    for(int i=block->size()-2; i>=0;i--){
      mapIter = this->freeList_INB->find(pair<int,int>(id,i));
      freeINSet = mapIter->second;

      mapIter = this->freeList_INB->find(pair<int,int>(id,i+1));
      newFreeOUTSet  = mapIter->second;

      mapIter = this->freeList_OUTB->find(pair<int,int>(id,i));


      this->freeList_OUTB->erase(mapIter);
      pair<int,int> idPair(id,i);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeOUTSet);
      this->freeList_OUTB->insert(elePair);


      elementIter = this->ElementList->find(pair<int,int>(id,i));
      visitElement = elementIter->second;

      freeSet = visitElement.getFreeSet();
      useSet = visitElement.getUseSet();
      killSet = visitElement.getID();
      for(set<int>::iterator i= freeSet->begin(); i!=freeSet->end();i++){
        newFreeOUTSet.insert(*i);
      }


      if(newFreeOUTSet!= freeINSet){

        change = true;
        mapIter = this->freeList_INB->find(pair<int,int>(id,i));	
        this->freeList_INB->erase(mapIter);
        pair<int,int> idPair(id,i);
        pair<pair<int,int>, set<int> > elePair(idPair,newFreeOUTSet);
        this->freeList_INB->insert(elePair);
      }
    }
  }
  if(change){
    for(clang::CFGBlock::const_pred_iterator i=block->pred_begin(); i!=block->pred_end(); i++){
      this->visit->insert(*i);
    }


  }


}
//   should init visit first
void GraphAnalyzer::backward(){
  CFGBlock* cb;

  set<CFGBlock*,compare_cfg>::reverse_iterator i=visit->rbegin();
  if(i!=this->visit->rend()){
    cb = (*i);
    this->visit->erase(cb);
    if(isEntry(cb)||isExit(cb)){
      backward();
    }
    else{
      backwardProcess(cb);
      backward();
    }

  }

}
void GraphAnalyzer::backwardProcess(CFGBlock * block){
  if(isEntry(block))return;
  unsigned int id = block->getBlockID();
  unsigned int size = block->size();//if no elemnt we will use the created single element 
  if(size!=0)size--;

  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->useList_OUTB->find(pair<int,int>(id,size));
  set<int> freeUseOutSet = mapIter->second;
  set<int>  newFreeUseOutSet;



  mapIter = this->useList_INB->find(pair<int,int>(id,size));
  set<int>  freeUseINSet = mapIter->second;
  clang::LangOptions langOptions;

  for(clang::CFGBlock::const_succ_iterator i= block->succ_begin(); i!=block->succ_end(); ++i){
    if (!*i){
      continue;
    }
    int succID=(*i)->getBlockID();

    set<int> succINSet;
    mapIter = this->useList_INB->find(pair<int,int>(succID,0));
    succINSet= mapIter->second;
    for(set<int>::iterator iter= succINSet.begin();iter!= succINSet.end();iter++){

      newFreeUseOutSet.insert(*iter);
    }


  }
  mapIter = this->useList_OUTB->find(pair<int,int>(id,size));

  this->useList_OUTB->erase(mapIter);

  pair<int,int> idPair(id,size);

  pair<pair<int,int>, set<int> > elePair(idPair,newFreeUseOutSet);
  this->useList_OUTB->insert(elePair);

  if(block->size()==0){
    if(newFreeUseOutSet!=freeUseINSet){
      for(clang::CFGBlock::const_pred_iterator i=block->pred_begin(); i!=block->pred_end(); i++){
        this->visit->insert(*i);
      }
      mapIter = this->useList_INB->find(pair<int,int>(id,size));

      this->useList_INB->erase(mapIter);

      pair<int,int> idPair(id,size);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeUseOutSet);
      this->useList_INB->insert(elePair);

    }

  }
  else{
    backwardElementProF(block);

  }



}

void GraphAnalyzer::backwardElementProF(CFGBlock *block){
  unsigned int id = block->getBlockID();
  unsigned int size= block->size();
  size--;
  set<int> toAddElement;
  bool change=false;

  map<pair<int,int>,set<int> >::iterator mapIter;
  mapIter = this->useList_OUTB->find(pair<int,int>(id,size));
  set<int>  freeUseOutSet = mapIter->second;


  mapIter = this->useList_INB->find(pair<int,int>(id,size));
  set<int>  freeUseINSet = mapIter->second;
  set<int>  newFreeUseOUTSet;

  map<pair<int,int>,Element>::iterator elementIter;
  elementIter = this->ElementList->find(pair<int,int>(id,size));
  Element visitElement = elementIter->second;

  set<int> * freeSet = visitElement.getFreeSet();
  set<int> * useSet = visitElement.getUseSet();
  set<int> * killSet= visitElement.getID();

  for(set<int>::iterator i= useSet->begin(); i!=useSet->end();i++){
    freeUseOutSet.insert(*i);
  }
  for(set<int>::iterator i=killSet->begin(); i!=killSet->end();i++){
    freeUseOutSet.erase(*i);
  }
  if(freeUseINSet!=freeUseOutSet){
    change=true;
    mapIter = this->useList_INB->find(pair<int,int>(id,size));

    this->useList_INB->erase(mapIter);

    pair<int,int> idPair(id,size);

    pair<pair<int,int>, set<int> > elePair(idPair,freeUseOutSet);
    this->useList_INB->insert(elePair);

  }
  // 1-size()-2 elements

  if(block->size()>1){
    for(int i=block->size()-2; i>=0;i--){
      mapIter = this->useList_INB->find(pair<int,int>(id,i));
      freeUseINSet = mapIter->second;

      mapIter = this->useList_INB->find(pair<int,int>(id,i+1));
      newFreeUseOUTSet  = mapIter->second;

      mapIter = this->useList_OUTB->find(pair<int,int>(id,i));


      this->useList_OUTB->erase(mapIter);
      pair<int,int> idPair(id,i);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeUseOUTSet);
      this->useList_OUTB->insert(elePair);


      elementIter = this->ElementList->find(pair<int,int>(id,i));
      visitElement = elementIter->second;


      useSet = visitElement.getUseSet();
      killSet = visitElement.getID();

      for(set<int>::iterator i= useSet->begin(); i!=useSet->end();i++){
        newFreeUseOUTSet.insert(*i);
      }
      for(set<int>::iterator i= killSet->begin();i!= killSet->end(); i++){
        newFreeUseOUTSet.erase(*i);
      }

      if(newFreeUseOUTSet!= freeUseINSet){
        change = true;
        mapIter = this->useList_INB->find(pair<int,int>(id,i));

        this->useList_INB->erase(mapIter);

        pair<int,int> idPair(id,i);

        pair<pair<int,int>, set<int> > elePair(idPair,newFreeUseOUTSet);
        this->useList_INB->insert(elePair);


      }
    }
  }
  if(change){
    for(clang::CFGBlock::const_pred_iterator i=block->pred_begin(); i!=block->pred_end(); i++){
      this->visit->insert(*i);
    }


  }


}
void GraphAnalyzer::forwardFree(){
  CFGBlock* cb;

  set<CFGBlock*,compare_cfg>::iterator i=this->visit->begin();
  if(i!=this->visit->end()){
    cb = (*i);
    this->visit->erase(i);
    if(isEntry(cb)||isExit(cb)){
      forwardFree();
    }
    else{
      forwardProcessFree(cb);
      forwardFree();
    }

  }


}
void GraphAnalyzer::forwardProcessFree(CFGBlock *block){

  unsigned int id = block->getBlockID();
  unsigned int index = 0;//if no elemnt we will use the created single element 


  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->freeList_INF->find(pair<int,int>(id,0));
  set<int>  freeInSet = mapIter->second;
  set<int>  newFreeInSet;


  mapIter = this->freeList_OUTF->find(pair<int,int>(id,0));
  set<int> freeOutSet = mapIter->second;
  set<int> newFreeOutSet;

  //caculate new malloc IN
  for(clang::CFGBlock::const_pred_iterator i= block->pred_begin(); i!=block->pred_end(); ++i){
    int predID=(*i)->getBlockID();

    int size= (*i)->size();
    if(size!=0)size--;

    set<int> predOutSet;
    mapIter =  this->freeList_OUTF->find(pair<int,int>(predID,size));
    predOutSet= mapIter->second;
    for(set<int>::iterator iter= predOutSet.begin();iter!= predOutSet.end();iter++){

      newFreeInSet.insert(*iter);
    }


  }
  mapIter = this->freeList_INF->find(pair<int,int>(id,0));

  this->freeList_INF->erase(mapIter);

  pair<int,int> idPair(id,0);

  pair<pair<int,int>, set<int> > elePair(idPair,newFreeInSet);
  this->freeList_INF->insert(elePair);
  if(block->size()==0){
    newFreeOutSet = newFreeInSet;
    if(newFreeOutSet != freeOutSet ){

      for(clang::CFGBlock::const_succ_iterator i=block->succ_begin(); i!=block->succ_end(); i++){
        if (!*i) continue;
        this->visit->insert(*i);
      }


      mapIter = this->freeList_OUTF->find(pair<int,int>(id,0));
      this->freeList_OUTF->erase(mapIter);

      pair<int,int> idPair(id,0);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeOutSet);
      this->freeList_OUTF->insert(elePair);

    }


  }
  else{
    processElementFree(block);
  }




}
void GraphAnalyzer::processElementFree(CFGBlock *block){

  unsigned int id = block->getBlockID();
  unsigned int size= block->size();

  bool change=false;

  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->freeList_INF->find(pair<int,int>(id,0));
  set<int> freeInSet = mapIter->second;


  mapIter = this->freeList_OUTF->find(pair<int,int>(id,0));
  set<int> freeOutSet = mapIter->second;
  set<int> newFreeOutSet;

  map<pair<int,int>,Element>::iterator elementIter;
  elementIter = this->ElementList->find(pair<int,int>(id,0));
  Element visitElement = elementIter->second;

  set<int> * freeSet = visitElement.getFreeSet();


  for(set<int>::iterator i= freeSet->begin(); i!=freeSet->end();i++){
    freeInSet.insert(*i);
  }

  if(freeInSet!=freeOutSet){
    change=true;
    mapIter = this->freeList_OUTF->find(pair<int,int>(id,0));
    this->freeList_OUTF->erase(mapIter);

    pair<int,int> idPair(id,0);

    pair<pair<int,int>, set<int> > elePair(idPair,freeInSet);
    this->freeList_OUTF->insert(elePair);


  }

  if(size>1){
    for(int i=1;i<size;i++){
      mapIter = this->freeList_OUTF->find(pair<int,int>(id,i));
      freeOutSet = mapIter->second;

      mapIter = this->freeList_OUTF->find(pair<int,int>(id,i-1));
      newFreeOutSet  = mapIter->second;

      mapIter = this->freeList_INF->find(pair<int,int>(id,i));


      this->freeList_INF->erase(mapIter);
      pair<int,int> idPair(id,i);

      pair<pair<int,int>, set<int> > elePair(idPair,newFreeOutSet);
      this->freeList_INF->insert(elePair);
      freeInSet = newFreeOutSet;

      elementIter = this->ElementList->find(pair<int,int>(id,i));
      visitElement = elementIter->second;

      freeSet = visitElement.getFreeSet();			 

      for(set<int>::iterator i= freeSet->begin(); i!=freeSet->end();i++){
        freeInSet.insert(*i);
      }


      if(freeInSet!=freeOutSet){
        change=true;
        mapIter = this->freeList_OUTF->find(pair<int,int>(id,i));
        this->freeList_OUTF->erase(mapIter);

        pair<int,int> idPair(id,i);

        pair<pair<int,int>, set<int> > elePair(idPair,freeInSet);
        this->freeList_OUTF->insert(elePair);


      }

    }

  }

  if(change){
    for(clang::CFGBlock::const_succ_iterator i=block->succ_begin(); i!=block->succ_end(); i++){
      if (!*i) continue;
      this->visit->insert(*i);
    }


  }


}
void GraphAnalyzer::forward(){
  CFGBlock* cb;

  set<CFGBlock*,compare_cfg>::iterator i=visit->begin();
  if(i!=this->visit->end()){
    cb = (*i);
    this->visit->erase(i);
    if(isEntry(cb)||isExit(cb)){
      forward();
    }
    else{
      forwardProcess(cb);
      forward();
    }

  }


}

void GraphAnalyzer::forwardProcess(CFGBlock *block){
  if(isExit(block))return;


  unsigned int id = block->getBlockID();
  unsigned int index = 0;//if no elemnt we will use the created single element 


  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->mallocList_INF->find(pair<int,int>(id,0));
  set<int>  mallocInSet = mapIter->second;
  set<int>  newMallocInSet;


  mapIter = this->mallocList_OUTF->find(pair<int,int>(id,0));
  set<int> mallocOutSet = mapIter->second;
  set<int> newMallocOutSet;

  //caculate new malloc IN
  for(clang::CFGBlock::const_pred_iterator i= block->pred_begin(); i!=block->pred_end(); ++i){
    int predID=(*i)->getBlockID();

    int size= (*i)->size();
    if(size!=0)size--;

    set<int> predOutSet;
    mapIter =  this->mallocList_OUTF->find(pair<int,int>(predID,size));
    predOutSet= mapIter->second;
    for(set<int>::iterator iter= predOutSet.begin();iter!= predOutSet.end();iter++){

      newMallocInSet.insert(*iter);
    }


  }
  mapIter = this->mallocList_INF->find(pair<int,int>(id,0));

  this->mallocList_INF->erase(mapIter);

  pair<int,int> idPair(id,0);

  pair<pair<int,int>, set<int> > elePair(idPair,newMallocInSet);
  this->mallocList_INF->insert(elePair);
  if(block->size()==0){
    newMallocOutSet = newMallocInSet;
    if(newMallocOutSet != mallocOutSet ){

      for(clang::CFGBlock::const_succ_iterator i=block->succ_begin(); i!=block->succ_end(); i++){
        if (!*i) continue;
        this->visit->insert(*i);
      }

      //this->visit->insert(block);
      mapIter = this->mallocList_OUTF->find(pair<int,int>(id,0));
      this->mallocList_OUTF->erase(mapIter);

      pair<int,int> idPair(id,0);

      pair<pair<int,int>, set<int> > elePair(idPair,newMallocOutSet);
      this->mallocList_OUTF->insert(elePair);

    }


  }
  else{
    processElement(block);
  }



}

void GraphAnalyzer::processElement(CFGBlock * block){
  unsigned int id = block->getBlockID();
  unsigned int size= block->size();

  bool change=false;

  map<pair<int,int>,set<int > >::iterator mapIter;
  mapIter = this->mallocList_INF->find(pair<int,int>(id,0));
  set<int> mallocInSet = mapIter->second;


  mapIter = this->mallocList_OUTF->find(pair<int,int>(id,0));
  set<int> mallocOutSet = mapIter->second;
  set<int> newMallocOutSet;

  map<pair<int,int>,Element>::iterator elementIter;
  elementIter = this->ElementList->find(pair<int,int>(id,0));
  Element visitElement = elementIter->second;

  set<int> * freeSet = visitElement.getFreeSet();
  set<int> * isMallocSet = visitElement.getID();

  for(set<int>::iterator i= isMallocSet->begin(); i!=isMallocSet->end();i++){
    mallocInSet.insert(*i);
  }

  if(mallocInSet!=mallocOutSet){
    change=true;
    mapIter = this->mallocList_OUTF->find(pair<int,int>(id,0));
    this->mallocList_OUTF->erase(mapIter);

    pair<int,int> idPair(id,0);

    pair<pair<int,int>, set<int> > elePair(idPair,mallocInSet);
    this->mallocList_OUTF->insert(elePair);


  }

  if(size>1){
    for(int i=1;i<size;i++){
      mapIter = this->mallocList_OUTF->find(pair<int,int>(id,i));
      mallocOutSet = mapIter->second;

      mapIter = this->mallocList_OUTF->find(pair<int,int>(id,i-1));
      newMallocOutSet  = mapIter->second;

      mapIter = this->mallocList_INF->find(pair<int,int>(id,i));


      this->mallocList_INF->erase(mapIter);
      pair<int,int> idPair(id,i);

      pair<pair<int,int>, set<int> > elePair(idPair,newMallocOutSet);
      this->mallocList_INF->insert(elePair);
      mallocInSet = newMallocOutSet;

      elementIter = this->ElementList->find(pair<int,int>(id,i));
      visitElement = elementIter->second;

      freeSet = visitElement.getFreeSet();			 
      set<int> * isMallocSet = visitElement.getID();
      for(set<int>::iterator i= isMallocSet->begin(); i!=isMallocSet->end();i++){
        mallocInSet.insert(*i);
      }


      if(mallocInSet!=mallocOutSet){
        change=true;
        mapIter = this->mallocList_OUTF->find(pair<int,int>(id,i));
        this->mallocList_OUTF->erase(mapIter);

        pair<int,int> idPair(id,i);

        pair<pair<int,int>, set<int> > elePair(idPair,mallocInSet);
        this->mallocList_OUTF->insert(elePair);


      }

    }

  }

  if(change){
    for(clang::CFGBlock::const_succ_iterator i=block->succ_begin(); i!=block->succ_end(); i++){
      if (!*i) continue;
      this->visit->insert(*i);
    }


  }




}
bool GraphAnalyzer::isEntry(CFGBlock* block){
  CFGBlock entry = cfg->getEntry();
  unsigned int id = entry.getBlockID();
  return block->getBlockID() == id;

}
bool GraphAnalyzer::isExit(CFGBlock* block){
  CFGBlock exit = cfg->getExit();
  unsigned int id = exit.getBlockID();
  return block->getBlockID() == id;

}
void GraphAnalyzer::initMap(){

  CFGBlock * block;
  unsigned int block_id;
  unsigned int size;
  int index;
  set<int> mallocSet;
  set<int> freeUseSet;
  for(clang::CFG::iterator i= cfg->begin(); i!=cfg->end(); ++i){

    block = *i;

    block_id = block->getBlockID();
    size = block->size();



    for(clang::CFGBlock::iterator iter = block->begin(); iter!=block->end();iter++)
    {	


      index = iter- block->begin();
      pair<int,int> idPair(block_id,index);

      pair<pair<int,int>, set<int> > elePair(idPair,mallocSet);
      this->mallocList_INF->insert(elePair);
      this->mallocList_OUTF->insert(elePair);
      this->freeList_INB->insert(elePair);
      this->freeList_OUTB->insert(elePair);

      pair<pair<int,int>,set<int> > elePair0(idPair,freeUseSet);
      this->useList_OUTB->insert(elePair0);
      this->useList_INB->insert(elePair0);
      this->freeList_INF->insert(elePair0);
      this->freeList_OUTF->insert(elePair0);






    }
    if(size==0){
      pair<int,int> idPair(block_id,0);
      pair<pair<int,int>, set<int> > elePair(idPair,mallocSet);
      this->mallocList_INF->insert(elePair);
      this->mallocList_OUTF->insert(elePair);
      this->freeList_INB->insert(elePair);
      this->freeList_OUTB->insert(elePair);

      pair<pair<int,int>,set<int> > elePair0(idPair,freeUseSet);
      this->useList_OUTB->insert(elePair0);
      this->useList_INB->insert(elePair0);
      this->freeList_INF->insert(elePair0);
      this->freeList_OUTF->insert(elePair0);



    }

  }


}
void GraphAnalyzer::initElementList(){

  CFGBlock * block;
  unsigned int block_id;
  unsigned int size;
  int index;
  for(clang::CFG::iterator i= cfg->begin(); i!=cfg->end(); ++i){

    block = *i;

    block_id = block->getBlockID();
    size = block->size();



    for(clang::CFGBlock::iterator iter = block->begin(); iter!=block->end();iter++)
    {	
      Element  element(block,iter);
      index = iter- block->begin();

      pair<int,int> idPair(block_id,index);
      pair<pair<int,int>, Element> elePair(idPair,element);
      this->ElementList->insert(elePair);



    }

  }


}
void GraphAnalyzer::initVisit(){

  if(this->visit->empty()){
  }
  int nu = 0;

  for(clang::CFG::iterator i= cfg->begin(); i!=cfg->end(); ++i){

    CFGBlock * cb;
    cb=(*i);


    this->visit->insert(cb);
  }

  for(set<CFGBlock*,compare_cfg>::iterator i=visit->begin();i!=this->visit->end();i++){


  }

}

GraphAnalyzer::GraphAnalyzer(CFG *cfg1, ASTUnit *unit, NodeRecognizer *nr, CLoopInfo * cl ): NR(nr){

  this->cfg = cfg1;
  this->astUnit = unit;
  this->cli = cl;

  this->ElementList = new map<pair<int, int>, Element>;

  this->freeList_INB= new map<pair<int,int>, set<int> >;
  this->freeList_OUTB = new map<pair<int,int>, set<int> >;

  this->useList_OUTB = new map<pair<int, int>, set<int> >;
  this->useList_INB = new map<pair<int,int>, set<int> >;

  this->mallocList_INF = new map<pair<int,int>, set<int> >;
  this->mallocList_OUTF = new map<pair<int,int>, set<int> >;
  this->freeList_INF = new map<pair<int,int>, set<int> > ;
  this->freeList_OUTF = new map<pair<int,int>, set<int> >;

  this->IN= new map<pair<int,int>, set<int> >;
  this->OUT = new map<pair<int,int>, set<int> >;






  this->visit = new set<CFGBlock*,compare_cfg>;




  initElementList();
  initMap();

}


GraphAnalyzer::~GraphAnalyzer(){



  delete this->ElementList;

  delete this->freeList_INB;
  delete this->freeList_OUTB;

  delete this->useList_OUTB;
  delete this->useList_INB;
  delete this->freeList_INF;
  delete this->freeList_OUTF;

  delete this->IN;
  delete this->OUT;


  delete this->visit;


}
