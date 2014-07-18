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
	void initBBandEdgeCounters(int64_t *BBPointer, int numBBs, int64_t *EdgePointer, int numEdges);
	void flushProfilingData(int64_t *BBPointer, int numBBs, int64_t *EdgePointer, int numEdges);

}


void initBBandEdgeCounters(int64_t *BBPointer, int numBBs, int64_t *EdgePointer, int numEdges){

	for (int i = 0; i < numBBs; i++) BBPointer[i] = 0;

	for (int i = 0; i < numEdges; i++) EdgePointer[i] = 0;
}


void flushProfilingData(int64_t *BBPointer, int numBBs, int64_t *EdgePointer, int numEdges){

	ofstream out;

	out.open ("BasicBlocks.out", std::ofstream::out);
	for (int i = 0; i < numBBs; i++) {
		if(BBPointer[i] > 0) out << i << "|" << BBPointer[i] << "\n";
	}
	out.close();

	out.open ("Edges.out", std::ofstream::out);
	for (int i = 0; i < numEdges; i++) {
		if(EdgePointer[i] > 0) out << i << "|" << EdgePointer[i] << "\n";
	}
	out.close();



}
