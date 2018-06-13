/*
 *  Copyright (c) 2017 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */
#ifndef BITSTREAM_PDEP_KERNEL_H
#define BITSTREAM_PDEP_KERNEL_H

#include <kernels/kernel.h>
#include <llvm/IR/Value.h>
#include <string>

namespace kernel {

class BitStreamPDEPKernel final : public MultiBlockKernel {
public:
    BitStreamPDEPKernel(const std::unique_ptr<kernel::KernelBuilder> & b, const unsigned numberOfStream = 8, std::string name = "BitStreamPDEPKernel");
    bool isCachable() const override { return true; }
    bool hasSignature() const override { return false; }

protected:
    void generateMultiBlockLogic(const std::unique_ptr<KernelBuilder> & b, llvm::Value * const numOfStrides) final;
private:
    const unsigned mNumberOfStream;
};

}

#endif