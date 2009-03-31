// -*- C++ -*-
#ifndef PYTHON_LL_COMPILE_H
#define PYTHON_LL_COMPILE_H

#ifndef __cplusplus
#error This header expects to be included only in C++ source
#endif

#include "llvm/Support/IRBuilder.h"
#include <string>

namespace py {

/// Helps the compiler build LLVM functions corresponding to Python
/// functions.  This class maintains the IRBuilder and several Value*s
/// set up in the entry block.
class LlvmFunctionBuilder {
public:
    LlvmFunctionBuilder(llvm::Module *module, const std::string& name);

    llvm::Function *function() { return function_; }
    llvm::IRBuilder<>& builder() { return builder_; }

    /// Sets the insert point to next_block, inserting an
    /// unconditional branch to there if the current block doesn't yet
    /// have a terminator instruction.
    void FallThroughTo(llvm::BasicBlock *next_block);

    /// The following methods operate like the opcodes with the same
    /// name.
    void LOAD_CONST(int index);
    void LOAD_FAST(int index);
    void STORE_FAST(int index);
    void DELETE_FAST(int index);

    void SETUP_LOOP(llvm::BasicBlock *target, llvm::BasicBlock *fallthrough);
    void GET_ITER();
    void FOR_ITER(llvm::BasicBlock *target, llvm::BasicBlock *fallthrough);
    void POP_BLOCK();

    void JUMP_FORWARD(llvm::BasicBlock *target, llvm::BasicBlock *fallthrough) {
        JUMP_ABSOLUTE(target, fallthrough);
    }
    void JUMP_ABSOLUTE(llvm::BasicBlock *target, llvm::BasicBlock *fallthrough);

    void RETURN_VALUE();

    void BINARY_ADD();
    void BINARY_SUBTRACT();
    void BINARY_MULTIPLY();
    void BINARY_TRUE_DIVIDE();
    void BINARY_DIVIDE();
    void BINARY_MODULO();
    void BINARY_POWER();
    void BINARY_LSHIFT();
    void BINARY_RSHIFT();
    void BINARY_OR();
    void BINARY_XOR();
    void BINARY_AND();
    void BINARY_FLOOR_DIVIDE();
    void BINARY_SUBSCR();

    void INPLACE_ADD();
    void INPLACE_SUBTRACT();
    void INPLACE_MULTIPLY();
    void INPLACE_TRUE_DIVIDE();
    void INPLACE_DIVIDE();
    void INPLACE_MODULO();
    void INPLACE_POWER();
    void INPLACE_LSHIFT();
    void INPLACE_RSHIFT();
    void INPLACE_OR();
    void INPLACE_XOR();
    void INPLACE_AND();
    void INPLACE_FLOOR_DIVIDE();

    void UNARY_CONVERT();
    void UNARY_INVERT();
    void UNARY_POSITIVE();
    void UNARY_NEGATIVE();
    void UNARY_NOT();

    void STORE_SUBSCR();
    void DELETE_SUBSCR();

    void POP_TOP();
    void DUP_TOP();
    void DUP_TOP_TWO();
    void DUP_TOP_THREE();
    void ROT_TWO();
    void ROT_THREE();
    void ROT_FOUR();
    void LIST_APPEND();
    void STORE_MAP();

    void BUILD_TUPLE(int size);
    void BUILD_LIST(int size);
    void BUILD_MAP(int size);

#define UNIMPLEMENTED(NAME) \
    void NAME() { \
        InsertAbort(); \
    }
#define UNIMPLEMENTED_I(NAME) \
    void NAME(int index) { \
        InsertAbort(); \
    }
#define UNIMPLEMENTED_J(NAME) \
    void NAME(llvm::BasicBlock *target, llvm::BasicBlock *fallthrough) { \
        InsertAbort(); \
    }

    UNIMPLEMENTED(SLICE_NONE);
    UNIMPLEMENTED(SLICE_LEFT);
    UNIMPLEMENTED(SLICE_RIGHT);
    UNIMPLEMENTED(SLICE_BOTH);
    UNIMPLEMENTED(STORE_SLICE_NONE);
    UNIMPLEMENTED(STORE_SLICE_LEFT);
    UNIMPLEMENTED(STORE_SLICE_RIGHT);
    UNIMPLEMENTED(STORE_SLICE_BOTH);
    UNIMPLEMENTED(DELETE_SLICE_NONE);
    UNIMPLEMENTED(DELETE_SLICE_LEFT);
    UNIMPLEMENTED(DELETE_SLICE_RIGHT);
    UNIMPLEMENTED(DELETE_SLICE_BOTH);
    UNIMPLEMENTED(BUILD_SLICE_TWO)
    UNIMPLEMENTED(BUILD_SLICE_THREE)
    UNIMPLEMENTED(BREAK_LOOP)
    UNIMPLEMENTED(RAISE_VARARGS_ZERO)
    UNIMPLEMENTED(RAISE_VARARGS_ONE)
    UNIMPLEMENTED(RAISE_VARARGS_TWO)
    UNIMPLEMENTED(RAISE_VARARGS_THREE)
    UNIMPLEMENTED(WITH_CLEANUP)
    UNIMPLEMENTED(END_FINALLY)
    UNIMPLEMENTED(YIELD_VALUE)

    UNIMPLEMENTED_I(LOAD_ATTR)
    UNIMPLEMENTED_I(STORE_ATTR)
    UNIMPLEMENTED_I(DELETE_ATTR)
    UNIMPLEMENTED_I(LOAD_DEREF);
    UNIMPLEMENTED_I(STORE_DEREF);
    UNIMPLEMENTED_I(LOAD_GLOBAL);
    UNIMPLEMENTED_I(STORE_GLOBAL);
    UNIMPLEMENTED_I(DELETE_GLOBAL);
    UNIMPLEMENTED_I(LOAD_NAME);
    UNIMPLEMENTED_I(STORE_NAME);
    UNIMPLEMENTED_I(DELETE_NAME);
    UNIMPLEMENTED_I(CALL_FUNCTION)
    UNIMPLEMENTED_I(CALL_FUNCTION_VAR_KW)
    UNIMPLEMENTED_I(LOAD_CLOSURE)
    UNIMPLEMENTED_I(MAKE_CLOSURE)
    UNIMPLEMENTED_I(COMPARE_OP)
    UNIMPLEMENTED_I(UNPACK_SEQUENCE)

    UNIMPLEMENTED_J(POP_JUMP_IF_FALSE);
    UNIMPLEMENTED_J(POP_JUMP_IF_TRUE);
    UNIMPLEMENTED_J(JUMP_IF_FALSE_OR_POP);
    UNIMPLEMENTED_J(JUMP_IF_TRUE_OR_POP);
    UNIMPLEMENTED_J(CONTINUE_LOOP);
    UNIMPLEMENTED_J(SETUP_EXCEPT);
    UNIMPLEMENTED_J(SETUP_FINALLY);

#undef UNIMPLEMENTED
#undef UNIMPLEMENTED_I
#undef UNIMPLEMENTED_UNCOND_J
#undef UNIMPLEMENTED_COND_J

private:
    /// These two functions increment or decrement the reference count
    /// of a PyObject*. The behavior is undefined if the Value's type
    /// isn't PyObject* or a subclass.
    void IncRef(llvm::Value *value);
    void DecRef(llvm::Value *value);
    void XDecRef(llvm::Value *value);
    /// These two push or pop a value onto or off of the stack. The
    /// behavior is undefined if the Value's type isn't PyObject* or a
    /// subclass.  These do no refcount operations.  You have to
    /// ensure that a pointer in the live part of the stack owns a
    /// reference.
    void Push(llvm::Value *value);
    llvm::Value *Pop();

    // Replaces a local variable with the PyObject* stored in
    // new_value.  Decrements the original value's refcount after
    // replacing it.
    void SetLocal(int locals_index, llvm::Value *new_value);

    /// Inserts a call that will abort the program when it's
    /// reached. This is useful for not-yet-defined instructions.
    void InsertAbort();

    /// Like format_exc_check_arg in ceval.cc.  exc_name and format_str are
    /// runtime constants; obj is computed.
    void FormatExcCheckArg(const std::string &exc_name,
                           const char *format_str,
                           llvm::Value *obj);

    // Returns the global variable with type T and name 'name'. The
    // variable will be looked up in Python's runtime.
    template<typename T>
    llvm::Constant *GetGlobalVariable(const std::string &name);
    // Returns the global function with type T and name 'name'. The
    // function will be looked up in Python's runtime.
    template<typename T>
    llvm::Function *GetGlobalFunction(const std::string &name);

    // Returns an i1, true if value represents a NULL pointer.
    llvm::Value *IsNull(llvm::Value *value);
    // Returns an i1, true if value is a non-zero integer.
    llvm::Value *IsNonZero(llvm::Value *value);

    // Helper methods for binary and unary operators
    void GenericBinOp(const char *name, const char *funcname);
    void GenericPowOp(const char *name, const char *funcname);
    void GenericUnaryOp(const char *name, const char *funcname);

    // Helper methods for list and tuple indexing
    void List_SET_ITEM(llvm::Value *lst, llvm::Value *idx, llvm::Value *item);
    void Tuple_SET_ITEM(llvm::Value *tup, llvm::Value *idx, llvm::Value *item);

    void SequenceBuilder(int size, const char *createname,
        void (LlvmFunctionBuilder::*method)(llvm::Value *, llvm::Value *,
                                            llvm::Value *));

    llvm::Module *const module_;
    llvm::Function *const function_;
    llvm::IRBuilder<> builder_;

    // The following pointers hold values created in the function's
    // entry block. They're constant after construction.
    llvm::Value *frame_;

    llvm::Value *stack_pointer_addr_;
    llvm::Value *varnames_;
    llvm::Value *names_;
    llvm::Value *consts_;
    llvm::Value *fastlocals_;
    llvm::Value *freevars_;
};

}  // namespace py

#endif /* PYTHON_LL_COMPILE_H */
