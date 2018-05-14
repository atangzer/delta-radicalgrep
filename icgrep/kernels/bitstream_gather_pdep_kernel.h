
#ifndef ICGREP_BITSTREAM_GATHER_PDEP_KERNEL_H
#define ICGREP_BITSTREAM_GATHER_PDEP_KERNEL_H

#include <kernels/kernel.h>
#include <llvm/IR/Value.h>
#include <string>

namespace kernel {

class BitStreamGatherPDEPKernel final : public MultiBlockKernel {
public:
    BitStreamGatherPDEPKernel(const std::unique_ptr<kernel::KernelBuilder> & b, const unsigned numberOfStream = 8, std::string name = "BitStreamPDEPKernel");
    bool isCachable() const override { return true; }
    bool hasSignature() const override { return false; }
private:
    void generateMultiBlockLogic(const std::unique_ptr<KernelBuilder> & b, llvm::Value * const numOfStrides) final;
private:
    const unsigned mNumberOfStream;
};

}

#endif //ICGREP_BITSTREAM_GATHER_PDEP_KERNEL_H
