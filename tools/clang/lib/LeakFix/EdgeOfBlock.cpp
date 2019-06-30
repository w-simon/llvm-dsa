#include "clang/LeakFix/EdgeOfBlock.h"
EdgeOfBlock::EdgeOfBlock(int pred,int succ,int fk,int uk){

    this->pred_block = pred;
    this->succ_block = succ;
    this->free_kind = 0;
    this->use_kind = 0;
}
EdgeOfBlock::~EdgeOfBlock(){}

bool EdgeOfBlock::isTheEdge(int pred,int succ)
{

    return (this->pred_block == pred )&&(this->succ_block == succ);
}
void EdgeOfBlock::setPred(int pred){
    this->pred_block = pred;
}
void EdgeOfBlock::setSucc(int succ){
    this->succ_block= succ;
}
void EdgeOfBlock::setFreeKind(int k){

    this->free_kind = k;
}
void EdgeOfBlock::setUseKind(int k){
    this->use_kind = k;
}

int EdgeOfBlock::getPred(){
    return this->pred_block;
}

int EdgeOfBlock::getSucc(){
    return this->succ_block;
}

int EdgeOfBlock::getFreeKind(){
    return this->free_kind;
}
int EdgeOfBlock::getUseKind(){
    return this->use_kind;

}

