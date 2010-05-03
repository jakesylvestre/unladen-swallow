#include "Python.h"

#include "JIT/opcodes/slice.h"
#include "JIT/llvm_fbuilder.h"

#include "llvm/BasicBlock.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"

using llvm::BasicBlock;
using llvm::ConstantInt;
using llvm::Function;
using llvm::Type;
using llvm::Value;

namespace py {

OpcodeSlice::OpcodeSlice(LlvmFunctionBuilder *fbuilder) :
    fbuilder_(fbuilder)
{
}

void
OpcodeSlice::AssignSlice(Value *seq, Value *start, Value *stop,
                                 Value *source)
{
    Function *assign_slice = fbuilder_->GetGlobalFunction<
        int (PyObject *, PyObject *, PyObject *, PyObject *)>(
            "_PyEval_AssignSlice");
    Value *result = fbuilder_->CreateCall(
        assign_slice, seq, start, stop, source, "ApplySlice_result");
    fbuilder_->XDecRef(source);
    fbuilder_->XDecRef(stop);
    fbuilder_->XDecRef(start);
    fbuilder_->DecRef(seq);
    fbuilder_->PropagateExceptionOnNonZero(result);
}

void
OpcodeSlice::ApplySlice(Value *seq, Value *start, Value *stop)
{
    Function *build_slice = fbuilder_->GetGlobalFunction<
        PyObject *(PyObject *, PyObject *, PyObject *)>("_PyEval_ApplySlice");
    Value *result = fbuilder_->CreateCall(
        build_slice, seq, start, stop, "ApplySlice_result");
    fbuilder_->XDecRef(stop);
    fbuilder_->XDecRef(start);
    fbuilder_->DecRef(seq);
    fbuilder_->PropagateExceptionOnNull(result);
    fbuilder_->Push(result);
}

void
OpcodeSlice::SLICE_BOTH()
{
    Value *stop = fbuilder_->Pop();
    Value *start = fbuilder_->Pop();
    Value *seq = fbuilder_->Pop();
    this->ApplySlice(seq, start, stop);
}

void
OpcodeSlice::SLICE_LEFT()
{
    Value *stop = fbuilder_->GetNull<PyObject*>();
    Value *start = fbuilder_->Pop();
    Value *seq = fbuilder_->Pop();
    this->ApplySlice(seq, start, stop);
}

void
OpcodeSlice::SLICE_RIGHT()
{
    Value *stop = fbuilder_->Pop();
    Value *start = fbuilder_->GetNull<PyObject*>();
    Value *seq = fbuilder_->Pop();
    this->ApplySlice(seq, start, stop);
}

void
OpcodeSlice::SLICE_NONE()
{
    Value *stop = fbuilder_->GetNull<PyObject*>();
    Value *start = fbuilder_->GetNull<PyObject*>();
    Value *seq = fbuilder_->Pop();
    this->ApplySlice(seq, start, stop);
}

void
OpcodeSlice::STORE_SLICE_BOTH()
{
    Value *stop = fbuilder_->Pop();
    Value *start = fbuilder_->Pop();
    Value *seq = fbuilder_->Pop();
    Value *source = fbuilder_->Pop();
    this->AssignSlice(seq, start, stop, source);
}

void
OpcodeSlice::STORE_SLICE_LEFT()
{
    Value *stop = fbuilder_->GetNull<PyObject*>();
    Value *start = fbuilder_->Pop();
    Value *seq = fbuilder_->Pop();
    Value *source = fbuilder_->Pop();
    this->AssignSlice(seq, start, stop, source);
}

void
OpcodeSlice::STORE_SLICE_RIGHT()
{
    Value *stop = fbuilder_->Pop();
    Value *start = fbuilder_->GetNull<PyObject*>();
    Value *seq = fbuilder_->Pop();
    Value *source = fbuilder_->Pop();
    this->AssignSlice(seq, start, stop, source);
}

void
OpcodeSlice::STORE_SLICE_NONE()
{
    Value *stop = fbuilder_->GetNull<PyObject*>();
    Value *start = fbuilder_->GetNull<PyObject*>();
    Value *seq = fbuilder_->Pop();
    Value *source = fbuilder_->Pop();
    this->AssignSlice(seq, start, stop, source);
}

void
OpcodeSlice::DELETE_SLICE_BOTH()
{
    Value *stop = fbuilder_->Pop();
    Value *start = fbuilder_->Pop();
    Value *seq = fbuilder_->Pop();
    Value *source = fbuilder_->GetNull<PyObject*>();
    this->AssignSlice(seq, start, stop, source);
}

void
OpcodeSlice::DELETE_SLICE_LEFT()
{
    Value *stop = fbuilder_->GetNull<PyObject*>();
    Value *start = fbuilder_->Pop();
    Value *seq = fbuilder_->Pop();
    Value *source = fbuilder_->GetNull<PyObject*>();
    this->AssignSlice(seq, start, stop, source);
}

void
OpcodeSlice::DELETE_SLICE_RIGHT()
{
    Value *stop = fbuilder_->Pop();
    Value *start = fbuilder_->GetNull<PyObject*>();
    Value *seq = fbuilder_->Pop();
    Value *source = fbuilder_->GetNull<PyObject*>();
    this->AssignSlice(seq, start, stop, source);
}

void
OpcodeSlice::DELETE_SLICE_NONE()
{
    Value *stop = fbuilder_->GetNull<PyObject*>();
    Value *start = fbuilder_->GetNull<PyObject*>();
    Value *seq = fbuilder_->Pop();
    Value *source = fbuilder_->GetNull<PyObject*>();
    this->AssignSlice(seq, start, stop, source);
}

void
OpcodeSlice::BUILD_SLICE_TWO()
{
    Value *step = fbuilder_->GetNull<PyObject*>();
    Value *stop = fbuilder_->Pop();
    Value *start = fbuilder_->Pop();
    Function *build_slice = fbuilder_->GetGlobalFunction<
        PyObject *(PyObject *, PyObject *, PyObject *)>("PySlice_New");
    Value *result = fbuilder_->CreateCall(
        build_slice, start, stop, step, "BUILD_SLICE_result");
    fbuilder_->DecRef(start);
    fbuilder_->DecRef(stop);
    fbuilder_->PropagateExceptionOnNull(result);
    fbuilder_->Push(result);
}

void
OpcodeSlice::BUILD_SLICE_THREE()
{
    Value *step = fbuilder_->Pop();
    Value *stop = fbuilder_->Pop();
    Value *start = fbuilder_->Pop();
    Function *build_slice = fbuilder_->GetGlobalFunction<
        PyObject *(PyObject *, PyObject *, PyObject *)>("PySlice_New");
    Value *result = fbuilder_->CreateCall(
        build_slice, start, stop, step, "BUILD_SLICE_result");
    fbuilder_->DecRef(start);
    fbuilder_->DecRef(stop);
    fbuilder_->DecRef(step);
    fbuilder_->PropagateExceptionOnNull(result);
    fbuilder_->Push(result);
}

}
