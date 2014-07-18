/*
 * FunctionPrinter.h
 *
 *  Created on: Jul 17, 2014
 *      Author: raphael
 */

#ifndef FUNCTIONPRINTER_H_
#define FUNCTIONPRINTER_H_

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"



using namespace llvm;

namespace llvm {

	class FunctionPrinter: public FunctionPass {
	public:
		static char ID;
		FunctionPrinter(): FunctionPass(ID) {};
		virtual ~FunctionPrinter(){};

		void getAnalysisUsage(AnalysisUsage& AU) const;

		bool runOnFunction(Function &F);
	};

}

#endif /* FUNCTIONPRINTER_H_ */
