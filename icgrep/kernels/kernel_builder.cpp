#include "kernel_builder.h"
#include <kernels/kernel.h>
#include <kernels/streamset.h>

using namespace llvm;
using namespace parabix;

using Value = Value;

namespace kernel {

Value * KernelBuilder::getScalarFieldPtr(Value * const index) {
    return CreateGEP(mKernel->getInstance(), {getInt32(0), index});
}

Value * KernelBuilder::getScalarFieldPtr(const std::string & fieldName) {
    return getScalarFieldPtr(getInt32(mKernel->getScalarIndex(fieldName)));
}

Value * KernelBuilder::getScalarField(const std::string & fieldName) {
    return CreateLoad(getScalarFieldPtr(fieldName), fieldName);
}

void KernelBuilder::setScalarField(const std::string & fieldName, Value * value) {
    CreateStore(value, getScalarFieldPtr(fieldName));
}

Value * KernelBuilder::getStreamSetBufferPtr(const std::string & name) {
    return getScalarField(name + Kernel::BUFFER_PTR_SUFFIX);
}

LoadInst * KernelBuilder::acquireLogicalSegmentNo() {
    return CreateAtomicLoadAcquire(getScalarFieldPtr(Kernel::LOGICAL_SEGMENT_NO_SCALAR));
}

void KernelBuilder::releaseLogicalSegmentNo(Value * nextSegNo) {
    CreateAtomicStoreRelease(nextSegNo, getScalarFieldPtr(Kernel::LOGICAL_SEGMENT_NO_SCALAR));
}

Value * KernelBuilder::getProducedItemCount(const std::string & name, Value * doFinal) {
    Kernel::Port port; unsigned index;
    std::tie(port, index) = mKernel->getStreamPort(name);
    assert (port == Kernel::Port::Output);
    const auto rate = mKernel->getStreamOutput(index).rate;
    if (rate.isExact()) {
        const auto & refSet = rate.referenceStreamSet();
        std::string principalField;
        if (refSet.empty()) {
            const auto & principleSet = mKernel->getStreamOutput(0).name;
            if (mKernel->getStreamInputs().empty()) {
                principalField = principleSet + Kernel::PRODUCED_ITEM_COUNT_SUFFIX;
            } else {
                principalField = principleSet + Kernel::PROCESSED_ITEM_COUNT_SUFFIX;
            }
        } else {
            Kernel::Port port; unsigned pfIndex;
            std::tie(port, pfIndex) = mKernel->getStreamPort(refSet);
            if (port == Kernel::Port::Input) {
               principalField = refSet + Kernel::PROCESSED_ITEM_COUNT_SUFFIX;
            } else {
               principalField = refSet + Kernel::PRODUCED_ITEM_COUNT_SUFFIX;
            }
        }
        Value * const principleCount = getScalarField(principalField);
        return rate.CreateRatioCalculation(this, principleCount, doFinal);
    }
    return getScalarField(name + Kernel::PRODUCED_ITEM_COUNT_SUFFIX);
}

Value * KernelBuilder::getProcessedItemCount(const std::string & name) {
    Kernel::Port port; unsigned index;
    std::tie(port, index) = mKernel->getStreamPort(name);
    assert (port == Kernel::Port::Input);
    const auto & rate = mKernel->getStreamInput(index).rate;
    if (rate.isExact()) {
        std::string refSet = rate.referenceStreamSet();
        if (refSet.empty()) {
            refSet = mKernel->getStreamInput(0).name;
        }
        Value * const principleCount = getScalarField(refSet + Kernel::PROCESSED_ITEM_COUNT_SUFFIX);
        return rate.CreateRatioCalculation(this, principleCount);
    }
    return getScalarField(name + Kernel::PROCESSED_ITEM_COUNT_SUFFIX);
}

Value * KernelBuilder::getAvailableItemCount(const std::string & name) {
//    for (unsigned i = 0; i < mStreamSetInputs.size(); ++i) {
//        if (mStreamSetInputs[i].name == name) {
//            return mAvailableItemCount[i];
//        }
//    }
    return nullptr;
}

Value * KernelBuilder::getConsumedItemCount(const std::string & name) {
    return getScalarField(name + Kernel::CONSUMED_ITEM_COUNT_SUFFIX);
}

void KernelBuilder::setProducedItemCount(const std::string & name, Value * value) {
    setScalarField(name + Kernel::PRODUCED_ITEM_COUNT_SUFFIX, value);
}

void KernelBuilder::setProcessedItemCount(const std::string & name, Value * value) {
    setScalarField(name + Kernel::PROCESSED_ITEM_COUNT_SUFFIX, value);
}

void KernelBuilder::setConsumedItemCount(const std::string & name, Value * value) {
    setScalarField(name + Kernel::CONSUMED_ITEM_COUNT_SUFFIX, value);
}

Value * KernelBuilder::getTerminationSignal() {
    return getScalarField(Kernel::TERMINATION_SIGNAL);
}

void KernelBuilder::setTerminationSignal() {
    setScalarField(Kernel::TERMINATION_SIGNAL, getTrue());
}

Value * KernelBuilder::getLinearlyAccessibleItems(const std::string & name, Value * fromPosition) {
    Value * instance = getStreamSetBufferPtr(name);
    const StreamSetBuffer * const buf = mKernel->getInputStreamSetBuffer(name);
    return buf->getLinearlyAccessibleItems(this, instance, fromPosition);
}

Value * KernelBuilder::getConsumerLock(const std::string & name) {
    return getScalarField(name + Kernel::CONSUMER_SUFFIX);
}

void KernelBuilder::setConsumerLock(const std::string & name, Value * value) {
    setScalarField(name + Kernel::CONSUMER_SUFFIX, value);
}

inline Value * KernelBuilder::computeBlockIndex(Value * itemCount) {
    const auto divisor = getBitBlockWidth();
    if (LLVM_LIKELY((divisor & (divisor - 1)) == 0)) {
        return CreateLShr(itemCount, std::log2(divisor));
    } else {
        return CreateUDiv(itemCount, getSize(divisor));
    }
}

Value * KernelBuilder::getInputStreamBlockPtr(const std::string & name, Value * streamIndex) {
    Value * const blockIndex = computeBlockIndex(getProcessedItemCount(name));
    const StreamSetBuffer * const buf = mKernel->getInputStreamSetBuffer(name);
    return buf->getStreamBlockPtr(this, getStreamSetBufferPtr(name), streamIndex, blockIndex, true);
}

Value * KernelBuilder::loadInputStreamBlock(const std::string & name, Value * streamIndex) {
    return CreateBlockAlignedLoad(getInputStreamBlockPtr(name, streamIndex));
}

Value * KernelBuilder::getInputStreamPackPtr(const std::string & name, Value * streamIndex, Value * packIndex) {
    Value * const blockIndex = computeBlockIndex(getProcessedItemCount(name));
    const StreamSetBuffer * const buf = mKernel->getInputStreamSetBuffer(name);
    return buf->getStreamPackPtr(this, getStreamSetBufferPtr(name), streamIndex, blockIndex, packIndex, true);
}

Value * KernelBuilder::loadInputStreamPack(const std::string & name, Value * streamIndex, Value * packIndex) {
    return CreateBlockAlignedLoad(getInputStreamPackPtr(name, streamIndex, packIndex));
}

Value * KernelBuilder::getInputStreamSetCount(const std::string & name) {
    const StreamSetBuffer * const buf = mKernel->getInputStreamSetBuffer(name);
    return buf->getStreamSetCount(this, getStreamSetBufferPtr(name));
}

Value * KernelBuilder::getAdjustedInputStreamBlockPtr(Value * blockAdjustment, const std::string & name, Value * streamIndex) {
    Value * const blockIndex = CreateAdd(computeBlockIndex(getProcessedItemCount(name)), blockAdjustment);
    const StreamSetBuffer * const buf = mKernel->getInputStreamSetBuffer(name);
    return buf->getStreamBlockPtr(this, getStreamSetBufferPtr(name), streamIndex, blockIndex, true);
}

Value * KernelBuilder::getOutputStreamBlockPtr(const std::string & name, Value * streamIndex) {
    Value * const blockIndex = computeBlockIndex(getProducedItemCount(name));
    const StreamSetBuffer * const buf = mKernel->getOutputStreamSetBuffer(name);
    return buf->getStreamBlockPtr(this, getStreamSetBufferPtr(name), streamIndex, blockIndex, false);
}

void KernelBuilder::storeOutputStreamBlock(const std::string & name, Value * streamIndex, Value * toStore) {
    return CreateBlockAlignedStore(toStore, getOutputStreamBlockPtr(name, streamIndex));
}

Value * KernelBuilder::getOutputStreamPackPtr(const std::string & name, Value * streamIndex, Value * packIndex) {
    Value * const blockIndex = computeBlockIndex(getProducedItemCount(name));
    const StreamSetBuffer * const buf = mKernel->getOutputStreamSetBuffer(name);
    return buf->getStreamPackPtr(this, getStreamSetBufferPtr(name), streamIndex, blockIndex, packIndex, false);
}

void KernelBuilder::storeOutputStreamPack(const std::string & name, Value * streamIndex, Value * packIndex, Value * toStore) {
    return CreateBlockAlignedStore(toStore, getOutputStreamPackPtr(name, streamIndex, packIndex));
}

Value * KernelBuilder::getOutputStreamSetCount(const std::string & name) {
    const StreamSetBuffer * const buf = mKernel->getOutputStreamSetBuffer(name);
    return buf->getStreamSetCount(this, getStreamSetBufferPtr(name));
}

Value * KernelBuilder::getRawInputPointer(const std::string & name, Value * streamIndex, Value * absolutePosition) {
    const StreamSetBuffer * const buf = mKernel->getInputStreamSetBuffer(name);
    return buf->getRawItemPointer(this, getStreamSetBufferPtr(name), streamIndex, absolutePosition);
}

Value * KernelBuilder::getRawOutputPointer(const std::string & name, Value * streamIndex, Value * absolutePosition) {
    const StreamSetBuffer * const buf = mKernel->getOutputStreamSetBuffer(name);
    return buf->getRawItemPointer(this, getStreamSetBufferPtr(name), streamIndex, absolutePosition);
}

Value * KernelBuilder::getBaseAddress(const std::string & name) {
    return mKernel->getAnyStreamSetBuffer(name)->getBaseAddress(this, getStreamSetBufferPtr(name));
}

void KernelBuilder::setBaseAddress(const std::string & name, Value * const addr) {
    return mKernel->getAnyStreamSetBuffer(name)->setBaseAddress(this, getStreamSetBufferPtr(name), addr);
}

Value * KernelBuilder::getBufferedSize(const std::string & name) {
    return mKernel->getAnyStreamSetBuffer(name)->getBufferedSize(this, getStreamSetBufferPtr(name));
}

void KernelBuilder::setBufferedSize(const std::string & name, Value * size) {
    mKernel->getAnyStreamSetBuffer(name)->setBufferedSize(this, getStreamSetBufferPtr(name), size);
}

BasicBlock * KernelBuilder::CreateWaitForConsumers() {
    const auto consumers = mKernel->getStreamOutputs();
    BasicBlock * const entry = GetInsertBlock();
    if (consumers.empty()) {
        return entry;
    } else {
        Function * const parent = entry->getParent();
        IntegerType * const sizeTy = getSizeTy();
        ConstantInt * const zero = getInt32(0);
        ConstantInt * const one = getInt32(1);
        ConstantInt * const size0 = getSize(0);

        Value * const segNo = acquireLogicalSegmentNo();
        const auto n = consumers.size();
        BasicBlock * load[n + 1];
        BasicBlock * wait[n];
        for (unsigned i = 0; i < n; ++i) {
            load[i] = BasicBlock::Create(getContext(), consumers[i].name + "Load", parent);
            wait[i] = BasicBlock::Create(getContext(), consumers[i].name + "Wait", parent);
        }
        load[n] = BasicBlock::Create(getContext(), "Resume", parent);
        CreateBr(load[0]);
        for (unsigned i = 0; i < n; ++i) {

            SetInsertPoint(load[i]);
            Value * const outputConsumers = getConsumerLock(consumers[i].name);

            Value * const consumerCount = CreateLoad(CreateGEP(outputConsumers, {zero, zero}));
            Value * const consumerPtr = CreateLoad(CreateGEP(outputConsumers, {zero, one}));
            Value * const noConsumers = CreateICmpEQ(consumerCount, size0);
            CreateUnlikelyCondBr(noConsumers, load[i + 1], wait[i]);

            SetInsertPoint(wait[i]);
            PHINode * const consumerPhi = CreatePHI(sizeTy, 2);
            consumerPhi->addIncoming(size0, load[i]);

            Value * const conSegPtr = CreateLoad(CreateGEP(consumerPtr, consumerPhi));
            Value * const processedSegmentCount = CreateAtomicLoadAcquire(conSegPtr);
            Value * const ready = CreateICmpEQ(segNo, processedSegmentCount);
            assert (ready->getType() == getInt1Ty());
            Value * const nextConsumerIdx = CreateAdd(consumerPhi, CreateZExt(ready, sizeTy));
            consumerPhi->addIncoming(nextConsumerIdx, wait[i]);
            Value * const next = CreateICmpEQ(nextConsumerIdx, consumerCount);
            CreateCondBr(next, load[i + 1], wait[i]);
        }

        BasicBlock * const exit = load[n];
        SetInsertPoint(exit);
        return exit;
    }
}

}