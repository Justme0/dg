#ifndef DEFECT_H
#define DEFECT_H

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstVisitor.h"
#include <unordered_set>
#include <memory>

/// namespace criterion
namespace crtr {

/// Abstract base class
class Defect {
public:
    /// Define slicing criterion type. Each criterion corresponds to an instruction.
    using Criterion = std::unordered_set<llvm::Instruction *>;

protected:
    /// Pointer to criterion.
    std::shared_ptr<Criterion> _criterion;

    /// Analyse which module.
    llvm::Module *_M = nullptr;

public:
    Defect(llvm::Module *M) : _M(M) {}

    /// Interface for client to get criterion.
    Criterion getCriterion() const;

    virtual ~Defect() {}

protected:
    /// Set criterion according to each kind of concrete defect class.
    virtual void setCriterion() = 0;
};

class MemoryLeak : public Defect, protected llvm::InstVisitor<MemoryLeak> {
public:
    MemoryLeak(llvm::Module *M) : Defect(M) {}

    void visitCallInst(llvm::CallInst &CI);

private:
    void setCriterion() override;
};

class DivideByZero : public Defect {
public:
    DivideByZero(llvm::Module *M);

private:
    void setCriterion() override;
};

class Dereference : public Defect {
public:
    Dereference(llvm::Module *M);

private:
    void setCriterion() override;
};

class BufferOverflow : public Defect {
public:
    BufferOverflow(llvm::Module *M);

private:
    void setCriterion() override;
};

class IntegerOverflow : public Defect {
public:
    IntegerOverflow(llvm::Module *M);

private:
    void setCriterion() override;
};

class FileIO : public Defect {
public:
    FileIO(llvm::Module *M) : Defect(M) {}

private:
    void setCriterion() override;
};

class UninitializedVariable : public Defect {
public:
    UninitializedVariable(llvm::Module *M) : Defect(M) {}

private:
    void setCriterion() override;
};

class ReturnOfStackVariableAddress : public Defect {
public:
    ReturnOfStackVariableAddress(llvm::Module *M);

private:
    void setCriterion() override;
};

} // end namespace crtr

#endif
