#ifndef _LLVM_DG_PSS_H_
#define _LLVM_DG_PSS_H_

#include <map>

//#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/CFG.h>
#include <llvm/IR/Function.h>

#include "analysis/ControlExpression/ControlExpression.h"

namespace dg {

class LLVMControlExpression
{
    llvm::Function *F;
    ControlExpressionNode<llvm::BasicBlock *> *root;
    ControlExpression<llvm::BasicBlock *> *CE;

public:
    LLVMControlExpression(llvm::Function *func)
        :F(func), root(nullptr), CE(nullptr) {}

    ControlExpressionNode<llvm::BasicBlock *> *getRoot() const
    {
        return root;
    }

        //XXX move back to buildCFG
        std::map<llvm::BasicBlock *,
                 ControlExpressionNode<llvm::BasicBlock *> *> blocks;
    // just copy the llvm CFG
    void buildCFG()
    {

        // create all blocks
        for (llvm::BasicBlock& block : *F) {
            blocks[&block]
                = new ControlExpressionNode<llvm::BasicBlock *>(&block);
        }

        // add edges between blocks
        for (auto it : blocks) {
            llvm::BasicBlock *BB = it.first;
            ControlExpressionNode<llvm::BasicBlock *> *node = it.second;

            for (llvm::succ_iterator S = succ_begin(BB), SE = succ_end(BB);
                 S != SE; ++S) {
                ControlExpressionNode<llvm::BasicBlock *> *succ = blocks[*S];
                assert(succ && "Missing basic block");

                node->addSuccessor(succ);
            }
        }

        root = blocks[&*(F->getBasicBlockList().begin())];
    }

    ControlExpression<llvm::BasicBlock *> *compute()
    {
        if (CE)
            return CE;

        CE = new ControlExpression<llvm::BasicBlock *>();
        auto C2 = new ControlExpression<llvm::BasicBlock *>(SYMBOLS);
        for (auto it : blocks) {
            llvm::BasicBlock *BB = it.first;
            C2->addElement(BB);
        }

        auto C1 = new ControlExpression<llvm::BasicBlock *>(STAR);
        auto C3 = new ControlExpression<llvm::BasicBlock *>(SYMBOLS);
        for (auto it : blocks) {
            llvm::BasicBlock *BB = it.first;
            C3->addElement(BB);
        }

        C1->addElement(C3);
        CE->addElement(C2);
        CE->addElement(C1);

        return CE;
    }
};

} // namespace dg

#endif
