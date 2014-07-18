#include "EdgeProfiler.h"

using namespace std;
using namespace llvm;

bool llvm::EdgeProfiler::runOnModule(Module &M){

	printBasicBlocks(M);
	printEdges(M);


	insertDeclarations(M);
	insertInitInstrumentation(M);
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

    int BBid = 0;

	//iterate through the functions and basic blocks
	for(Module::iterator Fit = M.begin(), Fend = M.end(); Fit != Fend; Fit++){

		for(Function::iterator BBit = Fit->begin(), BBend = Fit->end(); BBit != BBend; BBit++){

			BasicBlock* CurrentBB = BBit;

			BBmap[CurrentBB] = BBid;

			//ID and Name
			File << BBid << "|" << BBit->getName() << "|";

			//Type of terminator instruction
			TerminatorInst *T = CurrentBB->getTerminator();
			if(isa<BranchInst>(T)) File << "branch";
			else if(isa<SwitchInst>(T)) File << "switch";
			else if(isa<InvokeInst>(T)) File << "invoke";
			else File << "other";

			File << "\n";

			BBid++;

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

    int EdgeID = 0;


	//iterate through the functions and basic blocks
	for(Module::iterator Fit = M.begin(), Fend = M.end(); Fit != Fend; Fit++){

		for(Function::iterator BBit = Fit->begin(), BBend = Fit->end(); BBit != BBend; BBit++){

			BasicBlock* CurrentBB = BBit;

			//iterate through the predecessors of the basicblocks in the CFG
			for(pred_iterator pred = pred_begin(CurrentBB), predEnd = pred_end(CurrentBB); pred != predEnd; pred++){


				BasicBlock* predBB = *pred;

				if(!EdgeMap.count(make_pair(predBB, CurrentBB))) {

					EdgeMap[make_pair(predBB, CurrentBB)] = EdgeID;

					File << EdgeID << "|" << BBmap[predBB] << "|" << BBmap[CurrentBB] << "\n";

					EdgeID++;

				}

			}

		}
	}

	File.close();

}

void llvm::EdgeProfiler::insertInitInstrumentation(Module &M){

	Function* main = M.getFunction("main");
	if(!main) main = M.getFunction("MAIN__"); //Fortan hack

	assert(main && "Main function not found. Unable to insert instrumentation");

	int numBBs = BBmap.size();
	BBCounter = new GlobalVariable(M, ArrayType::get(Type::getInt64Ty(M.getContext()), numBBs), false, GlobalVariable::CommonLinkage, NULL, "BBCounter");
	BBCounter->setAlignment(16);

	// Constant Definitions
	ConstantAggregateZero* const_array = ConstantAggregateZero::get(ArrayType::get(Type::getInt64Ty(M.getContext()), numBBs));

	// Global Variable Definitions
	BBCounter->setInitializer(const_array);


	int numEdges = EdgeMap.size();
	EdgeCounter = new GlobalVariable(M, ArrayType::get(Type::getInt64Ty(M.getContext()), numEdges), false, GlobalVariable::CommonLinkage, NULL, "EdgeCounter");
	EdgeCounter->setAlignment(16);

	// Constant Definitions
	ConstantAggregateZero* const_array2 = ConstantAggregateZero::get(ArrayType::get(Type::getInt64Ty(M.getContext()), numEdges));

	// Global Variable Definitions
	EdgeCounter->setInitializer(const_array2);


	EntryBlock = &main->getEntryBlock();

	IRBuilder<> Builder(EntryBlock->getFirstInsertionPt());

	Value* BBCounterPtr = Builder.CreateConstGEP2_32(BBCounter, 0, 0, "BBCounterPtr");
	Value* EdgeCounterPtr = Builder.CreateConstGEP2_32(EdgeCounter, 0, 0, "EdgeCounterPtr");
	Value* ConstNumBBs = Builder.getInt32(numBBs);
	Value* ConstNumEdges = Builder.getInt32(numEdges);

	std::vector<Value*> args;
	args.push_back(BBCounterPtr);
	args.push_back(ConstNumBBs);
	args.push_back(EdgeCounterPtr);
	args.push_back(ConstNumEdges);
	llvm::ArrayRef<llvm::Value *> arrayArgs(args);
	Builder.CreateCall(initBBandEdgeCounters, arrayArgs, "");

}

void llvm::EdgeProfiler::insertEdgeInstrumentation(Module& M) {

	Value* constZero = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);
	Value* constOne = ConstantInt::get(Type::getInt64Ty(M.getContext()), 1);

	//iterate through the functions and basic blocks
	for(Module::iterator Fit = M.begin(), Fend = M.end(); Fit != Fend; Fit++){

		for(Function::iterator BBit = Fit->begin(), BBend = Fit->end(); BBit != BBend; BBit++){

			BasicBlock* CurrentBB = BBit;

			Instruction* InsertionPt;

			if (CurrentBB != EntryBlock)
				InsertionPt = CurrentBB->getFirstInsertionPt();
			else
				InsertionPt = CurrentBB->getTerminator();


			IRBuilder<> Builder(InsertionPt);

			int CurrentBBid = BBmap[CurrentBB];


			//Insert code to count how many times an edge is visited during the execution
			PHINode* IncomingBB = Builder.CreatePHI(Type::getInt32Ty(M.getContext()), 0, "IncomingBB");

			//iterate through the successors of the basicblocks in the CFG
			for(pred_iterator pred = pred_begin(CurrentBB), predEnd = pred_end(CurrentBB); pred != predEnd; pred++){

				BasicBlock* predBB = *pred;

				if (IncomingBB->getBasicBlockIndex(predBB) >= 0 ) continue;

				int CurrentEdge = EdgeMap[make_pair(predBB, CurrentBB)];
				Constant* CurrentEdge_Const = Builder.getInt32(CurrentEdge);

				IncomingBB->addIncoming(CurrentEdge_Const, predBB);

			}

			if(IncomingBB->getNumIncomingValues() > 0){

				std::vector<Value*> idxList;
				idxList.push_back(constZero);
				idxList.push_back(IncomingBB);
				llvm::ArrayRef<llvm::Value *> IdxList(idxList);
				Value* CurrentEdge_ptr = Builder.CreateInBoundsGEP(EdgeCounter, IdxList, "CurrentEdgePtr");
				Value* CurrentEdgeCount = Builder.CreateLoad(CurrentEdge_ptr, "CurrentEdgeCount" );
				Value* Inc = Builder.CreateAdd(CurrentEdgeCount, constOne, "CurrentEdgeIncCount" );
				Builder.CreateStore(Inc, CurrentEdge_ptr);

			} else {
				IncomingBB->eraseFromParent();
			}



			//Insert code to count how many times a basic block is visited during the execution
			Value* CurrentBB_ptr = Builder.CreateConstGEP2_32(BBCounter, 0,  CurrentBBid, "CurrentBBptr");
			Value* CurrentBBCount = Builder.CreateLoad(CurrentBB_ptr, "CurrentBBCount" );
			Value* Inc = Builder.CreateAdd(CurrentBBCount, constOne, "CurrentBBIncCount" );
			Builder.CreateStore(Inc, CurrentBB_ptr);

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
	args.push_back(Type::getInt64PtrTy(M.getContext()));   // BasicBlock array
	args.push_back(Type::getInt32Ty(M.getContext()));      // BasicBlock array size
	args.push_back(Type::getInt64PtrTy(M.getContext()));   // CFGEdge array
	args.push_back(Type::getInt32Ty(M.getContext()));      // CFGEdge array size
	llvm::ArrayRef<Type*> arrayArgs(args);

	FunctionType *FT = FunctionType::get(tyVoid, arrayArgs, true);

	initBBandEdgeCounters = M.getOrInsertFunction("initBBandEdgeCounters", FT);
	flushProfilingData = M.getOrInsertFunction("flushProfilingData", FT);

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

		int numBBs = BBmap.size();
		int numEdges = EdgeMap.size();

		Value* BBCounterPtr = Builder.CreateConstGEP2_32(BBCounter, 0, 0, "BBCounterPtr");
		Value* EdgeCounterPtr = Builder.CreateConstGEP2_32(EdgeCounter, 0, 0, "EdgeCounterPtr");
		Value* ConstNumBBs = Builder.getInt32(numBBs);
		Value* ConstNumEdges = Builder.getInt32(numEdges);

		std::vector<Value*> args;
		args.push_back(BBCounterPtr);
		args.push_back(ConstNumBBs);
		args.push_back(EdgeCounterPtr);
		args.push_back(ConstNumEdges);
		llvm::ArrayRef<llvm::Value *> arrayArgs(args);
		Builder.CreateCall(flushProfilingData, arrayArgs, "");

	}

}


char EdgeProfiler::ID = 0;
static RegisterPass<EdgeProfiler> X("edge-profiler", "Edge Profiler Pass");
