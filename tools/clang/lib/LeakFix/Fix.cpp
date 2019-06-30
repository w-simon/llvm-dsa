/*************************************************************************
  > File Name: Fix.cpp
  > Author: ma6174
  > Mail: ma6174@163.com 
  > Created Time: Fri 15 Aug 2014 04:58:57 PM CST
 ************************************************************************/
#include "clang/LeakFix/Fix.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Stmt.h"
#include "clang/Analysis/CFG.h"
#include "clang/LeakFix/CallStmtVisitor.h"
#include <map>
#include <fstream>
#include <utility>
#include <time.h>

using namespace clang;
using namespace std;

extern ASTContext *Contex;
extern string lto_cFile;
extern std::map<std::string, Stmt*> lto_strToCalledStmtMap;
extern std::map<string, string> lto_strToCalledFuncNameMap;
std::set<CFGBlock*> bracketSet;
int retCounter;
int tempCounter;
int callCounter;

void Fix::printBlockMap(){
  for (std::map<std::pair<int, int>, std::pair<CFGBlock*, CFGBlock::iterator> >::iterator I = NR->blockMap.begin(); I != NR->blockMap.end(); ++I){
    llvm::errs() << I->first.first << " " << I->first.second << "\n";
  }

}

static pair<int,int> printRearLocation(Stmt *S){
  SourceManager &SM = Contex->getSourceManager();
  SourceRange SR = S->getSourceRange();
  SourceLocation locBegin = SR.getBegin();
  SourceLocation locEnd = SR.getEnd();
  int lineN =  SM.getSpellingLineNumber(locEnd);
  int columN = SM.getSpellingColumnNumber(locEnd);
  return pair<int,int>(lineN,++columN);
}
static pair<int,int> printLocation(Stmt *S){
  SourceManager &SM = Contex->getSourceManager();
  SourceRange SR = S->getSourceRange();
  SourceLocation locBegin = SR.getBegin();
  SourceLocation locEnd = SR.getEnd();
  int lineN =  SM.getSpellingLineNumber(locBegin);
  int columN = SM.getSpellingColumnNumber(locBegin);
  return pair<int,int>(lineN,columN);
}
static pair<pair<int,int>,bool> printBracketLocation(int lineN, int columN, int lineEnd, int columEnd, string name, bool &isFront){
  ifstream infile;
  const char *name0 = name.data();
  infile.open(name0);
  int count = 1;
  bool readBegin = false;
  bool readEnd = false;
  string content;
  bool found = false;
  if(infile.is_open()){
    char lineContent[1024];
    
    while(infile.getline(lineContent,sizeof(lineContent))){
      if (readEnd) break;
      if (count == lineN){
        readBegin = true;
      }
      if (readBegin){
        content = lineContent;
        for (int i = 0; i < content.length(); ++i){
          if (content[i] == '{'){
            found = true;
            pair<int,int> p = pair<int,int>(count, i+2);
            pair<pair<int,int>,bool> p1 = pair<pair<int,int>,bool>(p, true);
            isFront = true;
            return p1;
          }
        }
      }
      if (count == lineEnd){
        readEnd = true;
      }
      ++count;
    }
    infile.close();
    
  }

  pair<int,int> p = pair<int,int>(0, 0);
  pair<pair<int,int>,bool> p1 = pair<pair<int,int>,bool>(p, false);
  return p1;
}

string Fix::getFileName(){
  SourceManager &sm = Contex->getSourceManager();

  const clang::Stmt *st;
  const clang::FileEntry *file_entry;
  for(clang::CFG::iterator iter = this->cfg->begin();iter!=this->cfg->end();iter++){
    if(!(*iter)->empty()){
      clang::CFGBlock::iterator BIter = (*iter)->begin();
      clang::CFGElement e = (*BIter);
      st= e.castAs<clang::CFGStmt>().getStmt();
      if (st){
        file_entry = getFileEntry(st);
        if(file_entry){
          break;
        }
      }
    }
  }
  if(file_entry == NULL) {


    return "";
  }

  string s = file_entry->getName();
  return s;
}

const clang::FileEntry *Fix::getFileEntry(const Stmt *st){
  SourceManager &sm = Contex->getSourceManager();
  clang::SourceLocation loc_begin = st->getLocStart();
  FullSourceLoc * full_loc_begin = new FullSourceLoc(loc_begin,sm);
  clang::FileID id = full_loc_begin->getFileID();
  delete full_loc_begin;
  return sm.getFileEntryForID(id);
}

void Fix::printResult(){
  const clang::Stmt *st=NULL;
  const clang::FileEntry *file_entry;
  for(clang::CFG::iterator iter = this->cfg->begin();iter!=this->cfg->end();iter++){
     
    if(!(*iter)->empty()){
      clang::CFGBlock::iterator BIter = (*iter)->begin();
      clang::CFGElement e = (*BIter);
      st= e.castAs<clang::CFGStmt>().getStmt();
      if (st){
        file_entry = getFileEntry(st);
        if(file_entry){
          file_name = file_entry->getName();
          break;
        }
      }
    }

  }

  if(file_entry){
    CFGBlock *exit = &cfg->getExit();
    int exitIndex = 0;
    pair<int,int> p(exit->getBlockID(), 0);
    map<pair<int,int>,vector<structNode> >::iterator mapIter;
    mapIter = this->structIN->find(p);
    vector<structNode> &v = mapIter->second;

    for (int i = 0; i < v.size(); ++i){
      structNode *I = &v[i];
      pair<int,int> locationNumber;
      bool isFront = false;
      if (I->isBlockEdge){
        int blockID = I->earBlockEdge.first;
        int succBlockID = I->earBlockEdge.second;
        CFGBlock *firstBlock = NR->blockIDMap[blockID];
        CFGBlock *secondBlock = NR->blockIDMap[succBlockID];
        if (secondBlock == &cfg->getExit()){
          CFGBlock::iterator BI = firstBlock->end();
          BI--;
          if (BI->getAs<CFGStmt>()){
            Stmt *S = const_cast<Stmt*>(BI->castAs<CFGStmt>().getStmt());
            if (ReturnStmt *RS = dyn_cast<ReturnStmt>(S)){
              continue;
            }
          }
        }
        CFGBlock::iterator BI = secondBlock->end();
        BI--;
        PrintInfo PI(v[i].var, true, I->earBlockEdge.second, 0);
        if (printed->count(PI) != 0) continue;
        printed->insert(PI);
        pair<pair<int,int>,bool> triple = printAssist(blockID, succBlockID, false, isFront);
        locationNumber = triple.first;
        if (secondBlock->size() == 1 && bracketSet.count(secondBlock) == 0){
          if (BI->getAs<CFGStmt>()){
            SourceManager &SM = Contex->getSourceManager();
            Stmt *S = const_cast<Stmt*>(BI->castAs<CFGStmt>().getStmt());
            SourceRange SR = S->getSourceRange();
            SourceLocation locBegin = SR.getBegin();
            SourceLocation locEnd = SR.getEnd();
            int lineN =  SM.getSpellingLineNumber(locBegin);
            int columN = SM.getSpellingColumnNumber(locBegin);
            int lineEnd = SM.getSpellingLineNumber(locEnd);
            int columEnd = SM.getSpellingColumnNumber(locEnd);
            bracketSet.insert(secondBlock);
            FixElement fe4("{",lineN,columN, 4, lineEnd, columEnd+2);
            this->toFix->insert(fe4);
            FixElement fe5("}",lineEnd,columEnd+2, 4, lineEnd, columEnd+2);
            this->toFix->insert(fe5);
          }
        }
      }
      else{
        PrintInfo PI(v[i].var, false, I->earElementEdge.first, I->earElementEdge.second);
        if (printed->count(PI) != 0) continue;
        printed->insert(PI);
        int blockID = I->earElementEdge.first;
        int edgeID = I->earElementEdge.second;
        pair<int,int> p1(blockID, edgeID);
        if (NR->blockMap.count(p1) != 0){
          pair<CFGBlock*, CFGBlock::iterator> p2 = NR->blockMap[p1];
          if (p2.second->getAs<CFGStmt>()){
            Stmt *S = const_cast<Stmt*>(p2.second->castAs<CFGStmt>().getStmt());
            locationNumber=printRearLocation(S);
          }
        }
      }
      FixElement fe(v[i].var,locationNumber.first,locationNumber.second);
      fe.isFront = isFront;
      this->toFix->insert(fe);

      if (v[i].var.find("leakfix_temp_") != string::npos){
        Stmt *S = lto_strToCalledStmtMap[v[i].var];
        string calledFuncName = lto_strToCalledFuncNameMap[v[i].var];
        CallStmtReplaceVisitor CSRV(Contex, S, v[i].var, calledFuncName);
        CSRV.Visit(S);

        pair<int,int> locationNumber = printLocation(S);
        SourceManager &SM = Contex->getSourceManager();
        SourceRange SR = S->getSourceRange();
        SourceLocation locEnd = SR.getEnd();
        int lineEnd = SM.getSpellingLineNumber(locEnd);
        int columEnd = SM.getSpellingColumnNumber(locEnd);
        FixElement fe2(CSRV.result,locationNumber.first,locationNumber.second, 2, lineEnd, columEnd);
        this->toFix->insert(fe2);
      }
    }
  }

}

pair<pair<int,int>,bool> Fix::printAssist(int blockID, int succBlockID, bool fixAdded, bool &isFront){
  pair<int,int> locationNumber;
  pair<int,int> p11(blockID,0);
  pair<int,int> p1(succBlockID, 0);
  CFGBlock * predBlock = NR->blockIDMap[blockID];
  CFGBlock * succBlock = NR->blockIDMap[succBlockID];
  if(predBlock->succ_size()>=1){
    pair<CFGBlock*, CFGBlock::iterator> p22= NR->blockMap[p11];
    if (NR->blockMap.count(p1) != 0){
      CFGBlock *B = NR->blockIDMap[succBlockID];
      if (B->size() > 0 && B->begin()->getAs<CFGStmt>()){
        Stmt *S = const_cast<Stmt*>(B->begin()->castAs<CFGStmt>().getStmt());
        const clang::FileEntry *file_entry = getFileEntry(S);
        if (file_entry){
          if (succBlock->getTerminatorCondition() == NULL){
            locationNumber=printLocation(S);
            fixAdded = true;
            isFront = true;
          }
          else if (predBlock->pred_size() == 1){
            CFGBlock *b = *predBlock->pred_begin();
            pair<pair<int,int>,bool> triple = printAssist(b->getBlockID(), blockID, fixAdded, isFront);
            locationNumber = triple.first;
            fixAdded = triple.second;
          }
        }
        else{
          if (predBlock->getTerminatorCondition() != NULL){
            Stmt *Cond = predBlock->getTerminatorCondition();
            //const ASTContext::ParentVector &PV = Contex->getParents<clang::Stmt>(*Cond);
            const auto &PV = Contex->getParents<clang::Stmt>(*Cond);
            SourceManager &SM = Contex->getSourceManager();
            SourceRange SR = PV[0].getSourceRange();
            SourceLocation locBegin = SR.getBegin();
            int lineN =  SM.getSpellingLineNumber(locBegin);
            int columN = SM.getSpellingColumnNumber(locBegin);
            SourceLocation locEnd = SR.getEnd();
            int lineEnd = SM.getSpellingLineNumber(locEnd);
            int columEnd = SM.getSpellingColumnNumber(locEnd);
            pair<pair<int,int>,bool> p =printBracketLocation(lineN, columN, lineEnd, columEnd, file_name, isFront);
            fixAdded = p.second;
            if (fixAdded) locationNumber = p.first;
          }
        }
      }
    }
  }
  if (!fixAdded){
    unsigned int size = predBlock->size();
    pair<int,int> p3(blockID,size-1);
    if(NR->blockMap.count(p3)){
      pair<CFGBlock*,CFGBlock::iterator> p33 = NR->blockMap[p3];
      if(p33.second->getAs<CFGStmt>()){
        Stmt *S = const_cast<Stmt*>(p33.second->castAs<CFGStmt>().getStmt());
        locationNumber=printRearLocation(S);
        isFront = false;

      }
    }
  }
  return pair<pair<int,int>,bool>(locationNumber,fixAdded);
}

void Fix::printCmdResult(){
  CFGBlock *exit = &cfg->getExit();
  int exitIndex = 0;
  pair<int,int> p(exit->getBlockID(), 0);
  map<pair<int,int>,vector<structNode> >::iterator mapIter;
  mapIter = this->structIN->find(p);
  vector<structNode> &v = mapIter->second;
  if (v.size() > 0){
    llvm::errs() << "Printing the result in Function " << NR->F->getName() << " in " << lto_cFile << "...\n";
    set<PrintInfo, PrintComp> printed;
    for (int i = 0; i < v.size(); ++i){
      structNode *I = &v[i];
      if (I->isBlockEdge){
        PrintInfo PI(v[i].var, true, I->earBlockEdge.second, 0);
        if (printed.count(PI) != 0) continue;
        printed.insert(PI);
        llvm::errs() << "Insert: ";
        llvm::errs().changeColor(raw_ostream::YELLOW, true);
        llvm::errs() << "free(" << v[i].var << ");\n";
        llvm::errs().resetColor();
        int blockID = I->earBlockEdge.first;
        int succBlockID = I->earBlockEdge.second;
        pair<int,int> p1(succBlockID, 0);
        if (NR->blockMap.count(p1) != 0){
          pair<CFGBlock*, CFGBlock::iterator> p2 = NR->blockMap[p1];
          llvm::errs() << "IsExit: " << isExit(p2.first) << "\n";
          if (p2.second->getAs<CFGStmt>()){
            Stmt *S = const_cast<Stmt*>(p2.second->castAs<CFGStmt>().getStmt());
            printLocation(S);
            llvm::errs() << "Before statement: ";
            S->dumpPretty(*Contex);
          }
        }
      }
      else{
        PrintInfo PI(v[i].var, false, I->earElementEdge.first, I->earElementEdge.second);
        if (printed.count(PI) != 0) continue;
        printed.insert(PI);
        llvm::errs() << "Insert: ";
        llvm::errs().changeColor(raw_ostream::YELLOW, true);
        llvm::errs() << "free(" << v[i].var << ");\n";
        llvm::errs().resetColor();
        int blockID = I->earElementEdge.first;
        int edgeID = I->earElementEdge.second;
        pair<int,int> p1(blockID, edgeID);
        if (NR->blockMap.count(p1) != 0){
          pair<CFGBlock*, CFGBlock::iterator> p2 = NR->blockMap[p1];
          if (p2.second->getAs<CFGStmt>()){
            Stmt *S = const_cast<Stmt*>(p2.second->castAs<CFGStmt>().getStmt());
            printLocation(S);
            llvm::errs() << "After statement: ";
            S->dumpPretty(*Contex);
          }
        }
      }
      llvm::errs() << "\n\n";
    }
    llvm::errs() << "\n";
  }

}

multiset<FixElement,FixComp> Fix::toFixSet(){
  return *this->toFix;
}

void Fix::printStructInfo(){
  llvm::errs() << "*******\nFix result for " << NR->F->getName() << "...\n";
  llvm::errs() << "EntryID: " << cfg->getEntry().getBlockID() << "\n";
  llvm::errs() << "ExitID: " << cfg->getExit().getBlockID() << "\n";
  llvm::errs() << "IN: \n";
  for (map<pair<int,int>,vector<structNode> >::iterator I = this->structIN->begin(); I != this->structIN->end(); ++I){
    pair<int,int> p = I->first;
    llvm::errs() << "PP: " << p.first << " " << p.second << "\n";
    vector<structNode> &v = I->second;
    map<pair<int, int>,pair<CFGBlock*, CFGBlock::iterator> >::iterator elementIter = NR->blockMap.find(p);
    if (elementIter != NR->blockMap.end()){
      CFGBlock::iterator BI = elementIter->second.second;
      if (BI->getAs<CFGStmt>()){
        llvm::errs() << "Stmt: ";
        BI->castAs<CFGStmt>().getStmt()->dumpPretty(*Contex);
        llvm::errs() << "\n";

      }
    }
    int s = v.size();
    for (int i = 0; i < s; ++i){
      llvm::errs() << "Fix set: \n";
      llvm::errs() << i << " " << v.size() << "\n";
      GA->printSet(llvm::errs(), v[i].fixSet);
      llvm::errs() << "Var: " << v[i].var;
      printFixLocation(v[i]);
      llvm::errs() << "\n";
    }
  }
  llvm::errs() << "OUT: \n";
  for (map<pair<int,int>,vector<structNode> >::iterator I = this->structOUT->begin(); I != this->structOUT->end(); ++I){
    pair<int,int> p = I->first;
    llvm::errs() << "PP: " << p.first << " " << p.second << "\n";
    vector<structNode> &v = I->second;    
    map<pair<int, int>,pair<CFGBlock*, CFGBlock::iterator> >::iterator elementIter = NR->blockMap.find(p);
    if (elementIter != NR->blockMap.end()){
      CFGBlock::iterator BI = elementIter->second.second;
      if (BI->getAs<CFGStmt>()){
        llvm::errs() << "Stmt: ";
        BI->castAs<CFGStmt>().getStmt()->dumpPretty(*Contex);
        llvm::errs() << "\n";
      }
    }
    int s = v.size();
    for (int i = 0; i < s; ++i){
      llvm::errs() << "Fix set: \n";
      llvm::errs() << i << " " << v.size() << "\n";
      GA->printSet(llvm::errs(), v[i].fixSet);
      llvm::errs() << "Var: " << v[i].var;
      printFixLocation(v[i]);
      llvm::errs() << "\n";
    }
  }
}

void Fix::printFixLocation(structNode &v){
  llvm::errs() << "Fix location: \n";
  if (v.isBlockEdge){
    llvm::errs() << "  Block edge.\n";
    pair<int,int> blockEdgePair = v.earBlockEdge;
    CFGBlock *firstBlock = NR->blockIDMap[blockEdgePair.first];
    CFGBlock *secondBlock = NR->blockIDMap[blockEdgePair.second];

    CFGBlock::iterator BI = firstBlock->end();
    BI--;
    if (BI->getAs<CFGStmt>()){
      llvm::errs() << "  First block last element:\n  ";
      BI->castAs<CFGStmt>().getStmt()->dumpPretty(*Contex);
      llvm::errs() << "\n";
    }
    BI = secondBlock->begin();
    if (BI->getAs<CFGStmt>()){
      if (BI->castAs<CFGStmt>().getStmt()){
        llvm::errs() << "  Second block first element:\n  ";
        llvm::errs() << "\n";
      }
    }
  }
  else{
    llvm::errs() << "  Element edge.\n";
    pair<int,int> elementEdgePair = v.earElementEdge;
    map<pair<int, int>,pair<CFGBlock*, CFGBlock::iterator> >::iterator elementIter = NR->blockMap.find(elementEdgePair);
    if (elementIter != NR->blockMap.end()){
      CFGBlock::iterator BI = elementIter->second.second;
      if (BI->getAs<CFGStmt>()){
        llvm::errs() << "  After element:\n  ";
        BI->castAs<CFGStmt>().getStmt()->dumpPretty(*Contex);
        llvm::errs() << "\n";
      }
    }
  }
}

void Fix::printBlocks(){
  for (CFG::iterator CI = cfg->begin(); CI != cfg->end(); ++CI){
    CFGBlock *block = *CI;
    llvm::errs() << block->getBlockID() << "\n";
    block->dump(cfg, LangOptions(), false);
    llvm::errs() << "\n";
  }
}

void Fix::printBlock(CFGBlock *block){
  llvm::errs() << block->getBlockID() << "\n";
  block->dump(cfg, LangOptions(), false);
  llvm::errs() << "\n";
}

void Fix::printAnalyzedResult(){
  callCounter++;
  llvm::errs() << "Analyzed time: " << callCounter << "\n";

  llvm::errs() << "Exit block ID: " << cfg->getExit().getBlockID() << "\n";
  for (CFG::iterator CI = cfg->begin(); CI != cfg->end(); ++CI){
    CFGBlock *block = *CI;
    int id = block->getBlockID();
    if(block->size()==0){

      pair<int,int> p(id, 0);
      map<pair<int,int>,vector<structNode> >::iterator mapINIter;
      map<pair<int,int>,vector<structNode> >::iterator mapIter;
      mapINIter = this->structIN->find(p);
      mapIter = this->structOUT->find(p);
      vector<structNode> &INv = mapINIter->second;
      vector<structNode> &v = mapIter->second;
      llvm::errs() << "Empty block.\n";
      if (block == &cfg->getExit()){
        llvm::errs() << "isExit.\n";
      }
      else if (block == &cfg->getEntry()){
        llvm::errs() << "isEntry.\n";
      }
      llvm::errs() << "Size: " << INv.size() << " " << v.size() << "\n";
      llvm::errs() << "IN:\n";
      int s = INv.size();
      for (int i = 0; i < s; ++i){
        llvm::errs() << "Fix set: \n";
        llvm::errs() << i << " " << INv.size() << "\n";
        GA->printSet(llvm::errs(), INv[i].fixSet);
        llvm::errs() << "Var: " << INv[i].var;
        llvm::errs() << "\n";
      }
      llvm::errs() << "OUT:\n";
      s = v.size();
      for (int i = 0; i < s; ++i){
        llvm::errs() << "Fix set: \n";
        llvm::errs() << i << " " << v.size() << "\n";
        GA->printSet(llvm::errs(), v[i].fixSet);
        llvm::errs() << "Var: " << v[i].var;
        llvm::errs() << "\n";
      }
    }

    else{
      for (CFGBlock::iterator BI = block->begin(); BI != block->end(); ++BI){
        int index = BI - block->begin();
        pair<int,int> p(id, index);
        map<pair<int,int>,vector<structNode> >::iterator mapINIter;
        map<pair<int,int>,vector<structNode> >::iterator mapIter;
        mapINIter = this->structIN->find(p);
        mapIter = this->structOUT->find(p);
        vector<structNode> &INv = mapIter->second;
        vector<structNode> &v = mapIter->second;
        llvm::errs() << "Stmt: ";
        if (BI->getAs<CFGStmt>()){
          BI->castAs<CFGStmt>().getStmt()->dumpPretty(*Contex);
        }
        llvm::errs() << "\n";
        llvm::errs() << "Size: " << INv.size() << " " << v.size() << "\n";
        llvm::errs() << "IN:\n";
        int s = INv.size();
        for (int i = 0; i < s; ++i){
          llvm::errs() << "Fix set: \n";
          llvm::errs() << i << " " << INv.size() << "\n";
          GA->printSet(llvm::errs(), INv[i].fixSet);
          llvm::errs() << "Var: " << INv[i].var;
          llvm::errs() << "\n";
        }
        llvm::errs() << "OUT:\n";
        s = v.size();
        for (int i = 0; i < s; ++i){
          llvm::errs() << "Fix set: \n";
          llvm::errs() << i << " " << v.size() << "\n";
          GA->printSet(llvm::errs(), v[i].fixSet);
          llvm::errs() << "Var: " << v[i].var;
          llvm::errs() << "\n";
        }
      }
    }
  }
}
void Fix::printExitResult(){
  CFGBlock *exit = &cfg->getExit();
  int id = exit->getBlockID();
  pair<int,int> p(id, 0);
  map<pair<int,int>,vector<structNode> >::iterator mapIter;
  mapIter = this->structOUT->find(p);
  vector<structNode> &v = mapIter->second;
  if (v.size() > 0){
    for (vector<structNode>::iterator VI = v.begin(); VI != v.end(); ++VI){
      if (VI->isBlockEdge){
        pair<int,int> blockEdgePair = VI->earBlockEdge;
        CFGBlock *firstBlock = NR->blockIDMap[blockEdgePair.first];
        CFGBlock *secondBlock = NR->blockIDMap[blockEdgePair.second];
        if (secondBlock == &cfg->getExit()){
          CFGBlock::iterator BI = firstBlock->end();
          BI--;
          if (BI->getAs<CFGStmt>()){
            Stmt *S = const_cast<Stmt*>(BI->castAs<CFGStmt>().getStmt());
            if (ReturnStmt *RS = dyn_cast<ReturnStmt>(S)){
              Expr *E = RS->getRetValue();
              QualType T = E->getType();
              string type = T.getAsString();
              string s;
              llvm::raw_string_ostream os(s);
              os << type << " leakfix_ret_" << retCounter << " = ";
              E->printPretty(os, 0, PrintingPolicy(Contex->getLangOpts()));
              os << "; free(" << VI->var << "); " << "return leakfix_ret_" << retCounter << "; ";
              retCounter++;
              pair<int,int> locationNumber = printLocation(RS);
              SourceManager &SM = Contex->getSourceManager();
              SourceRange SR = S->getSourceRange();
              SourceLocation locEnd = SR.getEnd();
              int lineEnd = SM.getSpellingLineNumber(locEnd);
              int columEnd = SM.getSpellingColumnNumber(locEnd);
              FixElement fe(os.str(),locationNumber.first,locationNumber.second, 1, lineEnd, columEnd);
              this->toFix->insert(fe);
            }
          }

        }
      }
      else{
        pair<int,int> elementEdgePair = VI->earElementEdge;
        map<pair<int, int>,pair<CFGBlock*, CFGBlock::iterator> >::iterator elementIter = NR->blockMap.find(elementEdgePair);
        CFGBlock::iterator BI = elementIter->second.second;
        if (BI->getAs<CFGStmt>()){
        }
      }
    }
  }
}


void Fix::analyze(){
  initVisit();
  initMap();
  getFixEdge();
  initStructIN();
  forward();  
  printExitResult();
  printResult();
}

Fix::Fix(CFG *cfg0, NodeRecognizer *nr, GraphAnalyzer *ga, VariableAnalyzer *va) : NR(nr), GA(ga), VA(va){
  this->cfg = cfg0;

  this->elementInfoList=new map<int,ElementInfo>;
  this->IN = ga->mallocList_INF;

  this-> OUT= ga->mallocList_OUTF;

  this->blockEdgeMap = new map<pair<int,int>, map<set<int>,string> >;
  this->elementEdgeMap = new map<pair<int,int>, map<set<int>,string> >;
  this->structIN = new map<pair<int,int>,vector<structNode> >;
  this->structOUT = new map<pair<int,int>,vector<structNode> >;
  this->visit = new set<CFGBlock*,compare_cfg2>;
  this->printed = new set<PrintInfo,PrintComp>;
  this->toFix = new multiset<FixElement,FixComp>;


}


Fix::~Fix(){

  delete this->elementInfoList;
  delete this->IN;
  delete this->OUT;
  delete this->blockEdgeMap;
  delete this->elementEdgeMap;
  delete this->structIN;
  delete this->structOUT;
  delete this->visit;
  delete this->printed;
  delete this->toFix;
}

void Fix::initVisit(){

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


//For each CFGElemeent, add initialized structNodeSet
void Fix::initMap(){

  CFGBlock * block;
  unsigned int block_id;
  unsigned int size;
  int index;
  vector<structNode> mallocSet;

  for(clang::CFG::iterator i= cfg->begin(); i!=cfg->end(); ++i){

    block = *i;

    block_id = block->getBlockID();
    size = block->size();



    for(clang::CFGBlock::iterator iter = block->begin(); iter!=block->end();iter++)
    {	


      index = iter- block->begin();
      pair<int,int> idPair(block_id,index);

      pair<pair<int,int>, vector<structNode> > elePair(idPair,mallocSet);
      this->structIN->insert(elePair);
      this->structOUT->insert(elePair);

    }
    if(size==0){
      pair<int,int> idPair(block_id,0);
      pair<pair<int,int>, vector<structNode> > elePair(idPair,mallocSet);
      this->structIN->insert(elePair);
      this->structOUT->insert(elePair);



    }

  }
}

//Using set<int> and string info to initialize the final data structure: struct on each block entry 
void Fix::initStructIN(){
  CFGBlock *block;
  unsigned int block_id;
  unsigned int size;

  for(CFG::iterator i=cfg->begin();i!=cfg->end();i++){
    block = *i;
    if(isEntry(block))continue;

    block_id = block->getBlockID();
    size = block->size();
    vector<structNode> structSet;
    for(CFGBlock::const_pred_iterator j=block->pred_begin();j!=block->pred_end();j++){

      CFGBlock * pred_block = *j;
      unsigned int pred_id = pred_block->getBlockID();
      map<pair<int,int>,map<set<int>,string> >::iterator mapIter;
      mapIter = this->blockEdgeMap->find(pair<int,int>(pred_id,block_id));
      map<set<int>,string > structEdgeMap = mapIter->second;
      for(map<set<int>,string>::iterator iter=structEdgeMap.begin();iter!=structEdgeMap.end();iter++){
        structNode s;
        s.fixSet = iter->first;
        s.var = iter->second;
        s.isBlockEdge =true;
        pair<int,int> p(pred_id,block_id);
        s.earBlockEdge = p;
        structSet.push_back(s);
      }


    }
    map<pair<int,int>,vector<structNode> >::iterator sMapIter;
    sMapIter = this->structIN->find(pair<int,int>(block_id,0));
    if(sMapIter == this->structIN->end()){
    }
    else{
      this->structIN->erase(sMapIter);
    }
    sMapIter = this->structOUT->find(pair<int,int>(block_id,0));
    if(sMapIter == this->structOUT->end()){
    }
    else{
      this->structOUT->erase(sMapIter);
    }

    pair<int,int> idPair(block_id,0);
    pair<pair<int,int>,vector<structNode> > elePair(idPair,structSet);
    this->structIN->insert(elePair);
    if (isExit(block))
      this->structOUT->insert(elePair);

    if(size>1){

      for(int i = 1; i<size;i++){
        vector<structNode> structSet0;
        map<pair<int,int>,map<set<int>,string> >::iterator mapIter;
        mapIter = this->elementEdgeMap->find(pair<int,int>(block_id,i-1));
        map<set<int>,string > structEdgeMap = mapIter->second;
        for(map<set<int>,string>::iterator iter=structEdgeMap.begin();iter!=structEdgeMap.end();iter++){

          structNode s;
          s.fixSet = iter->first;
          s.var = iter->second;
          s.isBlockEdge =false;
          pair<int,int> p(block_id,i-1);
          s.earElementEdge = p;
          structSet0.push_back(s);
        }
        map<pair<int,int>,vector<structNode> >::iterator sMapIter;
        sMapIter = this->structIN->find(pair<int,int>(block_id,i));
        if(sMapIter == this->structIN->end()){
        }
        else{
          this->structIN->erase(sMapIter);
        }

        sMapIter = this->structOUT->find(pair<int,int>(block_id,i));
        if(sMapIter == this->structOUT->end()){
        }
        else{
          this->structOUT->erase(sMapIter);
        }

        pair<int,int> idPair0(block_id,i-1);
        pair<pair<int,int>,vector<structNode> > elePair0(idPair0,structSet0);
        this->structOUT->insert(elePair0);
        pair<int,int> idPair1(block_id,i);
        pair<pair<int,int>,vector<structNode> > elePair1(idPair1,structSet0);
        this->structIN->insert(elePair1);
      }
    }

      vector<structNode> structSet1;
      vector<structNode> emptySet;
    for (CFGBlock::const_succ_iterator SI = block->succ_begin(); SI != block->succ_end(); ++SI){
      CFGBlock * succ_block = *SI;
      if (!succ_block) break;
      unsigned int succ_id = succ_block->getBlockID();
      map<pair<int,int>,map<set<int>,string> >::iterator mapIter;
      mapIter = this->blockEdgeMap->find(pair<int,int>(block_id,succ_id));
      map<set<int>,string > structEdgeMap = mapIter->second;
      for(map<set<int>,string>::iterator iter=structEdgeMap.begin();iter!=structEdgeMap.end();iter++){
        structNode s;
        s.fixSet = iter->first;
        s.var = iter->second;
        s.isBlockEdge =true;
        pair<int,int> p(block_id, succ_id);
        s.earBlockEdge = p;
        structSet1.push_back(s);
      }
    }
      pair<int,int> idPair1(block_id, block->size() > 0? block->size()-1: 0);
      sMapIter = this->structOUT->find(idPair1);
      if(sMapIter == this->structOUT->end()){
      }
      else{
        this->structOUT->erase(sMapIter);
      }
      if (block->succ_size() <= 1){
        pair<pair<int,int>,vector<structNode> > elePair1(idPair1,structSet1);
        this->structOUT->insert(elePair1);
      }
      else{
        pair<pair<int,int>,vector<structNode> > elePair1(idPair1,emptySet);
        this->structOUT->insert(elePair1);
      }

  }


}
ElementInfo Fix::getElementInfo(int id){
  ElementInfo e;
  map<int,ElementInfo>::iterator mapIter;
  mapIter = this->elementInfoList->find(id);
  if(mapIter!=this->elementInfoList->end()){
    e=mapIter->second;
  }
  return e;
}

bool Fix::isEntry(CFGBlock *block){
  CFGBlock entry = cfg->getEntry();
  unsigned int id = entry.getBlockID();
  return (id == block->getBlockID());

}

bool Fix::isExit(CFGBlock *block){
  CFGBlock exit = cfg->getExit();
  unsigned int id = exit.getBlockID();
  return (id == block->getBlockID());
}

//Initialization. For each node in and out, initialize the set<int> and string information according to canFix result
void Fix::getFixEdge(){

  for(CFG::iterator iter=cfg->begin();iter!=cfg->end();iter++){
    CFGBlock *block = (*iter);
    CFGBlock *succ_block;
    unsigned int id= block->getBlockID();
    unsigned int size = block->size();
    int index=0;
    string var;
    set<int>  mallocSet;
    map<pair<int,int>,set<int> >::iterator mapIter;
    CFGBlock::iterator in_iter,out_iter;
    if(size==0){
      in_iter=block->begin();
      pair<int,int> idPair(id,0);
      mapIter = this->OUT->find(idPair);
      mallocSet = mapIter->second;
      int succ_number=0;

      for(CFGBlock::const_succ_iterator next=block->succ_begin();next!=block->succ_end();next++){
        if (!*next) continue;
        succ_number++;
      }
      for(CFGBlock::const_succ_iterator next=block->succ_begin();next!=block->succ_end();next++){
        if (!*next) continue;
        succ_block=(*next);
        unsigned int succ_id =succ_block->getBlockID();
        unsigned int succ_size = succ_block->size();

        out_iter = succ_block->begin();
        //add
        pair<int,int> succ_idPair(succ_id,0);
        mapIter = this->IN->find(succ_idPair);
        if(succ_number > 1){
          mallocSet = mapIter->second;
        }


        map<set<int>,string> varMap;

        bool fixable = VA->canFix(&mallocSet,block,in_iter,succ_block,out_iter,varMap);

        pair<int,int> idPair(id,succ_id);

        pair<pair<int,int>,map<set<int>,string> > elePair(idPair, varMap);
        this->blockEdgeMap->insert(elePair);

      }


    }
    else{
      int number=0;
      for(CFGBlock::iterator i=block->begin();i!=block->end();i++){
        in_iter=i;
        out_iter= i+1;

        if(out_iter==block->end())break;
        index=in_iter-block->begin();
        pair<int,int> idPair(id,index);
        mapIter=this->OUT->find(idPair);
        mallocSet=mapIter->second;
        map<set<int>,string> varMap;

        bool fixable = VA->canFix(&mallocSet,block,in_iter,succ_block,out_iter,varMap);


        pair<pair<int,int>,map<set<int>,string> > elePair(idPair, varMap);
        this->elementEdgeMap->insert(elePair);

      }
      index = in_iter - block->begin();
      pair<int,int> idPair(id,index);
      mapIter=this->OUT->find(idPair);
      mallocSet = mapIter->second;
      int succ_number=0;
      //add
      for(CFGBlock::const_succ_iterator next=block->succ_begin();next!=block->succ_end();next++){
        if (!*next) continue;
        succ_number++;
      }
      for(CFGBlock::const_succ_iterator next=block->succ_begin();next!=block->succ_end();next++){
        if (!*next) continue;
        succ_block=(*next);
        unsigned int succ_id =succ_block->getBlockID();
        unsigned int succ_size = succ_block->size();

        out_iter = succ_block->begin();
        //add
        pair<int,int> succ_idPair(succ_id,0);
        mapIter = this->IN->find(succ_idPair);
        if(succ_number > 1){
          mallocSet = mapIter->second;
        }


        map<set<int>,string> varMap;
        bool fixable = VA->canFix(&mallocSet,block,in_iter,succ_block,out_iter,varMap);

        pair<int,int> idPair(id,succ_id);
        pair<pair<int,int>,map<set<int>,string> > elePair(idPair, varMap);
        this->blockEdgeMap->insert(elePair);

      }





    }


  }

}
void Fix::forward(){
  CFGBlock* cb;
  set<CFGBlock*,compare_cfg>::iterator i=visit->begin();
  if(i!=this->visit->end()){
    cb = (*i);
    this->visit->erase(i);
    if(isEntry(cb)){
      forward();
    }
    else{
      forwardProcess(cb);
      forward();
    }

  }
}
void Fix::unionVector(vector<structNode> &v){
  vector<structNode> v1;
  for(vector<structNode>::iterator i=v.begin();i!=v.end();i++){
    structNode sn = (*i);
    if(!includeNode(v1,sn)){
      v1.push_back(sn);
    }
  }
  v = v1;

}
bool Fix::includeNode(vector<structNode> v,structNode n){
  for(vector<structNode>::iterator j=v.begin();j!=v.end();j++){
    structNode s=(*j);
    if(s==n)return true;
  }
  return false;
}
vector<structNode> Fix::meetNotNull(vector<structNode> in,vector<structNode> out){
  vector<structNode> outNew = in;
  for(vector<structNode>::iterator i=out.begin();i!=out.end();i++){
    structNode n=(*i);
    bool is=false;
    for(vector<structNode>::iterator j=in.begin();j!=in.end();j++){
      structNode n2=(*j);
      if(canMeet(n,n2)){
        is=true;
        break;
      }
    }
    if(is==false){
      outNew.push_back(n);
    }


  }
  return outNew;
}
bool Fix::canMeet(structNode s1,structNode s2){
  set<int> fixset_in = s1.fixSet;
  set<int> fixSet_out = s2.fixSet;
  for(set<int>::iterator i = fixset_in.begin();i!=fixset_in.end();i++){
    int a=(*i);
    if(fixSet_out.find(a) != fixSet_out.end()){
      return true;
    }
  }
  return false;

}

void Fix::forwardProcess(CFGBlock* block){

  unsigned int id = block->getBlockID();
  unsigned int index = 0;//if no elemnt we will use the created single element 
  map<pair<int,int>,vector<structNode> >::iterator mapIter;
  mapIter = this->structIN->find(pair<int,int>(id,0));
  vector<structNode>  structInSet = mapIter->second;
  vector<structNode>  newStructInSet;



  mapIter = this->structOUT->find(pair<int,int>(id,0));
  vector<structNode> structOutSet = mapIter->second;
  vector<structNode> newstructOutSet;
  for(clang::CFGBlock::const_pred_iterator i= block->pred_begin(); i!=block->pred_end(); ++i){
    int predID=(*i)->getBlockID();

    int size= (*i)->size();

    vector<structNode> predOutSet;
    mapIter =  this->structOUT->find(pair<int,int>(predID,size>0?size-1:0));
    predOutSet= mapIter->second;
    for(vector<structNode>::iterator iter= predOutSet.begin();iter!= predOutSet.end();iter++){

      newStructInSet.push_back(*iter);
    }


  }
  //union
  unionVector(newStructInSet);
  newStructInSet = meetNotNull(newStructInSet, structInSet);
  mapIter = this->structIN->find(pair<int,int>(id,0));

  this->structIN->erase(mapIter);

  pair<int,int> idPair(id,0);

  pair<pair<int,int>, vector<structNode> > elePair(idPair,newStructInSet);
  this->structIN->insert(elePair);
  if(block->size()==0){
    newstructOutSet = meetNotNull(newStructInSet,structOutSet);
    if(newstructOutSet != structOutSet ){

      for(clang::CFGBlock::const_succ_iterator i=block->succ_begin(); i!=block->succ_end(); i++){
        if (!*i) continue;
        this->visit->insert(*i);
      }

      mapIter = this->structOUT->find(pair<int,int>(id,0));
      this->structOUT->erase(mapIter);

      pair<int,int> idPair(id,0);

      pair<pair<int,int>, vector<structNode> > elePair(idPair,newstructOutSet);
      this->structOUT->insert(elePair);

    }

  }
  else{
    processElement(block);
  }

}

void Fix::processElement(CFGBlock *block){
  unsigned int id = block->getBlockID();
  unsigned int size= block->size();

  bool change=false;

  map<pair<int,int>,vector<structNode> >::iterator mapIter;
  mapIter = this->structIN->find(pair<int,int>(id,0));
  vector<structNode>  structInSet = mapIter->second;

  mapIter = this->structOUT->find(pair<int,int>(id,0));
  vector<structNode> structOutSet = mapIter->second;
  vector<structNode> newstructOutSet;
  newstructOutSet = meetNotNull(structInSet,structOutSet);

  if(newstructOutSet != structOutSet){
    change=true;
    mapIter = this->structOUT->find(pair<int,int>(id,0));
    this->structOUT->erase(mapIter);

    pair<int,int> idPair(id,0);

    pair<pair<int,int>, vector<structNode> > elePair(idPair,newstructOutSet);
    this->structOUT->insert(elePair);

  }
  if(size>1){
    for(int i=1;i<size;i++){
      mapIter = this->structOUT->find(pair<int,int>(id,i));
      structOutSet = mapIter->second;

      mapIter = this->structOUT->find(pair<int,int>(id,i-1));
      newstructOutSet  = mapIter->second;

      mapIter = this->structIN->find(pair<int,int>(id,i));

      this->structIN->erase(mapIter);
      pair<int,int> idPair(id,i);

      pair<pair<int,int>, vector<structNode> > elePair(idPair,newstructOutSet);
      this->structIN->insert(elePair);
      structInSet = newstructOutSet;


      newstructOutSet = meetNotNull(structInSet,structOutSet);


      if(newstructOutSet != structOutSet){
        change=true;
        mapIter = this->structOUT->find(pair<int,int>(id,i));
        this->structOUT->erase(mapIter);

        pair<int,int> idPair(id,i);

        pair<pair<int,int>, vector<structNode> > elePair(idPair,newstructOutSet);
        this->structOUT->insert(elePair);

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
