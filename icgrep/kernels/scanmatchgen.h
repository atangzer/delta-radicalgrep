/*
 *  Copyright (c) 2016 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */
#ifndef SCANMATCHGEN_H
#define SCANMATCHGEN_H

#include <kernels/core/kernel.h>
namespace IDISA { class IDISA_Builder; }
namespace llvm { class Function; }
namespace llvm { class Module; }

namespace kernel {


class ScanMatchKernel : public MultiBlockKernel {
public:
    ScanMatchKernel(const std::unique_ptr<kernel::KernelBuilder> & b, StreamSet * const Matches, StreamSet * const LineBreakStream, StreamSet * const ByteStream, Scalar * const callbackObject);
    bool isCachable() const override { return true; }
    bool hasSignature() const override { return false; }
private:
    void generateMultiBlockLogic(const std::unique_ptr<kernel::KernelBuilder> & iBuilder, llvm::Value * const numOfStrides) override;
};

}
#endif // SCANMATCHGEN_H
