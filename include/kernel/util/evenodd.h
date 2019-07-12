/*
 *  Copyright (c) 2017 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */
#ifndef EVEN_ODD_H
#define EVEN_ODD_H

#include <kernel/core/kernel.h>
namespace IDISA { class IDISA_Builder; }
namespace llvm { class Value; }

namespace kernel {

class EvenOddKernel final : public BlockOrientedKernel {
public:
    EvenOddKernel(const std::unique_ptr<kernel::KernelBuilder> & b);
private:
    void generateDoBlockMethod(const std::unique_ptr<kernel::KernelBuilder> & iBuilder) override;
};

}
#endif
