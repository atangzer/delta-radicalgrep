/*
 *  Copyright (c) 2017 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#ifndef LZ4_BYTESTREAM_DECODER_H
#define LZ4_BYTESTREAM_DECODER_H

#include <kernels/core/kernel.h>

namespace IDISA { class IDISA_Builder; }

namespace kernel {

class LZ4ByteStreamDecoderKernel final : public MultiBlockKernel {
public:
    LZ4ByteStreamDecoderKernel(const std::unique_ptr<kernel::KernelBuilder> &,
                               // inputs
                               StreamSet * literalIndexes,
                               StreamSet * matchIndexes,
                               StreamSet * inputStream,
                               // output
                               StreamSet * outputStream);
protected:
    void generateMultiBlockLogic(const std::unique_ptr<kernel::KernelBuilder> & iBuilder, llvm::Value * numOfStrides) override;
private:
    const size_t mBufferSize;
};

}

#endif  // LZ4_BYTESTREAM_DECODER_H
