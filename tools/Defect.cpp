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

    throw std::invalid_argument("\"" + criterion + "\" is an invalid slicing criterion!");
}

Defect::Criterion Defect::getCriterion() const {
    // Lazy evaluate criterion.
    std::call_once(_firstFlag, &Defect::setCriterion, const_cast<Defect *>(this));

    return _criterion;
}

void MemoryLeak::visitCallInst(const CallInst &CI) {
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

void FileIO::visitCallInst(const CallInst &CI) {
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

void DivideByZero::visitBinaryOperator(const BinaryOperator &BO) {
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

void IntegerOverflow::visitTruncInst(const TruncInst &TI) {
    _criterion.push_back(&TI);
}

/// e.g. *p;
/// %p = alloca i32*, align 8
/// %1 = load i32*, i32** %p, align 8
/// %2 = load i32, i32* %1, align 4 (LI)
void PointerDereference::visitLoadInst(const LoadInst &LI) {
    if (LoadInst::classof(LI.getPointerOperand())) {
        _criterion.push_back(&LI);
    }
}

/// e.g. *p = 2;
/// %p = alloca i32*, align 8
/// %1 = load i32*, i32** %p, align 8
/// store i32 2, i32* %1, align 4 (SI)
void PointerDereference::visitStoreInst(const StoreInst &SI) {
    if (LoadInst::classof(SI.getPointerOperand())) {
        _criterion.push_back(&SI);
    }
}

/// e.g. arr[1];
/// %arr = alloca [3 x i32], align 4
/// %arrayidx3 = getelementptr inbounds [3 x i32], [3 x i32]* %arr, i64 0, i64 1
/// %2 = load i32, i32* %arrayidx3, align 4 (LI)
void BufferOverflow::visitLoadInst(const LoadInst &LI) {
    if(GetElementPtrInst::classof(LI.getPointerOperand())) {
        _criterion.push_back(&LI);
    }
}

/// e.g. arr[1] = 3;
/// %arr = alloca [3 x i32], align 4
/// %arrayidx = getelementptr inbounds [3 x i32], [3 x i32]* %arr, i64 0, i64 1
/// store i32 3, i32* %arrayidx, align 4 (SI)
void BufferOverflow::visitStoreInst(const StoreInst &SI) {
    if (GetElementPtrInst::classof(SI.getPointerOperand())) {
        _criterion.push_back(&SI);
    }
}

/// e.g. int a; a;
/// %a = alloca i32, align 4
/// %0 = load i32, i32* %a, align 4 (LI)
void UninitializedVariable::visitLoadInst(const LoadInst &LI) {
    if (AllocaInst::classof(LI.getPointerOperand())) {
        _criterion.push_back(&LI);
    }
}

/// e.g. int a; int *p = &a;
/// %a = alloca i32, align 4
/// %p = alloca i32*, align 8
/// store i32* %a, i32** %p, align 8 (SI)
void UninitializedVariable::visitStoreInst(const StoreInst &SI) {
    if (AllocaInst::classof(SI.getValueOperand())) {
        _criterion.push_back(&SI);
    }
}

/// e.g.
/// int *g_p;
/// void global() {
///   int a = 2;
///   g_p = &a;
/// }
/// %a = alloca i32, align 4
/// store i32 2, i32* %a, align 4
/// store i32* %a, i32** @g_p, align 8 (SI)
/// ret void
///
/// void argument(int **pp) {
///   int a = 3;
///   *pp = &a;
/// }
/// %pp.addr = alloca i32**, align 8
/// %a = alloca i32, align 4
/// store i32** %pp, i32*** %pp.addr, align 8
/// store i32 3, i32* %a, align 4
/// %0 = load i32**, i32*** %pp.addr, align 8
/// store i32* %a, i32** %0, align 8 (SI)
/// ret void
void StackAddressEscape::visitStoreInst(const llvm::StoreInst &SI) {
    // Conservative analysis. If a pointer is stored, the instruction is
    // added to criterion.
    if (SI.getValueOperand()->getType()->isPointerTy()) {
        _criterion.push_back(&SI);
    }
}

/// e.g.
/// int *local() {
///   int a[33] = {3, 2};
///   return a;
/// }
///
/// %a = alloca [33 x i32], align 16
/// %0 = bitcast [33 x i32]* %a to i8*
/// call void @llvm.memset.p0i8.i64(i8* %0, i8 0, i64 132, i32 16, i1 false)
/// %1 = bitcast i8* %0 to [33 x i32]*
/// %2 = getelementptr [33 x i32], [33 x i32]* %1, i32 0, i32 0
/// store i32 3, i32* %2
/// %3 = getelementptr [33 x i32], [33 x i32]* %1, i32 0, i32 1
/// store i32 2, i32* %3
/// %arraydecay = getelementptr inbounds [33 x i32], [33 x i32]* %a, i32 0, i32 0
/// ret i32* %arraydecay (RI)
void StackAddressEscape::visitReturnInst(const llvm::ReturnInst &RI) {
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
