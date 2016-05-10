#include <assert.h>
#include <cstdio>

#include <set>

#ifndef HAVE_LLVM
#error "This code needs LLVM enabled"
#endif

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Bitcode/ReaderWriter.h>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include "llvm/analysis/PointsTo.h"
#include "analysis/PointsTo/PointsToFlowInsensitive.h"
#include "analysis/PointsTo/PointsToFlowSensitive.h"
#include "analysis/PointsTo/Pointer.h"

#include "Utils.h"

using namespace dg;
using namespace dg::analysis::pss;
using llvm::errs;

static bool verbose;

enum PTType {
    FLOW_SENSITIVE = 1,
    FLOW_INSENSITIVE,
};

static std::string
getInstName(const llvm::Value *val)
{
    std::ostringstream ostr;
    llvm::raw_os_ostream ro(ostr);

    assert(val);
    if (llvm::isa<llvm::Function>(val))
        ro << val->getName().data();
    else
        ro << *val;

    ro.flush();

    // break the string if it is too long
    return ostr.str();
}

void printPSSType(enum PSSNodeType type)
{
#define ELEM(t) case t: do {printf("%s", #t); }while(0); break;
    switch(type) {
        ELEM(ALLOC)
        ELEM(DYN_ALLOC)
        ELEM(LOAD)
        ELEM(STORE)
        ELEM(GEP)
        ELEM(PHI)
        ELEM(CAST)
        ELEM(FUNCTION)
        ELEM(CALL)
        ELEM(CALL_FUNCPTR)
        ELEM(CALL_RETURN)
        ELEM(ENTRY)
        ELEM(RETURN)
        ELEM(CONSTANT)
        ELEM(NOOP)
        ELEM(MEMCPY)
        ELEM(NULL_ADDR)
        ELEM(UNKNOWN_MEM)
        default:
            printf("unknown PSS type");
    };
#undef ELEM
}

static void
printName(PSSNode *node, bool dot)
{
    std::string nm;
    const char *name = node->getName();

    if (!name) {
        if (node->isNull())
            name = "null";
        else if (node->isUnknownMemory())
            name = "unknown";
    }

    if (!name) {
        if (!node->getUserData<llvm::Value>()) {
            printPSSType(node->getType());
            if (dot)
                printf(" %p\\n", node);
            else
                printf(" %p\n", node);

            return;
        }

        nm = getInstName(node->getUserData<llvm::Value>());
        name = nm.c_str();
    }

    // escape the " character
    for (int i = 0; name[i] != '\0'; ++i) {
        // crop long names
        if (i >= 70) {
            printf(" ...");
            break;
        }

        if (name[i] == '"')
            putchar('\\');

        putchar(name[i]);
    }
}

static void
dumpMemoryObject(MemoryObject *mo, int ind, bool dot)
{
    for (auto it : mo->pointsTo) {
        for (const Pointer& ptr : it.second) {
            printf("%*s", ind, "");
            if (it.first.isUnknown())
                printf("[UNKNOWN] -> ");
            else
                printf("[%lu] -> ", *it.first);

            printName(ptr.target, dot);

            if (ptr.offset.isUnknown())
                puts(" + UNKNOWN");
            else
                printf(" + %lu\n", *ptr.offset);
        }
    }
}

static void
dumpMemoryMap(PointsToFlowSensitive::MemoryMapT *mm, int ind, bool dot)
{
    for (auto it : *mm) {
        // print the key
        const Pointer& key = it.first;
        printf("%*s", ind, "");

        putchar('[');
        printName(key.target, dot);

        if (key.offset.isUnknown())
            puts(" + UNKNOWN]:");
        else
            printf(" + %lu]:\n", *key.offset);

        for (MemoryObject *mo : it.second)
            dumpMemoryObject(mo, ind + 4, dot);
    }
}

static void
dumpPSSData(PSSNode *n, PTType type, bool dot = false)
{
    if (type == FLOW_INSENSITIVE) {
        MemoryObject *mo = n->getData<MemoryObject>();
        if (!mo)
            return;

        if (dot)
            printf("\\n    Memory: ---\\n");
        else
            printf("    Memory: ---\n");

        dumpMemoryObject(mo, 6, dot);

        if (!dot)
            printf("    -----------\n");
    } else {
        PointsToFlowSensitive::MemoryMapT *mm
            = n->getData<PointsToFlowSensitive::MemoryMapT>();
        if (!mm)
            return;

        if (dot)
            printf("\\n    Memory map: ---\\n");
        else
            printf("    Memory map: ---\n");

        dumpMemoryMap(mm, 6, dot);

        if (!dot)
            printf("    ----------------\n");
    }
}

static void
dumpPSSNode(PSSNode *n, PTType type)
{
    printf("NODE: ");
    printName(n, false);

    if (n->getSize() || n->isHeap() || n->isZeroInitialized())
        printf(" [size: %lu, heap: %u, zeroed: %u]",
               n->getSize(), n->isHeap(), n->isZeroInitialized());

    if (n->pointsTo.empty()) {
        puts(" -- no points-to");
        return;
    } else
        putchar('\n');

    for (const Pointer& ptr : n->pointsTo) {
        printf("    -> ");
        printName(ptr.target, false);
        if (ptr.offset.isUnknown())
            puts(" + UNKNOWN_OFFSET");
        else
            printf(" + %lu\n", *ptr.offset);
    }
    if (verbose) {
        dumpPSSData(n, type);
    }
}

static void
dumpPSSdot(LLVMPointsToAnalysis *pss, PTType type)
{
    std::set<PSSNode *> nodes;
    pss->getNodes(nodes);

    printf("digraph \"Pointer State Subgraph\" {\n");

    /* dump nodes */
    for (PSSNode *node : nodes) {
        printf("\tNODE%p [label=\"", node);
        printName(node, true);

        if (node->getSize() || node->isHeap() || node->isZeroInitialized())
            printf("\\n[size: %lu, heap: %u, zeroed: %u]",
               node->getSize(), node->isHeap(), node->isZeroInitialized());

        for (const Pointer& ptr : node->pointsTo) {
            printf("\\n    -> ");
            printName(ptr.target, true);
            printf(" + ");
            if (ptr.offset.isUnknown())
                printf("UNKNOWN_OFFSET");
            else
                printf("%lu", *ptr.offset);
        }

        if (verbose)
            dumpPSSData(node, type, true /* dot */);

        printf("\"");
        if (node->getType() != STORE) {
            printf(" shape=box");
            if (node->pointsTo.size() == 0)
                printf(" fillcolor=red");
        } else {
            printf(" shape=cds");
        }

        printf("]\n");
    }

    /* dump edges */
    for (PSSNode *node : nodes) {
        for (PSSNode *succ : node->getSuccessors())
            printf("\tNODE%p -> NODE%p [penwidth=2]\n", node, succ);
    }

    printf("}\n");
}

static void
dumpPSS(LLVMPointsToAnalysis *pss, PTType type, bool todot)
{
    assert(pss);

    if (todot)
        dumpPSSdot(pss, type);
    else {
        std::set<PSSNode *> nodes;
        pss->getNodes(nodes);

        for (PSSNode *node : nodes) {
            dumpPSSNode(node, type);
        }
    }
}

int main(int argc, char *argv[])
{
    llvm::Module *M;
    llvm::LLVMContext context;
    llvm::SMDiagnostic SMD;
    bool todot = false;
    const char *module = nullptr;
    PTType type = FLOW_INSENSITIVE;

    // parse options
    for (int i = 1; i < argc; ++i) {
        // run given points-to analysis
        if (strcmp(argv[i], "-pta") == 0) {
            if (strcmp(argv[i+1], "fs") == 0)
                type = FLOW_SENSITIVE;
        } else if (strcmp(argv[i], "-dot") == 0) {
            todot = true;
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = true;
        } else {
            module = argv[i];
        }
    }

    if (!module) {
        errs() << "Usage: % IR_module [output_file]\n";
        return 1;
    }

#if (LLVM_VERSION_MINOR < 5)
    M = llvm::ParseIRFile(module, SMD, context);
#else
    auto _M = llvm::parseIRFile(module, SMD, context);
    // _M is unique pointer, we need to get Module *
    M = &*_M;
#endif

    if (!M) {
        llvm::errs() << "Failed parsing '" << module << "' file:\n";
        SMD.print(argv[0], errs());
        return 1;
    }

    debug::TimeMeasure tm;

    LLVMPointsToAnalysis *PTA;
    if (type == FLOW_INSENSITIVE)
        PTA = new LLVMPointsToAnalysisFI(M);
    else
        PTA = new LLVMPointsToAnalysisFS(M);

    tm.start();
    PTA->run();
    tm.stop();
    tm.report("INFO: Points-to analysis [new] took");
    dumpPSS(PTA, type, todot);

    return 0;
}
