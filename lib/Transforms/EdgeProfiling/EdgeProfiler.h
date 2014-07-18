/*
 * EdgeProfiling.h
 *
 *  Created on: Jul 2, 2014
 *      Author: raphael
 */

#include <map>
#include <strings.h>

#include "llvm/Pass.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/raw_ostream.h"

#include "ExitInfo.h"


namespace llvm{


	class EdgeProfiler: public ModulePass{
	private:
		Value *initBBandEdgeCounters, *flushProfilingData;
		Value* moduleIdentifierStr;

		GlobalVariable *BBCounter, *EdgeCounter;

		BasicBlock *EntryBlock;

		std::map<BasicBlock*, int> BBmap;
		std::map<std::pair<BasicBlock*, BasicBlock*> , int> EdgeMap;


	public:
		static char ID;

		EdgeProfiler() : ModulePass(ID), initBBandEdgeCounters(NULL),
				         flushProfilingData(NULL), moduleIdentifierStr(NULL),
				         BBCounter(NULL), EdgeCounter(NULL), EntryBlock(NULL){};
		~EdgeProfiler() {};

		bool runOnModule(Module &M);

		virtual void getAnalysisUsage(AnalysisUsage &AU) const{
			AU.addRequired<ExitInfo>();
		}

		void insertDeclarations(Module &M);
		void printBasicBlocks(Module &M);
		void printEdges(Module &M);
		void insertInitInstrumentation(Module &M);
		void insertEdgeInstrumentation(Module &M);
		void insertExitPointInstrumentation(Module &M);


	};


}


