#include "Defect.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstVisitor.h"

namespace crtr {

Defect::Criterion Defect::getCriterion() const {
    if (nullptr == _criterion) {
        const_cast<Defect *>(this)->setCriterion();
        assert(nullptr != _criterion &&
               "Criterion shouldn't be null after setCriterion()!");
    }
    return *_criterion;
}

void MemoryLeak::visitCallInst(llvm::CallInst &CI) {
    static const std::array<std::string, 4> candidates = {
        "malloc", "realloc", "calloc", "free",
    };

    if (std::find(candidates.begin(), candidates.end(),
                  CI.getCalledFunction()->stripPointerCasts()->getName()) !=
        candidates.end()) {
        _criterion->insert(&CI);
    }
}

void MemoryLeak::setCriterion() {
    _criterion = std::make_shared<Criterion>();
    visit(_M);
}

} // end namespace crtr
