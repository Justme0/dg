#include "Defect.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <stdexcept>

using namespace llvm;

namespace crtr {

std::shared_ptr<Defect> Defect::create(Module *M,
                                       const std::string &criterion) {
    if (criterion == "ml") { return std::make_shared<MemoryLeak>(M); }
    if (criterion == "fio") { return std::make_shared<FileIO>(M); }
    if (criterion == "dbz") { return std::make_shared<DivideByZero>(M); }
    if (criterion == "io") { return std::make_shared<IntegerOverflow>(M); }
    if (criterion == "pd") { return std::make_shared<PointerDereference>(M); }
    if (criterion == "bo") { return std::make_shared<BufferOverflow>(M); }
    if (criterion == "uv") { return std::make_shared<UninitializedVariable>(M); }
    if (criterion == "sae") { return std::make_shared<StackAddressEscape>(M); }

    throw std::invalid_argument("\"" + criterion + "\" is invalid slicing criterion!");
}

Defect::Criterion Defect::getCriterion() const {
    // Lazy evaluate criterion.
    std::call_once(_firstFlag, &Defect::setCriterion, const_cast<Defect *>(this));

    return _criterion;
}

void MemoryLeak::visitCallInst(CallInst &CI) {
    static const std::vector<std::string> candidates = {
        "malloc",
        "realloc",
        "calloc",
        "free",
    };

    if (std::find(candidates.begin(), candidates.end(),
                  CI.getCalledFunction()->stripPointerCasts()->getName()) !=
        candidates.end()) {
        _criterion.push_back(&CI);
    }
}

void FileIO::visitCallInst(CallInst &CI) {
    static const std::vector<std::string> candidates = {
        "fopen",
        "freopen",
        "fclose",
        "open",
        "openat",
        "creat",
    };

    if (std::find(candidates.begin(), candidates.end(),
                  CI.getCalledFunction()->stripPointerCasts()->getName()) !=
        candidates.end()) {
        _criterion.push_back(&CI);
    }
}

void DivideByZero::visitBinaryOperator(BinaryOperator &BO) {
    static const std::vector<Instruction::BinaryOps> candidates = {
        Instruction::UDiv,
        Instruction::SDiv,
        Instruction::FDiv,
        Instruction::URem,
        Instruction::SRem,
        Instruction::FRem,
    };

    if (std::find(candidates.begin(), candidates.end(), BO.getOpcode()) !=
        candidates.end()) {
        _criterion.push_back(&BO);
    }
}

void IntegerOverflow::visitTruncInst(TruncInst &TI) {
    _criterion.push_back(&TI);
}

/// e.g. *p;
/// %p = alloca i32*, align 8
/// %1 = load i32*, i32** %p, align 8
/// %2 = load i32, i32* %1, align 4 (LI)
void PointerDereference::visitLoadInst(LoadInst &LI) {
    if (LoadInst::classof(LI.getPointerOperand())) {
        _criterion.push_back(&LI);
    }
}

/// e.g. *p = 2;
/// %p = alloca i32*, align 8
/// %1 = load i32*, i32** %p, align 8
/// store i32 2, i32* %1, align 4 (SI)
void PointerDereference::visitStoreInst(StoreInst &SI) {
    if (LoadInst::classof(SI.getPointerOperand())) {
        _criterion.push_back(&SI);
    }
}

/// e.g. arr[1] = 3;
/// %arr = alloca [3 x i32], align 4
/// %arrayidx = getelementptr inbounds [3 x i32], [3 x i32]* %arr, i64 0, i64 1
/// store i32 3, i32* %arrayidx, align 4
void BufferOverflow::visitLoadInst(LoadInst &LI) {
    if(GetElementPtrInst::classof(LI.getPointerOperand())) {
        _criterion.push_back(&LI);
    }
}

/// e.g. arr[1];
/// %arr = alloca [3 x i32], align 4
/// %arrayidx3 = getelementptr inbounds [3 x i32], [3 x i32]* %arr, i64 0, i64 1
/// %2 = load i32, i32* %arrayidx3, align 4
void BufferOverflow::visitStoreInst(StoreInst &SI) {
    if (GetElementPtrInst::classof(SI.getPointerOperand())) {
        _criterion.push_back(&SI);
    }
}

/// e.g. int a; a;
/// %a = alloca i32, align 4
/// %0 = load i32, i32* %a, align 4
void UninitializedVariable::visitLoadInst(LoadInst &LI) {
    if (AllocaInst::classof(LI.getPointerOperand())) {
        _criterion.push_back(&LI);
    }
}

/// e.g. int a; int *p = &a;
/// %a = alloca i32, align 4
/// %p = alloca i32*, align 8
/// store i32* %a, i32** %p, align 8
void UninitializedVariable::visitStoreInst(StoreInst &SI) {
    if (AllocaInst::classof(SI.getValueOperand())) {
        _criterion.push_back(&SI);
    }
}

/// e.g.
/// double *foo() {
///   double a;
///   double *p = &a;
///   return p;
/// }
///
/// %a = alloca double, align 8
/// %p = alloca double*, align 8
/// store double* %a, double** %p, align 8, !dbg !28
/// %0 = load double*, double** %p, align 8, !dbg !29
/// ret double* %0, !dbg !30
void StackAddressEscape::visitStoreInst(llvm::StoreInst &SI) {
    // Conservative analysis. If a pointer is stored, the instruction is
    // added to criterion.
    if (SI.getValueOperand()->getType()->isPointerTy()) {
        _criterion.push_back(&SI);
    }
}

void StackAddressEscape::visitReturnInst(llvm::ReturnInst &RI) {
    Value *Ret = RI.getReturnValue();
    if (Ret == nullptr) {
        assert(RI.getFunction()->getReturnType()->isVoidTy());
        return ; // void function
    }

    // Conservative analysis. If the function returns a pointer,
    // the return instruction is added to criterion.
    if (Ret->getType()->isPointerTy()) {
        assert(RI.getFunction()->getReturnType()->isPointerTy());
        _criterion.push_back(&RI);
    }
}

} // end namespace crtr
