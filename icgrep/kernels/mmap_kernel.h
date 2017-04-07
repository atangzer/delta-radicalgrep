/*
 *  Copyright (c) 2017 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */
#ifndef MMAP_KERNEL_H
#define MMAP_KERNEL_H

#include "kernel.h"
namespace IDISA { class IDISA_Builder; }

namespace kernel {

/* The MMapSourceKernel is a simple wrapper for an external MMap file buffer.
   The doSegment method of this kernel feeds one segment at a time to a 
   pipeline. */
    
class MMapSourceKernel final: public SegmentOrientedKernel {
public:
    MMapSourceKernel(IDISA::IDISA_Builder * iBuilder, unsigned blocksPerSegment = 1, unsigned codeUnitWidth = 8);  
    bool moduleIDisSignature() override {return true;}
private:
    void generateDoSegmentMethod(llvm::Value * doFinal, const std::vector<llvm::Value *> & producerPos) override;
private:
    const unsigned mSegmentBlocks;
    const unsigned mCodeUnitWidth;
};

}

#endif
