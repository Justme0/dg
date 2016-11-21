#ifndef DEFECT_H
#define DEFECT_H

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstVisitor.h"
#include <list>
#include <memory>
#include <mutex>

/// namespace criterion
namespace crtr {

/// Abstract base class
class Defect {
public:
    /// Define slicing criterion type. Each criterion corresponds to an instruction.
    using Criterion = std::list<const llvm::Instruction *>;

protected:
    /// Program slicing criterion.
    Criterion _criterion;

    /// Flag for computing criterion the first time.
    mutable std::once_flag _firstFlag;

    /// Analyse which module.
    llvm::Module *_M = nullptr;

public:
    /// Create a concrete derived class instance according to different criterion (factory method).
    static std::shared_ptr<Defect> create(llvm::Module *M,
                                          const std::string &criterion);

    Defect(llvm::Module *M) : _M(M) {}

    /// Interface for client to get criterion.
    Criterion getCriterion() const;

    /// Get defect's name. e.g. "memory leak".
    virtual std::string getName() const = 0;

    virtual ~Defect() {}

protected:
    /// Set criterion according to each kind of concrete defect class.
    virtual void setCriterion() = 0;
};

class MemoryLeak : public Defect, protected llvm::InstVisitor<MemoryLeak> {
public:
    MemoryLeak(llvm::Module *M) : Defect(M) {}

    std::string getName() const override { return "memory leak"; }

private:
    void setCriterion() override { visit(_M); }

    // Note: InstVisitor needs to be a friend here to call visit*.
    friend llvm::InstVisitor<MemoryLeak>;
    void visitCallInst(const llvm::CallInst &CI);
};

class FileIO : public Defect, protected llvm::InstVisitor<FileIO> {
public:
    FileIO(llvm::Module *M) : Defect(M) {}

    std::string getName() const override { return "file IO"; }

private:
    void setCriterion() override { visit(_M); }

    // Note: InstVisitor needs to be a friend here to call visit*.
    friend llvm::InstVisitor<FileIO>;
    void visitCallInst(const llvm::CallInst &CI);
};

class DivideByZero : public Defect, protected llvm::InstVisitor<DivideByZero> {
public:
    DivideByZero(llvm::Module *M) : Defect(M) {};

    std::string getName() const override { return "divide by zero"; }

private:
    void setCriterion() override { visit(_M); }

    // Note: InstVisitor needs to be a friend here to call visit*.
    friend llvm::InstVisitor<DivideByZero>;
    void visitBinaryOperator(const llvm::BinaryOperator &BO);
};

class IntegerOverflow : public Defect, protected llvm::InstVisitor<IntegerOverflow> {
public:
    IntegerOverflow(llvm::Module *M) : Defect(M) {};

    std::string getName() const override { return "integer overflow"; }

private:
    void setCriterion() override { visit(_M); }

    // Note: InstVisitor needs to be a friend here to call visit*.
    friend llvm::InstVisitor<IntegerOverflow>;
    void visitTruncInst(const llvm::TruncInst &TI);
};

class PointerDereference : public Defect, protected llvm::InstVisitor<PointerDereference> {
public:
    PointerDereference(llvm::Module *M) : Defect(M) {};

    std::string getName() const override { return "pointer dereference"; }

private:
    void setCriterion() override { visit(_M); }

    // Note: InstVisitor needs to be a friend here to call visit*.
    friend llvm::InstVisitor<PointerDereference>;
    void visitLoadInst(const llvm::LoadInst &LI);
    void visitStoreInst(const llvm::StoreInst &SI);
};

class BufferOverflow : public Defect, protected llvm::InstVisitor<BufferOverflow> {
public:
    BufferOverflow(llvm::Module *M) : Defect(M) {};

    std::string getName() const override { return "buffer overflow"; }

private:
    void setCriterion() override { visit(_M); }

    // Note: InstVisitor needs to be a friend here to call visit*.
    friend llvm::InstVisitor<BufferOverflow>;
    void visitLoadInst(const llvm::LoadInst &LI);
    void visitStoreInst(const llvm::StoreInst &SI);
};

class UninitializedVariable : public Defect, protected llvm::InstVisitor<UninitializedVariable> {
public:
    UninitializedVariable(llvm::Module *M) : Defect(M) {}

    std::string getName() const override { return "uninitialized variable"; }

private:
    void setCriterion() override { visit(_M); };

    // Note: InstVisitor needs to be a friend here to call visit*.
    friend llvm::InstVisitor<UninitializedVariable>;
    void visitLoadInst(const llvm::LoadInst &LI);
    void visitStoreInst(const llvm::StoreInst &SI);
};

class StackAddressEscape : public Defect, protected llvm::InstVisitor<StackAddressEscape> {
public:
    StackAddressEscape(llvm::Module *M) : Defect(M) {}

    std::string getName() const override { return "return of stack variable address"; }

private:
    void setCriterion() override { visit(_M); };

    // Note: InstVisitor needs to be a friend here to call visit*.
    friend llvm::InstVisitor<StackAddressEscape>;
    void visitStoreInst(const llvm::StoreInst &SI);
    void visitReturnInst(const llvm::ReturnInst &RI);
};

} // end namespace crtr

#endif
