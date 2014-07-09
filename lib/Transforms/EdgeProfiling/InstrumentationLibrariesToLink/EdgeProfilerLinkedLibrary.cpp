/*
 * TcProfilerLinkedLibrary.cpp
 *
 *  Created on: Dec 16, 2013
 *      Author: raphael
 */

#define __STDC_FORMAT_MACROS

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <map>
#include <set>
#include <iostream>
#include <fstream>

using namespace std;


extern "C"{
	void countVisitedBlock(int64_t BBPointer);
	void countVisitedEdge(int64_t BBFrom, int64_t BBTo);
	void flushProfilingData(char* moduleIdentifier);

}

class Edge{
public:
	int64_t from;
	int64_t to;

	Edge(int64_t from, int64_t to): from(from), to(to) {};
	~Edge(){};

    bool operator<(const Edge& other) const
    {
      if(from == other.from)
      {
        return this->to < other.to;
      }
      return from < other.from;
    }
};

std::map<int64_t, int> BBCount;
std::map<Edge, int> EdgeCount;

void countVisitedBlock(int64_t BBPointer){

	if(!BBCount.count(BBPointer)) BBCount[BBPointer] = 1;
	else BBCount[BBPointer]++;

}


void countVisitedEdge(int64_t BBFrom, int64_t BBTo){

	Edge key(BBFrom, BBTo);
	if(!EdgeCount.count(key)) EdgeCount[key] = 1;
	else EdgeCount[key]++;

}



void flushProfilingData(char* moduleIdentifier){

	ofstream out;

	out.open ("BasicBlocks.out", std::ofstream::out);
	for (std::map<int64_t, int>::iterator bbIt = BBCount.begin(), bbEnd = BBCount.end(); bbIt != bbEnd; bbIt++){
		out << bbIt->first << "|" << bbIt->second << "\n";
	}
	out.close();

	out.open ("Edges.out", std::ofstream::out);
	for (std::map<Edge, int>::iterator eIt = EdgeCount.begin(), eEnd = EdgeCount.end(); eIt != eEnd; eIt++){
		out << eIt->first.from << "|" << eIt->first.to << "|" << eIt->second << "\n";
	}
	out.close();



}
