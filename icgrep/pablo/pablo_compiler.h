/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#ifndef PABLO_COMPILER_H
#define PABLO_COMPILER_H

//indicates that we use llvm.uadd.with.overflow.carryin for genAddWithCarry
//#define USE_UADD_OVERFLOW

//Pablo Expressions
#include <pablo/codegenstate.h>
#include <pablo/pe_pabloe.h>
#include "unicode_categories.h"
#include <iostream>
#include <string>
#include <list>
#include <map>
#include <algorithm>

#include <llvm/Support/raw_ostream.h>

#ifdef USE_LLVM_3_4
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Linker.h>
#endif

#ifdef USE_LLVM_3_5
#include <llvm/IR/Verifier.h>
#endif

#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/MathExtras.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/Debug.h>

#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>

#include <llvm/IR/IRBuilder.h>

using namespace llvm;

namespace pablo {

struct LLVM_Gen_RetVal
{
    int carry_q_size;
    void *process_block_fptr;
};

class PabloCompiler {
    #ifdef USE_UADD_OVERFLOW
    struct SumWithOverflowPack {
        Value * sum;
        Value * obit;
    };
    #endif
public:
    PabloCompiler(std::map<std::string, std::string> name_map, std::string basis_pattern, int bits);
    ~PabloCompiler();
    LLVM_Gen_RetVal compile(const PabloBlock & cg_state);
private:
    Module * MakeLLVMModule();
    void DefineTypes();
    void DeclareFunctions();
    void DeclareCallFunctions(const ExpressionList & stmts);
    void DeclareCallFunctions(const PabloE * expr);
    void LoadBitBlocksFromStaticExtern();
    void SetReturnMarker(Value * marker, const unsigned output_idx);

    Value* GetMarker(const std::string & name);

    Value* compileStatements(const ExpressionList & stmts);
    Value* compileStatement(const PabloE * stmt);

    Value* compileExpression(const PabloE * expr);
    Value* genCarryInLoad(Value* ptr_carry_q, int carryq_idx);
    Value* genCarryOutStore(Value* carryout, Value* ptr_carry_q, int carryq_idx);
    Value* genAddWithCarry(Value* e1, Value* e2);
    Value* genAdvanceWithCarry(Value* e1);
    Value* genBitBlockAny(Value* e);
    Value* genShiftHighbitToLow(Value* e, const Twine &namehint = "");
    Value* genShiftLeft64(Value* e, const Twine &namehint = "") ;
    Value* genNot(Value* e, const Twine &namehint = "");

    #ifdef USE_UADD_OVERFLOW
    SumWithOverflowPack callUaddOverflow(Value *e1, Value *e2, Value *cin);
    #endif

    std::map<std::string, Value*>       mCalleeMap;
    std::map<std::string, Value*>       mMarkerMap;


    int                                 mBits;
    std::map<std::string, std::string>  m_name_map;
    std::string                         mBasisBitPattern;

    Module* const                       mMod;
    BasicBlock*                         mBasicBlock;
    ExecutionEngine*                    mExecutionEngine;

    VectorType* const                   mXi64Vect;
    VectorType* const                   mXi128Vect;
    PointerType*                        mBasisBitsInputPtr;
    PointerType*                        mOutputPtr;

    int                                 mCarryQueueIdx;
    Value*                              mptr_carry_q;

    int                                 mCarryQueueSize;

    ConstantAggregateZero* const        mZeroInitializer;
    Constant* const                     mAllOneInitializer;

    FunctionType*                       mFuncTy_0;
    Function*                           mFunc_process_block;


    AllocaInst*                         mBasisBitsAddr;
    AllocaInst*                         mPtr_carry_q_addr;
    AllocaInst*                         mPtr_output_addr;
};

}

#endif // LLVM_GENERATOR_H
