/*
 *  Copyright (c) 2016 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */

#ifndef SYMBOLTABLEPIPELINE_H
#define SYMBOLTABLEPIPELINE_H

#include <IDISA/idisa_builder.h>
#include "kernel.h"

namespace llvm {
    class Module;
    class Function;
    class Type;
}

namespace pablo { class PabloFunction; class PabloBlock; }

namespace kernel {

class SymbolTableBuilder {
public:
    SymbolTableBuilder(llvm::Module * m, IDISA::IDISA_Builder * b);
    ~SymbolTableBuilder();
    void createKernels();
    llvm::Function * ExecuteKernels();

protected:

    pablo::PabloFunction * generateLeadingFunction(const std::vector<unsigned> & endpoints);
    pablo::PabloFunction * generateSortingFunction(const pablo::PabloFunction * const leading, const std::vector<unsigned> & endpoints);

    void generateGatherKernel(KernelBuilder * kBuilder, const std::vector<unsigned> & endpoints, const unsigned scanWordBitWidth = 64);
    Function * generateGatherFunction(Type * const transposedVectorType, const unsigned minCount, const unsigned maxCount);

    Value * generateGather(Value * const base, Value * const vindex);
    Value * generateMaskedGather(Value * const base, Value * const vindex, Value * const mask);

    void generateLLVMParser();

private:
    llvm::Module *                      mMod;
    IDISA::IDISA_Builder *              iBuilder;
    KernelBuilder *                     mS2PKernel;
    KernelBuilder *                     mLeadingKernel;
    KernelBuilder *                     mSortingKernel;
    KernelBuilder *                     mGatherKernel;

    unsigned                            mLongestLookahead;
    llvm::Type *                        mBitBlockType;
    int                                 mBlockSize;
};

}

#endif // SYMBOLTABLEPIPELINE_H
