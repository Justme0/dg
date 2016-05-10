#ifndef _LLVM_DG_POINTS_TO_ANALYSIS_H_
#define _LLVM_DG_POINTS_TO_ANALYSIS_H_

#include <llvm/IR/Function.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/raw_ostream.h>

#include "analysis/PointsTo/PSS.h"
#include "analysis/PointsTo/PointsToFlowSensitive.h"
#include "analysis/PointsTo/PointsToFlowInsensitive.h"
#include "llvm/analysis/PSS.h"

namespace dg {

using analysis::pss::PSS;
using analysis::pss::PSSNode;
using analysis::pss::LLVMPSSBuilder;

template <typename PTType>
class LLVMPointsToAnalysisImpl : public PTType
{
    LLVMPSSBuilder *builder;

public:

    LLVMPointsToAnalysisImpl(LLVMPSSBuilder *b)
    :builder(b) {}

    // build new subgraphs on calls via pointer
    virtual bool functionPointerCall(PSSNode *callsite, PSSNode *called)
    {
        // with vararg it may happen that we get pointer that
        // is not to function, so just bail out here in that case
        if (!llvm::isa<llvm::Function>(called->getUserData<llvm::Value>()))
            return false;

        const llvm::Function *F = called->getUserData<llvm::Function>();
        const llvm::CallInst *CI = callsite->getUserData<llvm::CallInst>();

        // incompatible prototypes, skip it...
        if (!F->isVarArg() &&
            CI->getNumArgOperands() != F->arg_size())
            return false;

        std::pair<PSSNode *, PSSNode *> cf = builder->createCallToFunction(CI, F);

        // we got the return site for the call stored as the paired node
        PSSNode *ret = callsite->getPairedNode();
        // ret is a PHI node, so pass the values returned from the
        // procedure call
        ret->addOperand(cf.second);

        // replace the edge from call->ret that we
        // have due to connectivity of the graph until we
        // insert the subgraph
        if (callsite->successorsNum() == 1 &&
            callsite->getSingleSuccessor() == ret) {
            callsite->replaceSingleSuccessor(cf.first);
        } else
            callsite->addSuccessor(cf.first);

        cf.second->addSuccessor(ret);

        return true;
    }

    /*
    virtual bool error(PSSNode *at, const char *msg)
    {
        llvm::Value *val = at->getUserData<llvm::Value>();
        assert(val);

        llvm::errs() << "PTA - error @ " << *val << " : " << msg << "\n";
        return false;
    }

    virtual bool errorEmptyPointsTo(PSSNode *from, PSSNode *to)
    {
        // this is valid even in flow-insensitive points-to
        // because we process the nodes in CFG order
        llvm::Value *val = from->getUserData<llvm::Value>();
        assert(val);
        llvm::Value *val2 = to->getUserData<llvm::Value>();
        assert(val2);

        // due to int2ptr we may have spurious loads
        // in PSS. Don't do anything in this case
        if (!val->getType()->isPointerTy())
            return false;

        llvm::errs() << "PTA - error @ " << *val << " : no points-to >>\n\t"
                     << *val2 << "\n";

        return false;
    }
    */
};

typedef LLVMPointsToAnalysisImpl<analysis::pss::PointsToFlowSensitive> LLVMPTAFlowSensitive;
typedef LLVMPointsToAnalysisImpl<analysis::pss::PointsToFlowInsensitive> LLVMPTAFlowInsensitive;

class LLVMPointsToAnalysis
{
protected:
    PSS *pss;
    LLVMPSSBuilder *builder;

    LLVMPointsToAnalysis() :pss(nullptr), builder(nullptr) {}
public:

    virtual ~LLVMPointsToAnalysis()
    {
        delete pss;
        delete builder;
    }

    PSSNode *getNode(const llvm::Value *val)
    {
        return builder->getNode(val);
    }

    PSSNode *getPointsTo(const llvm::Value *val)
    {
        return builder->getPointsTo(val);
    }

    const std::unordered_map<const llvm::Value *, PSSNode *>&
    getNodesMap() const
    {
        return builder->getNodesMap();
    }

    void getNodes(std::set<PSSNode *>& cont)
    {
        pss->getNodes(cont);
    }

    void run()
    {
        assert(pss);
        assert(builder);

        pss->setRoot(builder->buildLLVMPSS());
        pss->run();
    }
};

class LLVMPointsToAnalysisFS : public LLVMPointsToAnalysis
{
public:
    LLVMPointsToAnalysisFS(const llvm::Module *M)
    {
        builder = new LLVMPSSBuilder(M);
        pss = new LLVMPTAFlowSensitive(builder);
    }
};

class LLVMPointsToAnalysisFI : public LLVMPointsToAnalysis
{

public:
    LLVMPointsToAnalysisFI(const llvm::Module *M)
    {
        builder = new LLVMPSSBuilder(M);
        pss = new LLVMPTAFlowInsensitive(builder);
    }
};

}

#endif // _LLVM_DG_POINTS_TO_ANALYSIS_H_
