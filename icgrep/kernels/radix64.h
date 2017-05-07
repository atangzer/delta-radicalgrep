/*
 *  Copyright (c) 2016 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */
#ifndef RADIX64_H
#define RADIX64_H

#include "kernel.h"

namespace llvm { class Module; }
namespace llvm { class Value; }

namespace IDISA { class IDISA_Builder; }

namespace kernel {

/*  expand3_4 transforms a byte sequence by duplicating every third byte. 
    Each 3 bytes of the input abc produces a 4 byte output abcc.   
    This is a useful preparatory transformation in various radix-64 encodings. */
 
class expand3_4Kernel final : public SegmentOrientedKernel {
public:   
    expand3_4Kernel(const std::unique_ptr<kernel::KernelBuilder> & iBuilder);
    bool isCachable() const override { return true; }
    bool moduleIDisSignature() const override { return true; }
private:
    void generateDoSegmentMethod() override;
};

class radix64Kernel final : public BlockOrientedKernel {
public:
    radix64Kernel(const std::unique_ptr<kernel::KernelBuilder> & iBuilder);
    bool isCachable() const override { return true; }
    bool moduleIDisSignature() const override { return true; }
private:
    virtual void generateDoBlockMethod() override;
    virtual void generateFinalBlockMethod(llvm::Value * remainingBytes) override;
    llvm::Value* processPackData(llvm::Value* packData) const;
};

class base64Kernel final : public BlockOrientedKernel {
public:
    base64Kernel(const std::unique_ptr<kernel::KernelBuilder> & iBuilder);
    bool isCachable() const override { return true; }
    bool moduleIDisSignature() const override { return true; }
private:
    virtual void generateDoBlockMethod() override;
    virtual void generateFinalBlockMethod(llvm::Value * remainingBytes) override;
    llvm::Value* processPackData(llvm::Value* packData) const;
};

}
#endif
