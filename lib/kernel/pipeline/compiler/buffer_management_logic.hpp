#ifndef BUFFER_ALLOCATION_HPP
#define BUFFER_ALLOCATION_HPP

#include "pipeline_compiler.hpp"

#include <llvm/Support/ErrorHandling.h>

namespace kernel {

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief addHandlesToPipelineKernel
 ** ------------------------------------------------------------------------------------------------------------- */
inline void PipelineCompiler::addBufferHandlesToPipelineKernel(BuilderRef b, const unsigned index) {
    for (const auto e : make_iterator_range(out_edges(index, mBufferGraph))) {
        const auto streamSet = target(e, mBufferGraph);
        const BufferNode & bn = mBufferGraph[streamSet];
        // external buffers already have a buffer handle
        if (LLVM_UNLIKELY(bn.isExternal())) {
            continue;
        }
        const BufferRateData & rd = mBufferGraph[e];
        const auto handleName = makeBufferName(index, rd.Port);
        StreamSetBuffer * const buffer = bn.Buffer;
        Type * const handleType = buffer->getHandleType(b);
        if (LLVM_LIKELY(bn.isOwned())) {
            if (LLVM_UNLIKELY(bn.NonLocal)) {
                mTarget->addInternalScalar(handleType, handleName);
            } else {
                mTarget->addThreadLocalScalar(handleType, handleName);
            }
        } else {
            mTarget->addNonPersistentScalar(handleType, handleName);
            mTarget->addInternalScalar(buffer->getPointerType(), handleName + LAST_GOOD_VIRTUAL_BASE_ADDRESS, index);
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief loadInternalStreamSetHandles
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::loadInternalStreamSetHandles(BuilderRef b, const bool nonLocal) {
    for (auto streamSet = FirstStreamSet; streamSet <= LastStreamSet; ++streamSet) {
        const BufferNode & bn = mBufferGraph[streamSet];
        // external buffers already have a buffer handle
        StreamSetBuffer * const buffer = bn.Buffer;
        if (LLVM_UNLIKELY(bn.isExternal())) {
            assert (isFromCurrentFunction(b, buffer->getHandle()));
        } else if (bn.NonLocal == nonLocal) {
            const auto pe = in_edge(streamSet, mBufferGraph);
            const auto producer = source(pe, mBufferGraph);
            const BufferRateData & rd = mBufferGraph[pe];
            const auto handleName = makeBufferName(producer, rd.Port);
            Value * const handle = b->getScalarFieldPtr(handleName);
            assert (buffer->getHandle() == nullptr);
            buffer->setHandle(handle);
        }
    }
    if (mHasZeroExtendedStream && (mTarget->hasThreadLocal() != nonLocal)) {
        mZeroExtendBuffer = b->getScalarFieldPtr(ZERO_EXTENDED_BUFFER);
        mZeroExtendSpace = b->getScalarFieldPtr(ZERO_EXTENDED_SPACE);
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief allocateOwnedBuffers
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::allocateOwnedBuffers(BuilderRef b, Value * const expectedNumOfStrides, const bool shared) {
    assert (expectedNumOfStrides);

    if (LLVM_UNLIKELY(CheckAssertions)) {
        Value * const valid = b->CreateIsNotNull(expectedNumOfStrides);
        b->CreateAssert(valid,
           "%s: expected number of strides for internally allocated buffers is 0",
           b->GetString(mTarget->getName()));
    }

    // recursively allocate any internal buffers for the nested kernels, giving them the correct
    // num of strides it should expect to perform
    for (auto i = FirstKernel; i <= LastKernel; ++i) {
        const Kernel * const kernelObj = getKernel(i);
        if (LLVM_UNLIKELY(kernelObj->allocatesInternalStreamSets())) {
            if (shared || kernelObj->hasThreadLocal()) {
                setActiveKernel(b, i, !shared);
                assert (mKernel == kernelObj);
                SmallVector<Value *, 3> params;
                if (LLVM_LIKELY(mKernelSharedHandle)) {
                    params.push_back(mKernelSharedHandle);
                }
                Value * func = nullptr;
                if (shared) {
                    func = getKernelAllocateSharedInternalStreamSetsFunction(b);
                } else {
                    func = getKernelAllocateThreadLocalInternalStreamSetsFunction(b);
                    params.push_back(mKernelThreadLocalHandle);
                }

                const auto scale = MaximumNumOfStrides[i] * Rational{mNumOfThreads};
                params.push_back(b->CreateCeilUMulRate(expectedNumOfStrides, scale));
                b->CreateCall(func, params);
            }
        }
        // and allocate any output buffers
        for (const auto e : make_iterator_range(out_edges(i, mBufferGraph))) {
            const auto j = target(e, mBufferGraph);
            const BufferNode & bn = mBufferGraph[j];
            if (LLVM_UNLIKELY(bn.isOwned() && bn.NonLocal == shared)) {
                StreamSetBuffer * const buffer = bn.Buffer;
                if (LLVM_LIKELY(bn.isInternal())) {
                    const BufferRateData & rd = mBufferGraph[e];
                    const auto handleName = makeBufferName(i, rd.Port);
                    Value * const handle = b->getScalarFieldPtr(handleName);
                    buffer->setHandle(handle);
                }
                assert ("a threadlocal buffer cannot be external" && (bn.isInternal() || shared));
                assert (buffer->getHandle());
                assert (isFromCurrentFunction(b, buffer->getHandle(), false));
                buffer->allocateBuffer(b, expectedNumOfStrides);
            }
        }
    }

}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief releaseOwnedBuffers
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::releaseOwnedBuffers(BuilderRef b, const bool nonLocal) {
    loadInternalStreamSetHandles(b, nonLocal);
    for (auto i = FirstStreamSet; i <= LastStreamSet; ++i) {
        const BufferNode & bn = mBufferGraph[i];
        if (LLVM_LIKELY(bn.isOwned() && bn.NonLocal == nonLocal)) {
            StreamSetBuffer * const buffer = bn.Buffer;
            assert (isFromCurrentFunction(b, buffer->getHandle(), false));
            buffer->releaseBuffer(b);

            #warning TraceDynamicBuffers needs to be fixed to permit thread local buffers.

            if (LLVM_UNLIKELY(codegen::DebugOptionIsSet(codegen::TraceDynamicBuffers))) {
                if (isa<DynamicBuffer>(buffer)) {

                    const auto pe = in_edge(i, mBufferGraph);
                    const auto p = source(pe, mBufferGraph);
                    const BufferRateData & rd = mBufferGraph[pe];
                    const auto prefix = makeBufferName(p, rd.Port);

                    Value * const traceData = b->getScalarFieldPtr(prefix + STATISTICS_BUFFER_EXPANSION_SUFFIX);
                    Constant * const ZERO = b->getInt32(0);
                    b->CreateFree(b->CreateLoad(b->CreateInBoundsGEP(traceData, {ZERO, ZERO})));
                }
            }
        }
    }
    if (mHasZeroExtendedStream && (mTarget->hasThreadLocal() != nonLocal)) {
        assert (isFromCurrentFunction(b, mZeroExtendBuffer, false));
        b->CreateFree(b->CreateLoad(mZeroExtendBuffer));
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief resetInternalBufferHandles
 ** ------------------------------------------------------------------------------------------------------------- */
inline void PipelineCompiler::resetInternalBufferHandles() {
    #ifndef NDEBUG
    for (auto i = FirstStreamSet; i <= LastStreamSet; ++i) {
        const BufferNode & bn = mBufferGraph[i];
        if (LLVM_LIKELY(bn.isInternal())) {
            StreamSetBuffer * const buffer = bn.Buffer;
            buffer->setHandle(nullptr);
        }
    }
    #endif
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief constructStreamSetBuffers
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::constructStreamSetBuffers(BuilderRef /* b */) {

    mStreamSetInputBuffers.clear();
    const auto numOfInputStreams = out_degree(PipelineInput, mBufferGraph);
    mStreamSetInputBuffers.resize(numOfInputStreams);
    for (const auto e : make_iterator_range(out_edges(PipelineInput, mBufferGraph))) {
        const BufferRateData & rd = mBufferGraph[e];
        const auto i = rd.Port.Number;
        const auto streamSet = target(e, mBufferGraph);
        assert (mBufferGraph[streamSet].isExternal());
        const auto j = streamSet - FirstStreamSet;
        StreamSetBuffer * const buffer = mInternalBuffers[j].release();
        assert (buffer == mBufferGraph[streamSet].Buffer);
        mStreamSetInputBuffers[i].reset(buffer);
    }

    mStreamSetOutputBuffers.clear();
    const auto numOfOutputStreams = in_degree(PipelineOutput, mBufferGraph);
    mStreamSetOutputBuffers.resize(numOfOutputStreams);
    for (const auto e : make_iterator_range(in_edges(PipelineOutput, mBufferGraph))) {
        const BufferRateData & rd = mBufferGraph[e];
        const auto i = rd.Port.Number;
        const auto streamSet = source(e, mBufferGraph);
        assert (mBufferGraph[streamSet].isExternal());
        const auto j = streamSet - FirstStreamSet;
        StreamSetBuffer * const buffer = mInternalBuffers[j].release();
        assert (buffer == mBufferGraph[streamSet].Buffer);
        mStreamSetOutputBuffers[i].reset(buffer);
    }

}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief readProcessedItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::readProcessedItemCounts(BuilderRef b) {
    for (const auto e : make_iterator_range(in_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];
        const auto inputPort = br.Port;
        const Binding & input = br.Binding;
        const auto prefix = makeBufferName(mKernelId, inputPort);
        Value * const processed = b->getScalarField(prefix + ITEM_COUNT_SUFFIX);
        mInitiallyProcessedItemCount(mKernelId, inputPort) = processed;
        if (input.isDeferred()) {
            Value * const deferred = b->getScalarField(prefix + DEFERRED_ITEM_COUNT_SUFFIX);
            mInitiallyProcessedDeferredItemCount(mKernelId, inputPort) = deferred;
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief readProducedItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::readProducedItemCounts(BuilderRef b) {

    for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];
        const auto outputPort = br.Port;
        const Binding & output = br.Binding;
        const auto prefix = makeBufferName(mKernelId, outputPort);
        const auto streamSet = target(e, mBufferGraph);
        mInitiallyProducedItemCount[streamSet] = b->getScalarField(prefix + ITEM_COUNT_SUFFIX);
        if (output.isDeferred()) {
            Value * const deferred = b->getScalarField(prefix + DEFERRED_ITEM_COUNT_SUFFIX);
            mInitiallyProducedDeferredItemCount[streamSet] = deferred;
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief initializeLocallyAvailableItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::initializeLocallyAvailableItemCounts(BuilderRef b, BasicBlock * const entryBlock) {
    IntegerType * const sizeTy = b->getSizeTy();
    for (auto streamSet = FirstStreamSet; streamSet <= LastStreamSet; ++streamSet) {
        PHINode * const phi = b->CreatePHI(sizeTy, 2);
        phi->addIncoming(mLocallyAvailableItems[streamSet], entryBlock);
        mInitiallyAvailableItemsPhi[streamSet] = phi;
        mLocallyAvailableItems[streamSet] = phi;
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief updateLocallyAvailableItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::updateLocallyAvailableItemCounts(BuilderRef b, BasicBlock * const entryBlock) {
    for (auto streamSet = FirstStreamSet; streamSet <= LastStreamSet; ++streamSet) {
        PHINode * const phi = mInitiallyAvailableItemsPhi[streamSet];
        phi->addIncoming(mLocallyAvailableItems[streamSet], entryBlock);
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getTotalItemCount
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getLocallyAvailableItemCount(BuilderRef /* b */, const StreamSetPort inputPort) const {
    const auto streamSet = getInputBufferVertex(inputPort);
    return mLocallyAvailableItems[streamSet];
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getTotalItemCount
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::setLocallyAvailableItemCount(BuilderRef /* b */, const StreamSetPort outputPort, Value * const available) {
    const auto streamSet = getOutputBufferVertex(outputPort);
    mLocallyAvailableItems[streamSet] = available;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief writeUpdatedItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::writeUpdatedItemCounts(BuilderRef b, const ItemCountSource source) {

    auto getProcessedArg = [&](const StreamSetPort port) -> Value * {
        switch (source) {
            case ItemCountSource::ComputedAtKernelCall:
                return mProcessedItemCount(mKernelId, port);
            case ItemCountSource::UpdatedItemCountsFromLoopExit:
                return mUpdatedProcessedPhi(mKernelId, port);
        }
        llvm_unreachable("unknown source type");
    };
    auto getProcessedDeferredArg = [&](const StreamSetPort port) -> Value * {
        switch (source) {
            case ItemCountSource::ComputedAtKernelCall:
                return mProcessedDeferredItemCount(mKernelId, port);
            case ItemCountSource::UpdatedItemCountsFromLoopExit:
                return mUpdatedProcessedDeferredPhi(mKernelId, port);
        }
        llvm_unreachable("unknown source type");
    };
    auto getProducedArg = [&](const StreamSetPort port) -> Value * {
        switch (source) {
            case ItemCountSource::ComputedAtKernelCall:
                return mProducedItemCount(mKernelId, port);
            case ItemCountSource::UpdatedItemCountsFromLoopExit:
                return mUpdatedProducedPhi(mKernelId, port);
        }
        llvm_unreachable("unknown source type");
    };
    auto getProducedDeferredArg = [&](const StreamSetPort port) -> Value * {
        switch (source) {
            case ItemCountSource::ComputedAtKernelCall:
                return mProducedDeferredItemCount(mKernelId, port);
            case ItemCountSource::UpdatedItemCountsFromLoopExit:
                return mUpdatedProducedDeferredPhi(mKernelId, port);
        }
        llvm_unreachable("unknown source type");
    };

    const auto numOfInputs = numOfStreamInputs(mKernelId);
    for (unsigned i = 0; i < numOfInputs; ++i) {
        const StreamSetPort inputPort{PortType::Input, i};
        const Binding & input = getInputBinding(inputPort);
        const auto prefix = makeBufferName(mKernelId, inputPort);
        b->setScalarField(prefix + ITEM_COUNT_SUFFIX, getProcessedArg(inputPort));
        if (input.isDeferred()) {
            b->setScalarField(prefix + DEFERRED_ITEM_COUNT_SUFFIX, getProcessedDeferredArg(inputPort));
        }
    }

    const auto numOfOutputs = numOfStreamOutputs(mKernelId);
    for (unsigned i = 0; i < numOfOutputs; ++i) {
        const StreamSetPort outputPort{PortType::Output, i};
        const Binding & output = getOutputBinding(outputPort);
        const auto prefix = makeBufferName(mKernelId, outputPort);
        b->setScalarField(prefix + ITEM_COUNT_SUFFIX, getProducedArg(outputPort));
        if (output.isDeferred()) {
            b->setScalarField(prefix + DEFERRED_ITEM_COUNT_SUFFIX, getProducedDeferredArg(outputPort));
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief recordFinalProducedItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::recordFinalProducedItemCounts(BuilderRef b) {
    for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];
        const auto outputPort = br.Port;
        Value * fullyProduced = nullptr;
        if (LLVM_UNLIKELY(mKernelIsInternallySynchronized)) {
            fullyProduced = mProducedItemCount(outputPort);
        } else {
            fullyProduced = mFullyProducedItemCount(outputPort);
        }
        setLocallyAvailableItemCount(b, outputPort, fullyProduced);
        initializeConsumedItemCount(b, outputPort, fullyProduced);
        #ifdef PRINT_DEBUG_MESSAGES
        const auto prefix = makeBufferName(mKernelId, outputPort);
        const auto streamSet = getOutputBufferVertex(outputPort);
        Value * const producedDelta = b->CreateSub(fullyProduced, mInitiallyProducedItemCount[streamSet]);
        debugPrint(b, prefix + "_producedΔ = %" PRIu64, producedDelta);
        #endif
    }
}


/** ------------------------------------------------------------------------------------------------------------- *
 * @brief readReturnedOutputVirtualBaseAddresses
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::readReturnedOutputVirtualBaseAddresses(BuilderRef b) const {
    for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
        const auto streamSet = target(e, mBufferGraph);
        const BufferNode & bn = mBufferGraph[streamSet];
        // owned or external buffers do not have a mutable vba
        if (LLVM_LIKELY(bn.isOwned() || bn.isExternal())) {
            continue;
        }
        const BufferRateData & rd = mBufferGraph[e];
        const StreamSetPort port(rd.Port.Type, rd.Port.Number);
        Value * const ptr = mReturnedOutputVirtualBaseAddressPtr(port); assert (ptr);
        Value * vba = b->CreateLoad(ptr);
        StreamSetBuffer * const buffer = bn.Buffer;
        vba = b->CreatePointerCast(vba, buffer->getPointerType());
        buffer->setBaseAddress(b.get(), vba);
        buffer->setCapacity(b.get(), mProducedItemCount(port));
        const auto handleName = makeBufferName(mKernelId, port);
        #ifdef PRINT_DEBUG_MESSAGES
        debugPrint(b, handleName + "_updatedVirtualBaseAddress = 0x%" PRIx64, buffer->getBaseAddress(b));
        #endif
        b->setScalarField(handleName + LAST_GOOD_VIRTUAL_BASE_ADDRESS, vba);
    }
}


/** ------------------------------------------------------------------------------------------------------------- *
 * @brief loadLastGoodVirtualBaseAddressesOfUnownedBuffers
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::loadLastGoodVirtualBaseAddressesOfUnownedBuffers(BuilderRef b, const size_t kernelId) const {
    for (const auto e : make_iterator_range(out_edges(kernelId, mBufferGraph))) {
        const auto streamSet = target(e, mBufferGraph);
        const BufferNode & bn = mBufferGraph[streamSet];
        // owned or external buffers do not have a mutable vba
        if (LLVM_LIKELY(bn.isOwned() || bn.isExternal())) {
            continue;
        }
        const BufferRateData & rd = mBufferGraph[e];
        const auto handleName = makeBufferName(kernelId, rd.Port);
        Value * const vba = b->getScalarField(handleName + LAST_GOOD_VIRTUAL_BASE_ADDRESS);
        StreamSetBuffer * const buffer = bn.Buffer;
        buffer->setBaseAddress(b.get(), vba);
    }
}

#warning Linear buffers do not correctly handle lookbehind

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief writeLookBehindLogic
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::writeLookBehindLogic(BuilderRef b) {
    for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];
        if (br.LookBehind) {
            const auto streamSet = target(e, mBufferGraph);
            const BufferNode & bn = mBufferGraph[streamSet];
            const StreamSetBuffer * const buffer = bn.Buffer;
            Constant * const underflow = b->getSize(br.LookBehind);
            Value * const produced = mAlreadyProducedPhi(br.Port);
            Value * const capacity = buffer->getCapacity(b);
            Value * const producedOffset = b->CreateURem(produced, capacity);
            Value * const needsCopy = b->CreateICmpULE(producedOffset, underflow);
            copy(b, CopyMode::LookBehind, needsCopy, br.Port, buffer, br.LookBehind);
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief writeLookBehindReflectionLogic
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::writeLookBehindReflectionLogic(BuilderRef b) {
    for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];
        if (br.Delay) {
            const auto streamSet = target(e, mBufferGraph);
            const BufferNode & bn = mBufferGraph[streamSet];
            const StreamSetBuffer * const buffer = bn.Buffer;

            Value * const capacity = buffer->getCapacity(b);
            const BufferRateData & br = mBufferGraph[e];
            Value * const produced = mAlreadyProducedPhi(mKernelId, br.Port);
            const auto size = round_up_to(br.Delay, b->getBitBlockWidth());
            Constant * const reflection = b->getSize(size);
            Value * const producedOffset = b->CreateURem(produced, capacity);
            Value * const needsCopy = b->CreateICmpULT(producedOffset, reflection);
            copy(b, CopyMode::LookBehindReflection, needsCopy, br.Port, buffer, br.Delay);
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief writeCopyBackLogic
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::writeCopyBackLogic(BuilderRef b) {
    for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];
        if (br.CopyBack) {
            const auto streamSet = target(e, mBufferGraph);
            const BufferNode & bn = mBufferGraph[streamSet];
            const StreamSetBuffer * const buffer = bn.Buffer;

            Value * const capacity = buffer->getCapacity(b);
            Value * const alreadyProduced = mAlreadyProducedPhi(br.Port);
            Value * const priorOffset = b->CreateURem(alreadyProduced, capacity);
            Value * const produced = mProducedItemCount(br.Port);
            Value * const producedOffset = b->CreateURem(produced, capacity);
            Value * const nonCapacityAlignedWrite = b->CreateIsNotNull(producedOffset);
            Value * const wroteToOverflow = b->CreateICmpULT(producedOffset, priorOffset);
            Value * const needsCopy = b->CreateAnd(nonCapacityAlignedWrite, wroteToOverflow);
            copy(b, CopyMode::CopyBack, needsCopy, br.Port, buffer, br.CopyBack);
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief writeLookAheadLogic
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::writeLookAheadLogic(BuilderRef b) {
    // Unless we modified the portion of data that ought to be reflected in the overflow region, do not copy
    // any data. To do so would incur extra writes and pollute the cache with potentially unnecessary data.
    for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];

        if (br.LookAhead) {

            const auto streamSet = target(e, mBufferGraph);
            const BufferNode & bn = mBufferGraph[streamSet];
            const StreamSetBuffer * const buffer = bn.Buffer;

            Value * const capacity = buffer->getCapacity(b);
            Value * const initial = mInitiallyProducedItemCount[streamSet];
            Value * const produced = mUpdatedProducedPhi(br.Port);

            // If we wrote anything and it was not our first write to the buffer ...
            Value * overwroteData = b->CreateICmpUGT(produced, capacity);
            const Binding & output = getOutputBinding(br.Port);
            const ProcessingRate & rate = output.getRate();
            const Rational ONE(1, 1);
            bool mayProduceZeroItems = false;
            if (rate.getLowerBound() < ONE) {
                mayProduceZeroItems = true;
            } else if (rate.isRelative()) {
                const Binding & ref = getBinding(getReference(br.Port));
                const ProcessingRate & refRate = ref.getRate();
                mayProduceZeroItems = (rate.getLowerBound() * refRate.getLowerBound()) < ONE;
            }
            if (LLVM_LIKELY(mayProduceZeroItems)) {
                Value * const producedOutput = b->CreateICmpNE(initial, produced);
                overwroteData = b->CreateAnd(overwroteData, producedOutput);
            }

            // And we started writing within the first block ...
            const auto size = round_up_to(br.LookAhead, b->getBitBlockWidth());
            Constant * const overflowSize = b->getSize(size);
            Value * const initialOffset = b->CreateURem(initial, capacity);
            Value * const startedWithinFirstBlock = b->CreateICmpULT(initialOffset, overflowSize);
            Value * const wroteToFirstBlock = b->CreateAnd(overwroteData, startedWithinFirstBlock);

            // And we started writing at the end of the buffer but wrapped over to the start of it,
            Value * const producedOffset = b->CreateURem(produced, capacity);
            Value * const wroteFromEndToStart = b->CreateICmpULT(producedOffset, initialOffset);

            // Then mirror the data in the overflow region.
            Value * const needsCopy = b->CreateOr(wroteToFirstBlock, wroteFromEndToStart);

            // TODO: optimize this further to ensure that we don't copy data that was just copied back from
            // the overflow. Should be enough just to have a "copyback flag" phi node to say it that was the
            // last thing it did to the buffer.

            copy(b, CopyMode::LookAhead, needsCopy, br.Port, buffer, br.LookAhead);
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief writeOverflowCopy
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::copy(BuilderRef b, const CopyMode mode, Value * cond,
                            const StreamSetPort outputPort, const StreamSetBuffer * const buffer,
                            const unsigned itemsToCopy) {

    auto makeSuffix = [](CopyMode mode) {
        switch (mode) {
            case CopyMode::LookAhead: return "LookAhead";
            case CopyMode::CopyBack: return "CopyBack";
            case CopyMode::LookBehind: return "LookBehind";
            case CopyMode::LookBehindReflection: return "Reflection";
        }
        llvm_unreachable("unknown copy mode!");
    };

    const auto prefix = makeBufferName(mKernelId, outputPort) + "_" + makeSuffix(mode);

    BasicBlock * const copyStart = b->CreateBasicBlock(prefix, mKernelExit);
    BasicBlock * const copyLoop = b->CreateBasicBlock(prefix + "Loop", mKernelExit);
    BasicBlock * recordCopyCycleCount = nullptr;
    if (EnableCycleCounter) {
        recordCopyCycleCount = b->CreateBasicBlock(prefix + "RecordCycleCount", mKernelExit);
    }
    BasicBlock * const copyExit = b->CreateBasicBlock(prefix + "Exit", mKernelExit);

    b->CreateUnlikelyCondBr(cond, copyStart, copyExit);

    b->SetInsertPoint(copyStart);

    startCycleCounter(b, CycleCounter::BEFORE_COPY);

    const auto itemWidth = getItemWidth(buffer->getBaseType());
    assert (is_power_2(itemWidth));
    const auto blockWidth = b->getBitBlockWidth();
    const auto bitsToCopy = round_up_to(itemsToCopy * itemWidth, 8);
    const auto bitsPerBlock = round_up_to(bitsToCopy, blockWidth);

    Value * const numOfStreams = buffer->getStreamSetCount(b);
    Value * const bytesToCopy = b->getSize(bitsToCopy / 8);
    Value * const bytesPerStream = b->getSize(bitsPerBlock / 8);

  //  Value * const bytesToCopy = b->CreateMul(bytesPerStream, numOfStreams);

    Value * source =  buffer->getOverflowAddress(b);
    Value * target = nullptr;

    b->CallPrintInt("overflow", source);
    b->CallPrintInt("bytesToCopy", bytesToCopy);
    b->CallPrintInt("bytesPerStream", bytesPerStream);

    if (buffer->isLinear()) {
        target = buffer->getMallocAddress(b);
    } else {
        target = buffer->getBaseAddress(b);
    }

    PointerType * const int8PtrTy = b->getInt8PtrTy();
    source = b->CreatePointerCast(source, int8PtrTy);
    target = b->CreatePointerCast(target, int8PtrTy);

    if (mode == CopyMode::LookBehind || mode == CopyMode::LookBehindReflection) {
        DataLayout DL(b->getModule());
        Type * const intPtrTy = DL.getIntPtrType(source->getType());
        Value * const offset = b->CreateNeg(b->CreateZExt(bytesToCopy, intPtrTy));
        source = b->CreateInBoundsGEP(source, offset);
        target = b->CreateInBoundsGEP(target, offset);
    }

    if (mode == CopyMode::LookAhead || mode == CopyMode::LookBehindReflection) {
        std::swap(target, source);
    }

    b->CallPrintInt("source", source);
    b->CallPrintInt("target", target);

    #ifdef PRINT_DEBUG_MESSAGES
    debugPrint(b, prefix + std::to_string(itemsToCopy) + "_bytesToCopy = %" PRIu64, bytesToCopy);
    #endif

    b->CreateBr(copyLoop);

    ConstantInt * const ZERO = b->getSize(0);

    b->SetInsertPoint(copyLoop);
    PHINode * const idx = b->CreatePHI(b->getSizeTy(), 2);
    idx->addIncoming(ZERO, copyStart);
    Value * const offset = b->CreateMul(idx, bytesPerStream);
    Value * const sourcePtr = b->CreateGEP(source, offset);
    Value * const targetPtr = b->CreateGEP(target, offset);

    b->CallPrintInt("sourcePtr", sourcePtr);
    b->CallPrintInt("targetPtr", targetPtr);


    b->CreateMemCpy(targetPtr, sourcePtr, bytesToCopy, bitsToCopy / 8);

    Value * const nextIdx = b->CreateAdd(idx, b->getSize(1));
    idx->addIncoming(nextIdx, copyLoop);
    Value * const done = b->CreateICmpEQ(nextIdx, numOfStreams);
    BasicBlock * const loopExit = EnableCycleCounter ? recordCopyCycleCount : copyExit;
    b->CreateCondBr(done, loopExit, copyLoop);

    if (EnableCycleCounter) {
        b->SetInsertPoint(recordCopyCycleCount);
        updateCycleCounter(b, CycleCounter::BEFORE_COPY, CycleCounter::AFTER_COPY);
        b->CreateBr(copyExit);
    }

    b->SetInsertPoint(copyExit);



}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief prepareLinearBuffers
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::prepareLinearBuffers(BuilderRef b) {

    for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
        const auto streamSet = target(e, mBufferGraph);
        const BufferNode & bn = mBufferGraph[streamSet];
        const StreamSetBuffer * const buffer = bn.Buffer;
        if (bn.isOwned() && buffer->isLinear()) {
            Value * const produced = mInitiallyProducedItemCount[streamSet];
            Value * consumed = produced;
            if (LLVM_UNLIKELY(bn.NonLocal)) {
                consumed = mInitialConsumedItemCount[streamSet];
            }
            const BufferRateData & br = mBufferGraph[e];
            buffer->prepareLinearBuffer(b, produced, consumed, br.LookBehind);
        }
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getVirtualBaseAddress
 *
 * Returns the address of the "zeroth" item of the (logically-unbounded) stream set.
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getVirtualBaseAddress(BuilderRef b,
                                                const BufferRateData & rateData,
                                                const StreamSetBuffer * const buffer,
                                                Value * position) const {
    assert ("buffer cannot be null!" && buffer);
    Constant * const LOG_2_BLOCK_WIDTH = b->getSize(floor_log2(b->getBitBlockWidth()));
    Constant * const ZERO = b->getSize(0);
    PointerType * const bufferType = buffer->getPointerType();
    Value * const blockIndex = b->CreateLShr(position, LOG_2_BLOCK_WIDTH);
    Value * const baseAddress = buffer->getBaseAddress(b);

    const Binding & binding = rateData.Binding;

    if (LLVM_UNLIKELY(CheckAssertions)) {
        b->CreateAssert(baseAddress, "%s.%s: baseAddress cannot be null",
                        mCurrentKernelName,
                        b->GetString(binding.getName()));
    }
    Value * const address = buffer->getStreamLogicalBasePtr(b, baseAddress, ZERO, blockIndex);
    return b->CreatePointerCast(address, bufferType);
}


/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getInputVirtualBaseAddresses
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::getInputVirtualBaseAddresses(BuilderRef b, Vec<Value *> & baseAddresses) const {
    for (const auto e : make_iterator_range(in_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & rt = mBufferGraph[e];
        assert (rt.Port.Type == PortType::Input);
        PHINode * processed = nullptr;
        if (mAlreadyProcessedDeferredPhi(rt.Port)) {
            processed = mAlreadyProcessedDeferredPhi(rt.Port);
        } else {
            processed = mAlreadyProcessedPhi(rt.Port);
        }
        const auto buffer = source(e, mBufferGraph);
        const BufferNode & bn = mBufferGraph[buffer];
        assert (isFromCurrentFunction(b, bn.Buffer->getHandle()));
        baseAddresses[rt.Port.Number] = getVirtualBaseAddress(b, rt, bn.Buffer, processed);
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getInputPortIndex
 ** ------------------------------------------------------------------------------------------------------------- */
BOOST_FORCEINLINE unsigned PipelineCompiler::getInputPortIndex(const unsigned kernel, const StreamSetPort port) const {
    assert (port.Type == PortType::Input);
    const auto key = std::pair<unsigned, unsigned>(kernel, port.Number);
    const auto f = mInputPortSet.lower_bound(key);
    assert(f != mInputPortSet.end() && *f == key);
    const auto i = f - mInputPortSet.begin();
    assert(mInputPortSet.nth(i) == f);
    return i;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getOutputPortIndex
 ** ------------------------------------------------------------------------------------------------------------- */
BOOST_FORCEINLINE unsigned PipelineCompiler::getOutputPortIndex(const unsigned kernel, const StreamSetPort port) const {
    assert(port.Type == PortType::Output);
    const auto key = std::pair<unsigned, unsigned>(kernel, port.Number);
    const auto f = mOutputPortSet.lower_bound(key);
    assert(f != mOutputPortSet.end() && *f == key);
    const auto i = f - mOutputPortSet.begin();
    assert(mOutputPortSet.nth(i) == f);
    return i;
}

} // end of kernel namespace

#endif // BUFFER_ALLOCATION_HPP
