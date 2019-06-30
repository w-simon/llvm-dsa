/*************************************************************************
    > File Name: FixInfo.h
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: Tue 29 Jul 2014 06:29:27 PM CST
 ************************************************************************/

#include<iostream>
#include<string>
using namespace std;
class FixInfo{
	public:
		FixInfo(string name, string file_name,  int column,int line);
		~FixInfo();
		string getFixStmt();
		int getfixColumn();
		int getfixLine();
		string getmallocNodeName();
		string getFileName();
	private:
		string name;
		int fixColumn;
		int fixLine;
		string fileName;	
		

};
