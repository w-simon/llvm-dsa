/*************************************************************************
    > File Name: FixInfo.cpp
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: Tue 29 Jul 2014 06:36:51 PM CST
 ************************************************************************/
#include<clang/LeakFix/FixInfo.h>
#include<iostream>
using namespace std;

FixInfo::FixInfo(string name, string file_name, int column,int line){
	this->name = name;
	this->fileName = file_name;
	this->fixColumn = column;
	this->fixLine = line;
}

string FixInfo::getFixStmt(){
	string free = "";
	return free;
}

int FixInfo::getfixColumn(){

	return this->fixColumn;
}

int FixInfo::getfixLine(){

	return this->fixLine;
}

string FixInfo::getmallocNodeName(){
	return this->name;
}

string FixInfo::getFileName(){
	return this->fileName;
}
