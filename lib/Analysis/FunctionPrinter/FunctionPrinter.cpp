/*
 * FunctionPrinter.cpp
 *
 *  Created on: Jul 17, 2014
 *      Author: raphael
 */

#include "FunctionPrinter.h"

using namespace llvm;


cl::opt<std::string> DumpFilename("function",
                                  cl::desc("Specify function name"),
                                  cl::value_desc("functionName"),
                                  cl::init("main"),
                                  cl::NotHidden);



void FunctionPrinter::getAnalysisUsage(AnalysisUsage& AU) const {

	/*
	 * We won't modify anything, so we shall tell this to LLVM
	 * to allow LLVM to not invalidate other analyses.
	 */
	AU.setPreservesAll();
}


bool FunctionPrinter::runOnFunction(Function &F) {

	//We only want to print the selected function
	if (F.getName().compare(DumpFilename)) return false;

	for (Function::iterator BBit = F.begin(), BBend = F.end(); BBit != BBend; BBit++) {

		errs() << *BBit << "\n\n";
	}


	//We don't modify anything, so we must return false;
	return false;
}

char FunctionPrinter::ID = 0;
static RegisterPass<FunctionPrinter> Y("function-printer", "Function printer pass");
