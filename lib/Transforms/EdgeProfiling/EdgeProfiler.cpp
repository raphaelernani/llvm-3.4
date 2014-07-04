#include "EdgeProfiler.h"

using namespace std;
using namespace llvm;

bool llvm::EdgeProfiler::runOnModule(Module &M){

	printBasicBlocks(M);
	printEdges(M);


	insertDeclarations(M);
	insertEdgeInstrumentation(M);
	insertExitPointInstrumentation(M);


	return true;
}

void llvm::EdgeProfiler::printBasicBlocks(Module& M) {

	std::string ErrorInfo;
	std::string FileName = M.getModuleIdentifier() + ".BasicBlocks.txt";

    raw_fd_ostream File(FileName.c_str(), ErrorInfo);

    if (!ErrorInfo.empty()) {
            errs() << "Error opening file " << FileName
                            << " for writing! Error Info: " << ErrorInfo << " \n";
            return;
    }


	//iterate through the functions and basic blocks
	for(Module::iterator Fit = M.begin(), Fend = M.end(); Fit != Fend; Fit++){

		for(Function::iterator BBit = Fit->begin(), BBend = Fit->end(); BBit != BBend; BBit++){

			BasicBlock* CurrentBB = BBit;

			//ID and Name
			File << (long)CurrentBB << "|" << BBit->getName() << "|";

			//Type of terminator instruction
			TerminatorInst *T = CurrentBB->getTerminator();
			if(isa<BranchInst>(T)) File << "branch";
			else if(isa<SwitchInst>(T)) File << "switch";
			else if(isa<InvokeInst>(T)) File << "invoke";
			else File << "other";

			File << "\n";

		}
	}

	File.close();

}

void llvm::EdgeProfiler::printEdges(Module& M) {

	std::string ErrorInfo;
	std::string FileName = M.getModuleIdentifier() + ".CFGEdges.txt";

    raw_fd_ostream File(FileName.c_str(), ErrorInfo);

    if (!ErrorInfo.empty()) {
            errs() << "Error opening file " << FileName
                            << " for writing! Error Info: " << ErrorInfo << " \n";
            return;
    }


	//iterate through the functions and basic blocks
	for(Module::iterator Fit = M.begin(), Fend = M.end(); Fit != Fend; Fit++){

		for(Function::iterator BBit = Fit->begin(), BBend = Fit->end(); BBit != BBend; BBit++){

			BasicBlock* CurrentBB = BBit;

			//iterate through the successors of the basicblocks in the CFG
			for(succ_iterator succ = succ_begin(CurrentBB), succEnd = succ_end(CurrentBB); succ != succEnd; succ++){

				BasicBlock* succBB = *succ;
				File << (long)CurrentBB << "|" << (long)succBB << "\n";

			}
		}
	}

	File.close();

}

void llvm::EdgeProfiler::insertEdgeInstrumentation(Module& M) {

	//iterate through the functions and basic blocks
	for(Module::iterator Fit = M.begin(), Fend = M.end(); Fit != Fend; Fit++){

		for(Function::iterator BBit = Fit->begin(), BBend = Fit->end(); BBit != BBend; BBit++){

			BasicBlock* CurrentBB = BBit;

			Instruction* InsertionPt = CurrentBB->getFirstInsertionPt();

			IRBuilder<> Builder(InsertionPt);

			Constant* CurrentBB_Const = Builder.getInt64((long)CurrentBB);


			//FIXME: Probably there is a better place to initialize this variable.
			if(!moduleIdentifierStr) {
				moduleIdentifierStr = Builder.CreateGlobalStringPtr(M.getModuleIdentifier(), "moduleIdentifierStr");
			}


			//Insert code to count how many times an edge is visited during the execution
			PHINode* IncomingBB = Builder.CreatePHI(Type::getInt64Ty(M.getContext()), 0, "IncomingBB");

			//iterate through the successors of the basicblocks in the CFG
			for(pred_iterator pred = pred_begin(CurrentBB), predEnd = pred_end(CurrentBB); pred != predEnd; pred++){

				BasicBlock* predBB = *pred;
				Constant* predBB_Const = Builder.getInt64((long)predBB);
				IncomingBB->addIncoming(predBB_Const, predBB);

			}

			if(IncomingBB->getNumIncomingValues() > 0){

				std::vector<Value*> args;
				args.push_back(IncomingBB);
				args.push_back(CurrentBB_Const);
				llvm::ArrayRef<llvm::Value *> arrayArgs(args);
				Builder.CreateCall(countVisitedEdge, arrayArgs, "");

			} else {
				IncomingBB->eraseFromParent();
			}

			//Insert code to count how many times a basic block is visited during the execution
			std::vector<Value*> args;
			args.push_back(CurrentBB_Const);
			llvm::ArrayRef<llvm::Value *> arrayArgs(args);
			Builder.CreateCall(countVisitedBlock, arrayArgs, "");

		}
	}
}

void llvm::EdgeProfiler::insertDeclarations(Module& M) {


	/*
	 * We will insert calls to functions in specific
	 * points of the program.
	 *
	 * Before doing that, we must declare the functions.
	 * Here we have our declarations.
	 */

	Type* tyVoid = Type::getVoidTy(M.getContext());

	std::vector<Type*> args;
	args.push_back(Type::getInt64Ty(M.getContext()));   // BasicBlock identifier
	llvm::ArrayRef<Type*> arrayArgs(args);
	FunctionType *T1 = FunctionType::get(tyVoid, arrayArgs, true);
	countVisitedBlock = M.getOrInsertFunction("countVisitedBlock", T1);

	std::vector<Type*> args2;
	args2.push_back(Type::getInt64Ty(M.getContext()));   // Origin BasicBlock identifier
	args2.push_back(Type::getInt64Ty(M.getContext()));   // Destination BasicBlock identifier
	llvm::ArrayRef<Type*> arrayArgs2(args2);
	FunctionType *T2 = FunctionType::get(tyVoid, arrayArgs2, true);
	countVisitedEdge = M.getOrInsertFunction("countVisitedEdge", T2);

	std::vector<Type*> args3;
	args3.push_back(Type::getInt8PtrTy(M.getContext()));
	llvm::ArrayRef<Type*> arrayArgs3(args3);
	FunctionType *T3 = FunctionType::get(tyVoid, arrayArgs3, true);
	flushProfilingData = M.getOrInsertFunction("flushProfilingData", T3);

}

void llvm::EdgeProfiler::insertExitPointInstrumentation(Module& M) {

	// Here we insert code to flush the instrumentation data immediately before the program stop.
	// We only cover exits aborts and return in the main function
	//
	// Notice that this only work for executions that stop normally.
	// Programs aborted by the OS (e.g. segmentation fault) are not covered.
	ExitInfo& eI = getAnalysis<ExitInfo>();
	for(std::set<Instruction*>::iterator Iit = eI.exitPoints.begin(), Iend = eI.exitPoints.end(); Iit != Iend; Iit++){

		Instruction* I = *Iit;

		IRBuilder<> Builder(I);

		std::vector<Value*> args;
		args.push_back(moduleIdentifierStr);
		llvm::ArrayRef<llvm::Value *> arrayArgs(args);
		Builder.CreateCall(flushProfilingData, arrayArgs, "");

	}

}


char EdgeProfiler::ID = 0;
static RegisterPass<EdgeProfiler> X("edge-profiler", "Edge Profiler Pass");
