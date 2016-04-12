#include <assert.h>
#include <cstdio>

#include <map>

#ifndef HAVE_LLVM
#error "This code needs LLVM enabled"
#endif

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/IRReader/IRReader.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include "analysis/ControlExpression/ControlExpression.h"
#include "llvm/ControlExpression.h"

#include "Utils.h"

using namespace dg;
using llvm::errs;
std::map<llvm::BasicBlock *,
         std::pair<char, unsigned int>> names;

static void dumpCE(ControlExpression<llvm::BasicBlock *> *CE)
{
    static char lastChar = 'A';
    static unsigned int lastNum = 0;
    int n = 0;

    switch(CE->getType()) {
        case PLUS:
            llvm::errs() << "(";

            n = 0;
            for (auto elem : CE->getElements()) {
                if (n++ > 0)
                    llvm::errs() << " + ";

                dumpCE(elem.subexpression);
            }

            llvm::errs() << ")*";
            break;
        case STAR:
            llvm::errs() << "(";
            for (auto elem : CE->getElements())
                dumpCE(elem.subexpression);
            llvm::errs() << ")*";
            break;
        case SYMBOLS:
            for (auto elem : CE->getElements()) {
                std::pair<char, unsigned int>& name = names[elem.label];
                if (name.first == 0 && name.second == 0) {
                    name.first = lastChar;
                    name.second = ++lastNum;

                    // XXX overflow of chars?
                    if (lastNum == 9) {
                        ++lastChar;
                        lastNum = 0;
                    }
                }

                llvm::errs() << name.first << name.second;
            }

            break;
        case SEQUENCE:
            for (auto elem : CE->getElements())
                dumpCE(elem.subexpression);
            break;
        default:
            assert(0 && "Unknown type");
    }
}

int main(int argc, char *argv[])
{
    llvm::LLVMContext context;
    llvm::SMDiagnostic SMD;
    llvm::Module *M;
    const char *module = nullptr;

    module = argv[1];

    if (argc != 2) {
        errs() << "Usage: llvm-control-expression IR_module\n";
        return 1;
    }

    M = llvm::ParseIRFile(module, SMD, context);
    if (!M) {
        SMD.print(argv[0], errs());
        return 1;
    }

    debug::TimeMeasure tm;
    for (llvm::Function& F : *M) {
        LLVMControlExpression *ce = new LLVMControlExpression(&F);
        ce->buildCFG();

        dumpCE(ce->compute());
        llvm::errs() << "\n";

        delete ce;
    }

    return 0;
}
