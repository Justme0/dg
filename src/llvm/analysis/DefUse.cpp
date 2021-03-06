#include <map>

// ignore unused parameters in LLVM libraries
#if (__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include <llvm/IR/Value.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Support/raw_ostream.h>

#if (__clang__)
#pragma clang diagnostic pop // ignore -Wunused-parameter
#else
#pragma GCC diagnostic pop
#endif

#include "llvm/LLVMNode.h"
#include "llvm/LLVMDependenceGraph.h"
#include "llvm/llvm-utils.h"

#include "llvm/analysis/PointsTo/PointsTo.h"
#include "ReachingDefinitions/ReachingDefinitions.h"
#include "DefUse.h"

#include "analysis/PointsTo/PointerSubgraph.h"
#include "analysis/DFS.h"

using dg::analysis::rd::LLVMReachingDefinitions;
using dg::analysis::rd::RDNode;

using namespace llvm;

/// --------------------------------------------------
//   Add def-use edges
/// --------------------------------------------------
namespace dg {

static void handleInstruction(const Instruction *Inst, LLVMNode *node)
{
    LLVMDependenceGraph *dg = node->getDG();

    for (auto I = Inst->op_begin(), E = Inst->op_end(); I != E; ++I) {
        LLVMNode *op = dg->getNode(*I);
        if (op)
            op->addDataDependence(node);
    }
}

static void addReturnEdge(LLVMNode *callNode, LLVMDependenceGraph *subgraph)
{
    // FIXME we may loose some accuracy here and
    // this edges causes that we'll go into subprocedure
    // even with summary edges
    if (!callNode->isVoidTy())
        subgraph->getExit()->addDataDependence(callNode);
}

LLVMDefUseAnalysis::LLVMDefUseAnalysis(LLVMDependenceGraph *dg,
                                       LLVMReachingDefinitions *rd,
                                       LLVMPointerAnalysis *pta)
    : analysis::DataFlowAnalysis<LLVMNode>(dg->getEntryBB(),
                                           analysis::DATAFLOW_INTERPROCEDURAL),
      dg(dg), RD(rd), PTA(pta), DL(new DataLayout(dg->getModule()))
{
    assert(PTA && "Need points-to information");
    assert(RD && "Need reaching definitions");
}

void LLVMDefUseAnalysis::handleInlineAsm(LLVMNode *callNode)
{
    CallInst *CI = cast<CallInst>(callNode->getValue());
    LLVMDependenceGraph *dg = callNode->getDG();

    // the last operand is the asm itself, so iterate only to e - 1
    for (unsigned i = 0, e = CI->getNumOperands(); i < e - 1; ++i) {
        Value *opVal = CI->getOperand(i);
        if (!opVal->getType()->isPointerTy())
            continue;

        LLVMNode *opNode = dg->getNode(opVal->stripInBoundsOffsets());
        if (!opNode) {
            // FIXME: ConstantExpr
            llvmutils::printerr("WARN: unhandled inline asm operand: ", opVal);
            continue;
        }

        assert(opNode && "Do not have an operand for inline asm");

        // if nothing else, this call at least uses the operands
        opNode->addDataDependence(callNode);
    }
}

void LLVMDefUseAnalysis::handleIntrinsicCall(LLVMNode *callNode,
                                             CallInst *CI)
{
    IntrinsicInst *I = cast<IntrinsicInst>(CI);
    Value *dest, *src = nullptr;

    switch (I->getIntrinsicID())
    {
        case Intrinsic::memmove:
        case Intrinsic::memcpy:
            dest = I->getOperand(0);
            src = I->getOperand(1);
            break;
        case Intrinsic::memset:
            dest = I->getOperand(0);
            break;
        case Intrinsic::vastart:
            dest = I->getOperand(0);
            break;
        default:
            //assert(0 && "DEF-USE: Unhandled intrinsic call");
            //handleUndefinedCall(callNode, CI);
            return;
    }

    // we must have dest set
    assert(dest);

    // these functions touch the memory of the pointers
    addDataDependence(callNode, CI, dest, UNKNOWN_OFFSET /* FIXME */);

    if (src)
        addDataDependence(callNode, CI, src, UNKNOWN_OFFSET /* FIXME */);
}

void LLVMDefUseAnalysis::handleCallInst(LLVMNode *node)
{
    CallInst *CI = cast<CallInst>(node->getKey());

    if (CI->isInlineAsm()) {
        handleInlineAsm(node);
        return;
    }

    Function *func
        = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts());
    if (func) {
        if (func->isIntrinsic() && !isa<DbgValueInst>(CI)) {
            handleIntrinsicCall(node, CI);
            return;
        }

        // for realloc, we need to make it data dependent on the
        // memory it reallocates, since that is the memory it copies
        if (strcmp(func->getName().data(), "realloc") == 0)
            addDataDependence(node, CI, CI->getOperand(0), UNKNOWN_OFFSET /* FIXME */);
    }

    /*
    if (func && func->size() == 0) {
        handleUndefinedCall(node);
        return;
    }
    */

    // add edges from the return nodes of subprocedure
    // to the call (if the call returns something)
    for (LLVMDependenceGraph *subgraph : node->getSubgraphs())
        addReturnEdge(node, subgraph);
}

// Add data dependence edges from all memory location that may write
// to memory pointed by 'pts' to 'node'
void LLVMDefUseAnalysis::addUnknownDataDependence(LLVMNode *node, PSNode *pts)
{
    // iterate over all nodes from ReachingDefinitions Subgraph. It is faster than
    // going over all llvm nodes and querying the pointer to analysis
    for (auto& it : RD->getNodesMap()) {
        RDNode *rdnode = it.second;

        // only STORE may be a definition site
        if (rdnode->getType() != analysis::rd::STORE)
            continue;

        llvm::Value *rdVal = rdnode->getUserData<llvm::Value>();
        // artificial node?
        if (!rdVal)
            continue;

        // does this store define some value that is in pts?
        for (const analysis::rd::DefSite& ds : rdnode->getDefines()) {
            llvm::Value *llvmVal = ds.target->getUserData<llvm::Value>();
            // is this an artificial node?
            if (!llvmVal)
                continue;

            // if these two sets have an over-lap, we must add the data dependence
            for (const auto& ptr : pts->pointsTo)
                if (ptr.target->getUserData<llvm::Value>() == llvmVal) {
                    addDataDependence(node, rdVal);
            }
        }
    }
}

void LLVMDefUseAnalysis::addDataDependence(LLVMNode *node, llvm::Value *rdval)
{
    LLVMNode *rdnode = dg->getNode(rdval);
    if (!rdnode) {
        // that means that the value is not from this graph.
        // We need to add interprocedural edge
        llvm::Function *F
            = llvm::cast<llvm::Instruction>(rdval)->getParent()->getParent();
        LLVMNode *entryNode = dg->getGlobalNode(F);
        assert(entryNode && "Don't have built function");

        // get the graph where the node lives
        LLVMDependenceGraph *graph = entryNode->getDG();
        assert(graph != dg && "Cannot find a node");
        rdnode = graph->getNode(rdval);
        if (!rdnode) {
            llvmutils::printerr("ERROR: DG has not val: ", rdval);
            return;
        }
    }

    assert(rdnode);
    rdnode->addDataDependence(node);
}


void LLVMDefUseAnalysis::addDataDependence(LLVMNode *node, RDNode *rd)
{
    llvm::Value *rdval = rd->getUserData<llvm::Value>();
    assert(rdval && "RDNode has not set the coresponding value");
    addDataDependence(node, rdval);
}

// \param mem   current reaching definitions point
void LLVMDefUseAnalysis::addDataDependence(LLVMNode *node, PSNode *pts,
                                           RDNode *mem, uint64_t size)
{
    using namespace dg::analysis;

    for (const pta::Pointer& ptr : pts->pointsTo) {
        if (!ptr.isValid())
            continue;

        llvm::Value *llvmVal = ptr.target->getUserData<llvm::Value>();
        assert(llvmVal && "Don't have Value in PSNode");

        RDNode *val = RD->getNode(llvmVal);
        if(!val) {
            llvmutils::printerr("Don't have mapping:\n  ", llvmVal);
            continue;
        }

        std::set<RDNode *> defs;
        // Get even reaching definitions for UNKNOWN_MEMORY.
        // Since those can be ours definitions, we must add them always
        mem->getReachingDefinitions(rd::UNKNOWN_MEMORY, UNKNOWN_OFFSET, UNKNOWN_OFFSET, defs);
        if (!defs.empty()) {
            for (RDNode *rd : defs) {
                assert(!rd->isUnknown() && "Unknown memory defined at unknown location?");
                addDataDependence(node, rd);
            }

            defs.clear();
        }

        mem->getReachingDefinitions(val, ptr.offset, size, defs);
        if (defs.empty()) {
            llvm::GlobalVariable *GV
                = llvm::dyn_cast<llvm::GlobalVariable>(llvmVal);
            if (!GV || !GV->hasInitializer()) {
                static std::set<const llvm::Value *> reported;
                if (reported.insert(llvmVal).second) {
                    llvm::errs() << "No reaching definition for: " << *llvmVal
                                 << " off: " << *ptr.offset << "\n";
                }
            }

            continue;
        }

        // add data dependence
        for (RDNode *rd : defs) {
            if (rd->isUnknown()) {
                // we don't know what definitions reach this node,
                // se we must add data dependence to all possible
                // write to this memory
                addUnknownDataDependence(node, pts);

                // we can bail out, since we have added all
                break;
            }

            addDataDependence(node, rd);
        }
    }
}

void LLVMDefUseAnalysis::addDataDependence(LLVMNode *node,
                                           const llvm::Value *where, /* in CFG */
                                           const llvm::Value *ptrOp,
                                           uint64_t size)
{
    using namespace dg::analysis;

    // get points-to information for the operand
    pta::PSNode *pts = PTA->getPointsTo(ptrOp);
    //assert(pts && "Don't have points-to information for LoadInst");
    if (!pts) {
        llvmutils::printerr("ERROR: No points-to: ", ptrOp);
        return;
    }

    // get the node from reaching definition where we have
    // all the reaching definitions
    RDNode *mem = RD->getMapping(where);
    if(!mem) {
        llvmutils::printerr("ERROR: Don't have mapping: ", where);
        return;
    }

    // take every memory the load inst can use and get the
    // reaching definition
    addDataDependence(node, pts, mem, size);
}

static uint64_t getAllocatedSize(llvm::Type *Ty, const llvm::DataLayout *DL)
{
    // Type can be i8 *null or similar
    if (!Ty->isSized())
            return UNKNOWN_OFFSET;

    return DL->getTypeAllocSize(Ty);
}

void LLVMDefUseAnalysis::handleLoadInst(llvm::LoadInst *Inst, LLVMNode *node)
{
    using namespace dg::analysis;

    uint64_t size = getAllocatedSize(Inst->getType(), DL);
    addDataDependence(node, Inst, Inst->getPointerOperand(), size);
}

bool LLVMDefUseAnalysis::runOnNode(LLVMNode *node, LLVMNode *prev)
{
    Value *val = node->getKey();
    (void) prev;

    if (LoadInst *Inst = dyn_cast<LoadInst>(val)) {
        handleLoadInst(Inst, node);
    } else if (isa<CallInst>(val)) {
        handleCallInst(node);
    /*} else if (StoreInst *Inst = dyn_cast<StoreInst>(val)) {
        handleStoreInst(Inst, node);*/
    }

    /* just add direct def-use edges to every instruction */
    if (Instruction *Inst = dyn_cast<Instruction>(val))
        handleInstruction(Inst, node);

    // we will run only once
    return false;
}

} // namespace dg
