#ifndef IO_CALCULATION_LOGIC_HPP
#define IO_CALCULATION_LOGIC_HPP

#include "pipeline_compiler.hpp"
#include <llvm/Support/ErrorHandling.h>

// TODO: add in assertions to prove whether all countable rate pipeline I/O was satisfied in the single iteration
// Is it sufficient to verify symbolic rate of the pipeline matches the rate of the I/O?

// TODO: if a popcount ref stream is zero extended, the current partial sum replacement would not work correctly
// since the equivalent for it would be to repeat the final number infinitely.

namespace kernel {

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief determineNumOfLinearStrides
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::determineNumOfLinearStrides(BuilderRef b) {

    mFixedRateLCM = getFixedRateLCM(mKernel);

    // If this kernel does not have an explicit do final segment, then we want to know whether this stride will
    // be the final stride of the kernel. (i.e., that it will be flagged as terminated after executing the kernel
    // code.) For this to occur, at least one of the inputs must be closed and we must pass all of the data from
    // that closed input stream to the kernel. It is possible for a stream to be closed but to have more data
    // left to process (either due to some data being divided across a buffer boundary or because another stream
    // has less data (relatively speaking) than the closed stream.

    if (LLVM_UNLIKELY(DebugOptionIsSet(codegen::EnableBlockingIOCounter) || DebugOptionIsSet(codegen::TraceBlockedIO))) {
        mBranchToLoopExit = b->getFalse();
    }

    mNumOfLinearStrides = nullptr;

//    if (mIsPartitionRoot) {
//        mNumOfLinearStrides = nullptr;
//    } else {
//        const auto diff = (MaximumNumOfStrides[mKernelId] / MaximumNumOfStrides[mPartitionRootKernelId]);
//        Value * numOfStrides = b->CreateCeilUMulRate(mNumOfPartitionStrides, diff);
//        if (mMayHaveNonLinearIO) {
//            numOfStrides = b->CreateSub(numOfStrides, mCurrentNumOfStrides);
//        }
//        mNumOfLinearStrides = numOfStrides;
//    }

    if (mIsPartitionRoot || mMayHaveNonLinearIO) {
        for (const auto e : make_iterator_range(in_edges(mKernelId, mBufferGraph))) {
            const BufferRateData & br = mBufferGraph[e];
            const auto streamSet = source(e, mBufferGraph);
            const BufferNode & bn = mBufferGraph[streamSet];
            if (mIsPartitionRoot || bn.NonLocal || !bn.Linear) {
                checkForSufficientInputData(b, br.Port);
            }
        }
        mNumOfLinearStrides = nullptr;
        for (const auto e : make_iterator_range(in_edges(mKernelId, mBufferGraph))) {
            const BufferRateData & br = mBufferGraph[e];
            const auto streamSet = source(e, mBufferGraph);
            const BufferNode & bn = mBufferGraph[streamSet];
            if (mIsPartitionRoot || bn.NonLocal || !bn.Linear) {
                Value * const strides = getNumOfAccessibleStrides(b, br.Port);
                mNumOfLinearStrides = b->CreateUMin(mNumOfLinearStrides, strides);
            }
        }
    }
    if (mNumOfLinearStrides == nullptr) {
        mNumOfLinearStrides = b->getSize(1);
    }

    if (mMayHaveNonLinearIO) {
        for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
            const BufferRateData & br = mBufferGraph[e];
            Value * const strides = getNumOfWritableStrides(b, br.Port);
            mNumOfLinearStrides = b->CreateUMin(mNumOfLinearStrides, strides);
        }
        mUpdatedNumOfStrides = b->CreateAdd(mCurrentNumOfStrides, mNumOfLinearStrides);
    } else {
        mUpdatedNumOfStrides = mNumOfLinearStrides;
    }

    calculateItemCounts(b);

    for (const auto e : make_iterator_range(out_edges(mKernelId, mBufferGraph))) {
        const BufferRateData & output = mBufferGraph[e];
        ensureSufficientOutputSpace(b, output.Port);
    }

    // When tracing blocking I/O, test all I/O streams but do not execute the
    // kernel if any stream is insufficient.
    if (mBranchToLoopExit) {
        BasicBlock * const noStreamIsInsufficient = b->CreateBasicBlock("", mKernelCheckOutputSpace);
        b->CreateUnlikelyCondBr(mBranchToLoopExit, mKernelInsufficientInput, noStreamIsInsufficient);
        updatePHINodesForLoopExit(b);
        b->SetInsertPoint(noStreamIsInsufficient);
    }

    b->CreateBr(mKernelLoopCall);

}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief calculateItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::calculateItemCounts(BuilderRef b) {

    const auto numOfInputs = in_degree(mKernelId, mBufferGraph);
    const auto numOfOutputs = out_degree(mKernelId, mBufferGraph);

    // --- lambda function start
    auto phiOutItemCounts = [&](const Vec<Value *> & accessibleItems,
                               const Vec<Value *> & inputVirtualBaseAddress,
                               const Vec<Value *> & writableItems,
                               Value * const fixedRateFactor,
                               Value * const numOfReportedStrides,
                               Constant * const terminationSignal) {
        BasicBlock * const exitBlock = b->GetInsertBlock();
        for (unsigned i = 0; i < numOfInputs; ++i) {
            const auto port = StreamSetPort{ PortType::Input, i };
            mLinearInputItemsPhi(port)->addIncoming(accessibleItems[i], exitBlock);
            mInputVirtualBaseAddressPhi(port)->addIncoming(inputVirtualBaseAddress[i], exitBlock);
        }
        for (unsigned i = 0; i < numOfOutputs; ++i) {
            const auto port = StreamSetPort{ PortType::Output, i };
            mLinearOutputItemsPhi(port)->addIncoming(writableItems[i], exitBlock);
        }
        if (mFixedRateFactorPhi) { assert (fixedRateFactor);
            mFixedRateFactorPhi->addIncoming(fixedRateFactor, exitBlock);
        }
        if (mReportedNumOfStridesPhi) {
            mReportedNumOfStridesPhi->addIncoming(numOfReportedStrides, exitBlock);
        }
        mIsFinalInvocationPhi->addIncoming(terminationSignal, exitBlock);
    };
    // --- lambda function end


    Vec<Value *> accessibleItems(numOfInputs);

    Vec<Value *> inputVirtualBaseAddress(numOfInputs, nullptr);

    Vec<Value *> writableItems(numOfOutputs);

    Constant * const unterminated = getTerminationSignal(b, TerminationSignal::None);

    getInputVirtualBaseAddresses(b, inputVirtualBaseAddress);

    assert ("unbounded zero extended input?" && ((mHasZeroExtendedInput == nullptr) || mIsBounded));

    if (LLVM_LIKELY(in_degree(mKernelId, mBufferGraph) > 0)) {

        const auto prefix = makeKernelName(mKernelId);
        BasicBlock * const enteringNonFinalSegment = b->CreateBasicBlock(prefix + "_nonFinalSegment", mKernelCheckOutputSpace);
        BasicBlock * const enteringFinalStride = b->CreateBasicBlock(prefix + "_finalStride", mKernelCheckOutputSpace);
        Value * const isFinal = determineIsFinal(b); assert (isFinal);

        Vec<Value *> zeroExtendedInputVirtualBaseAddress(numOfInputs, nullptr);

        BasicBlock * nonZeroExtendExit = nullptr;
        BasicBlock * zeroExtendExit = nullptr;

        /// -------------------------------------------------------------------------------------
        /// HANDLE ZERO EXTENSION
        /// -------------------------------------------------------------------------------------

        if (mHasZeroExtendedInput) {
            BasicBlock * const checkFinal =
                b->CreateBasicBlock(prefix + "_checkFinal", enteringNonFinalSegment);
            Value * const isFinalOrZeroExtended = b->CreateOr(mHasZeroExtendedInput, isFinal);
            nonZeroExtendExit = b->GetInsertBlock();
            b->CreateUnlikelyCondBr(isFinalOrZeroExtended, checkFinal, enteringNonFinalSegment);

            b->SetInsertPoint(checkFinal);
            Value * const zeroExtendSpace = allocateLocalZeroExtensionSpace(b, enteringNonFinalSegment);
            getZeroExtendedInputVirtualBaseAddresses(b, inputVirtualBaseAddress, zeroExtendSpace, zeroExtendedInputVirtualBaseAddress);
            zeroExtendExit = b->GetInsertBlock();
            b->CreateCondBr(isFinal, enteringFinalStride, enteringNonFinalSegment);

        } else {
            b->CreateUnlikelyCondBr(isFinal, enteringFinalStride, enteringNonFinalSegment);
        }

        /// -------------------------------------------------------------------------------------
        /// KERNEL ENTERING FINAL STRIDE
        /// -------------------------------------------------------------------------------------

        b->SetInsertPoint(enteringFinalStride);
        Value * fixedItemFactor, * maxNumOfOutputStrides;

        std::tie(fixedItemFactor, maxNumOfOutputStrides) = calculateFinalItemCounts(b, accessibleItems, writableItems);
        // if we have a potentially zero-extended buffer, use that; otherwise select the normal buffer
        Vec<Value *> truncatedVirtualBaseAddress(numOfInputs);
        for (unsigned i = 0; i != numOfInputs; ++i) {
            Value * const ze = zeroExtendedInputVirtualBaseAddress[i];
            Value * const vba = inputVirtualBaseAddress[i];
            truncatedVirtualBaseAddress[i] = ze ? ze : vba;
        }
        zeroInputAfterFinalItemCount(b, accessibleItems, truncatedVirtualBaseAddress);
        Constant * const completed = getTerminationSignal(b, TerminationSignal::Completed);
        phiOutItemCounts(accessibleItems, truncatedVirtualBaseAddress, writableItems, fixedItemFactor, maxNumOfOutputStrides, completed);
        b->CreateBr(mKernelCheckOutputSpace);

        /// -------------------------------------------------------------------------------------
        /// KERNEL ENTERING NON-FINAL SEGMENT
        /// -------------------------------------------------------------------------------------

        b->SetInsertPoint(enteringNonFinalSegment);
        if (mHasZeroExtendedInput) {
            for (unsigned i = 0; i != numOfInputs; ++i) {
                Value * const ze = zeroExtendedInputVirtualBaseAddress[i];
                if (ze) {
                    PHINode * const phi = b->CreatePHI(ze->getType(), 2);
                    phi->addIncoming(inputVirtualBaseAddress[i], nonZeroExtendExit);
                    phi->addIncoming(ze, zeroExtendExit);
                    inputVirtualBaseAddress[i] = phi;
                }
            }
        }
    }

    Value * const nonFinalFactor = calculateNonFinalItemCounts(b, accessibleItems, writableItems);
    phiOutItemCounts(accessibleItems, inputVirtualBaseAddress, writableItems, nonFinalFactor, mNumOfLinearStrides, unterminated);
    b->CreateBr(mKernelCheckOutputSpace);

    b->SetInsertPoint(mKernelCheckOutputSpace);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief checkForSufficientInputData
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::checkForSufficientInputData(BuilderRef b, const StreamSetPort inputPort) {
    Value * const accessible = getAccessibleInputItems(b, inputPort); assert (accessible);
    Value * const strideLength = getInputStrideLength(b, inputPort);
    Value * const required = addLookahead(b, inputPort, strideLength); assert (required);
    const auto prefix = makeBufferName(mKernelId, inputPort);
    #ifdef PRINT_DEBUG_MESSAGES
    debugPrint(b, prefix + "_required = %" PRIu64, required);
    #endif
    Value * const hasEnough = b->CreateICmpUGE(accessible, required);
    Value * const closed = isClosed(b, inputPort);
    #ifdef PRINT_DEBUG_MESSAGES
    debugPrint(b, prefix + "_closed = %" PRIu8, closed);
    #endif

    Value * const sufficientInput = b->CreateOr(hasEnough, closed);

    BasicBlock * const hasInputData = b->CreateBasicBlock(prefix + "_hasInputData", mKernelCheckOutputSpace);

    BasicBlock * recordBlockedIO = nullptr;
    BasicBlock * insufficentIO = mKernelInsufficientInput;

    if (mBranchToLoopExit) {
        recordBlockedIO = b->CreateBasicBlock(prefix + "_recordBlockedIO", mKernelInsufficientInput);
        insufficentIO = recordBlockedIO;
    }

    BasicBlock * const entryBlock = b->GetInsertBlock();


    Value * test = sufficientInput;
    Value * insufficient = mBranchToLoopExit;
    if (mBranchToLoopExit) {
        // do not record the block if this not the first execution of the
        // kernel but ensure that the system knows at least one failed.
        test = b->CreateOr(sufficientInput, mExecutedAtLeastOncePhi);
        insufficient = b->CreateOr(mBranchToLoopExit, b->CreateNot(sufficientInput));
    }

    b->CreateLikelyCondBr(test, hasInputData, insufficentIO);

    // When tracing blocking I/O, test all I/O streams but do not execute
    // the kernel if any stream is insufficient.
    if (mBranchToLoopExit) {
        b->SetInsertPoint(recordBlockedIO);
        recordBlockingIO(b, inputPort);
        BasicBlock * const exitBlock = b->GetInsertBlock();
        b->CreateBr(hasInputData);

        b->SetInsertPoint(hasInputData);
        IntegerType * const boolTy = b->getInt1Ty();

        PHINode * const anyInsufficient = b->CreatePHI(boolTy, 2);
        anyInsufficient->addIncoming(insufficient, entryBlock);
        anyInsufficient->addIncoming(b->getTrue(), exitBlock);
        mBranchToLoopExit = anyInsufficient;
    } else {
        if (mExhaustedPipelineInputPhi) {
            Value * exhausted = mExhaustedInput;
            if (LLVM_UNLIKELY(mHasPipelineInput.test(inputPort.Number))) {
                exhausted = b->getTrue();
            }
            mExhaustedPipelineInputPhi->addIncoming(exhausted, entryBlock);
        }
        b->SetInsertPoint(hasInputData);
    }

    mAccessibleInputItems(inputPort) = accessible;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief determineIsFinal
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::determineIsFinal(BuilderRef b) const {
    if (LLVM_LIKELY(mIsBounded)) {
        return b->CreateIsNull(mNumOfLinearStrides, "isFinal");
    } else { // check whether any stream is fully consumed; if so, we cannot execute any further segments
        Value * isFinal = nullptr;
        for (const auto e : make_iterator_range(in_edges(mKernelId, mBufferGraph))) {
            const BufferRateData & br =  mBufferGraph[e];
            if (LLVM_UNLIKELY(br.ZeroExtended)) {
                continue;
            }
            Value * const closed = isClosed(b, br.Port);
            assert (closed);
            Value * fullyConsumed = closed;
            if (LLVM_LIKELY(mMayHaveNonLinearIO)) {
                Value * const processed = mAlreadyProcessedPhi(br.Port);
                Value * const accessible = mAccessibleInputItems(br.Port);
                Value * const total = b->CreateAdd(processed, accessible);
                Value * const avail = getLocallyAvailableItemCount(b, br.Port);
                Value * const fullyReadable = b->CreateICmpEQ(total, avail);
                fullyConsumed = b->CreateAnd(closed, fullyReadable);
            }
            if (isFinal) {
                isFinal = b->CreateOr(isFinal, fullyConsumed);
            } else {
                isFinal = fullyConsumed;
            }
        }
        assert (isFinal && "non-zero-extended stream is required");
        if (mHasExplicitFinalPartialStride) {
            Value * const noMoreStrides = b->CreateIsNull(mNumOfLinearStrides);
            isFinal = b->CreateAnd(isFinal, noMoreStrides);
        }
        isFinal->setName("isFinal");
        return isFinal;
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getAccessibleInputItems
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getAccessibleInputItems(BuilderRef b, const StreamSetPort inputPort, const bool useOverflow) {
    const StreamSetBuffer * const buffer = getInputBuffer(inputPort);
    Value * const available = getLocallyAvailableItemCount(b, inputPort);
    Value * const processed = mAlreadyProcessedPhi(inputPort);
    Value * lookAhead = nullptr;
    if (LLVM_LIKELY(useOverflow)) {
        const auto size = getLookAhead(getInputBufferVertex(inputPort));
        if (size) {
            Value * const closed = isClosed(b, inputPort);
            ConstantInt * const ZERO = b->getSize(0);
            ConstantInt * const SIZE = b->getSize(size);
            lookAhead = b->CreateSelect(closed, ZERO, SIZE);
        }
    }
    const BufferRateData & rateData = mBufferGraph[getInput(mKernelId, inputPort)];


    const Binding & input = rateData.Binding;
    Value * accessible = buffer->getLinearlyAccessibleItems(b, processed, available, lookAhead);

    #ifndef DISABLE_ZERO_EXTEND
    if (LLVM_UNLIKELY(rateData.ZeroExtended)) {
        // To zero-extend an input stream, we must first exhaust all input for this stream before
        // switching to a "zeroed buffer". The size of the buffer will be determined by the final
        // number of non-zero-extended strides.

        // NOTE: the producer of this stream will zero out all data after its last produced item
        // that can be read by a single iteration of any consuming kernel.

        Constant * const MAX_INT = ConstantInt::getAllOnesValue(b->getSizeTy());
        Value * const closed = isClosed(b, inputPort);
        Value * const exhausted = b->CreateICmpUGE(processed, available);
        Value * const useZeroExtend = b->CreateAnd(closed, exhausted);
        mIsInputZeroExtended(mKernelId, inputPort) = useZeroExtend;
        if (LLVM_LIKELY(mHasZeroExtendedInput == nullptr)) {
            mHasZeroExtendedInput = useZeroExtend;
        } else {
            mHasZeroExtendedInput = b->CreateOr(mHasZeroExtendedInput, useZeroExtend);
        }
        accessible = b->CreateSelect(useZeroExtend, MAX_INT, accessible);
    }
    #endif

    #ifdef PRINT_DEBUG_MESSAGES
    const auto prefix = makeBufferName(mKernelIndex, inputPort);
    debugPrint(b, prefix + "_available = %" PRIu64, available);
    debugPrint(b, prefix + "_processed = %" PRIu64, processed);
    debugPrint(b, prefix + "_accessible = %" PRIu64, accessible);
    if (lookAhead) {
        debugPrint(b, prefix + "_lookAhead = %" PRIu64, lookAhead);
    }
    #endif
    if (LLVM_UNLIKELY(mCheckAssertions)) {
        Value * sanityCheck = b->CreateICmpULE(processed, available);
        if (mIsInputZeroExtended(mKernelId, inputPort)) {
            sanityCheck = b->CreateOr(mIsInputZeroExtended(mKernelId, inputPort), sanityCheck);
        }
        b->CreateAssert(sanityCheck,
                        "%s.%s: processed count (%" PRIu64 ") exceeds total count (%" PRIu64 ")",
                        mKernelAssertionName,
                        b->GetString(input.getName()),
                        processed, available);
    }
    return accessible;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief ensureSufficientOutputSpace
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::ensureSufficientOutputSpace(BuilderRef b, const StreamSetPort  outputPort) {
    const auto bufferVertex = getOutputBufferVertex(outputPort);
    const BufferNode & bn = mBufferGraph[bufferVertex];
    if (LLVM_UNLIKELY(bn.isUnowned())) {
        return;
    }
    const StreamSetBuffer * const buffer = bn.Buffer;
    Value * const produced = mAlreadyProducedPhi(outputPort); assert (produced);
    Value * const consumed = mConsumedItemCount(outputPort); assert (consumed);
    Value * const required = mLinearOutputItemsPhi(outputPort);
    ConstantInt * copyBack = nullptr;
    const auto size = getCopyBack(getOutputBufferVertex(outputPort));
    if (size) {
        copyBack = b->getSize(size);
    }

    Value * const remaining = buffer->getLinearlyWritableItems(b, produced, consumed, copyBack);

    const auto prefix = makeBufferName(mKernelId, outputPort);

    #ifdef PRINT_DEBUG_MESSAGES
    debugPrint(b, prefix + "_produced = %" PRIu64, produced);
    debugPrint(b, prefix + "_consumed = %" PRIu64, consumed);
    debugPrint(b, prefix + "_required = %" PRIu64, required);
    debugPrint(b, prefix + "_remaining = %" PRIu64, remaining);
    #endif

    BasicBlock * const expandBuffer = b->CreateBasicBlock(prefix + "_expandBuffer", mKernelCheckOutputSpace);
    BasicBlock * const expanded = b->CreateBasicBlock(prefix + "_expandedBuffer", mKernelCheckOutputSpace);

    Value * const hasEnoughSpace = b->CreateICmpULE(required, remaining);
    b->CreateLikelyCondBr(hasEnoughSpace, expanded, expandBuffer);

    b->SetInsertPoint(expandBuffer);
    Value * const cycleCounterAccumulator = getBufferExpansionCycleCounter(b);
    Value * cycleCounterStart = nullptr;
    if (cycleCounterAccumulator) {
        cycleCounterStart = b->CreateReadCycleCounter();
    }

    // TODO: we need to calculate the total amount required assuming we process all input. This currently
    // has a flaw in which if the input buffers had been expanded sufficiently yet processing had been
    // held back by some input stream, we may end up expanding twice in the same iteration of this kernel,
    // which could result in free'ing the "old" buffer twice.

    buffer->reserveCapacity(b, produced, consumed, required, copyBack);

    recordBufferExpansionHistory(b, outputPort, buffer);
    if (cycleCounterAccumulator) {
        Value * const cycleCounterEnd = b->CreateReadCycleCounter();
        Value * const duration = b->CreateSub(cycleCounterEnd, cycleCounterStart);
        Value * const accum = b->CreateAdd(b->CreateLoad(cycleCounterAccumulator), duration);
        b->CreateStore(accum, cycleCounterAccumulator);
    }
    b->CreateBr(expanded);

    b->SetInsertPoint(expanded);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getWritableOutputItems
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getWritableOutputItems(BuilderRef b, const StreamSetPort outputPort, const bool useOverflow) {
    const Binding & output = getOutputBinding(outputPort);
    const StreamSetBuffer * const buffer = getOutputBuffer(outputPort);
    Value * const produced = mAlreadyProducedPhi(outputPort); assert (produced);
    Value * const consumed = mConsumedItemCount(outputPort); assert (consumed);
    #ifdef PRINT_DEBUG_MESSAGES
    const auto prefix = makeBufferName(mKernelIndex, outputPort);
    debugPrint(b, prefix + "_produced = %" PRIu64, produced);
    debugPrint(b, prefix + "_consumed = %" PRIu64, consumed);
    debugPrint(b, prefix + "_capacity = %" PRIu64, buffer->getCapacity(b));
    #endif
    if (LLVM_UNLIKELY(mCheckAssertions)) {
        Value * const sanityCheck = b->CreateICmpULE(consumed, produced);
        b->CreateAssert(sanityCheck,
                        "%s.%s: consumed count (%" PRIu64 ") exceeds produced count (%" PRIu64 ")",
                        mKernelAssertionName,
                        b->GetString(output.getName()),
                        consumed, produced);
    }
    ConstantInt * copyBack = nullptr;
    if (LLVM_LIKELY(useOverflow)) {
        const auto size = getCopyBack(getOutputBufferVertex(outputPort));
        if (size) {
            copyBack = b->getSize(size);
        }
    }
    Value * const writable = buffer->getLinearlyWritableItems(b, produced, consumed, copyBack);
    #ifdef PRINT_DEBUG_MESSAGES
    debugPrint(b, prefix + "_writable = %" PRIu64, writable);
    #endif
    return writable;
}


/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getNumOfAccessibleStrides
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getNumOfAccessibleStrides(BuilderRef b, const StreamSetPort inputPort) {
    const Binding & input = getInputBinding(inputPort);
    const ProcessingRate & rate = input.getRate();
    Value * numOfStrides = nullptr;
    if (LLVM_UNLIKELY(rate.isPartialSum())) {
        numOfStrides = getMaximumNumOfPartialSumStrides(b, inputPort);
    } else if (LLVM_UNLIKELY(rate.isGreedy())) {
        return nullptr;
    } else {
        Value * const accessible = mAccessibleInputItems(inputPort); assert (accessible);
        Value * const strideLength = getInputStrideLength(b, inputPort); assert (strideLength);
        numOfStrides = b->CreateUDiv(subtractLookahead(b, inputPort, accessible), strideLength);
    }
    #ifdef PRINT_DEBUG_MESSAGES
    const auto prefix = makeBufferName(mKernelIndex, inputPort);
    #endif
    Value * const ze = mIsInputZeroExtended(mKernelId, inputPort);
    if (ze) {
        numOfStrides = b->CreateSelect(ze, mNumOfLinearStrides, numOfStrides);
    }
    #ifdef PRINT_DEBUG_MESSAGES
    debugPrint(b, "< " + prefix + "_numOfStrides = %" PRIu64, numOfStrides);
    #endif
    return numOfStrides;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getNumOfWritableStrides
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getNumOfWritableStrides(BuilderRef b, const StreamSetPort outputPort) {

    const auto bufferVertex = getOutputBufferVertex(outputPort);
    const BufferNode & bn = mBufferGraph[bufferVertex];
    if (LLVM_UNLIKELY(bn.isUnowned())) {
        return nullptr;
    }
    const Binding & output = getOutputBinding(outputPort);
    Value * numOfStrides = nullptr;
    if (LLVM_UNLIKELY(output.getRate().isPartialSum())) {
        numOfStrides = getMaximumNumOfPartialSumStrides(b, outputPort);
    } else {
        Value * const writable = getWritableOutputItems(b, outputPort);
        mWritableOutputItems(outputPort) = writable;
        Value * const strideLength = getOutputStrideLength(b, outputPort);
        numOfStrides = b->CreateUDiv(writable, strideLength);
    }
    #ifdef PRINT_DEBUG_MESSAGES
    const auto prefix = makeBufferName(mKernelIndex, outputPort);
    debugPrint(b, "> " + prefix + "_numOfStrides = %" PRIu64, numOfStrides);
    #endif
    return numOfStrides;
}


/** ------------------------------------------------------------------------------------------------------------- *
 * @brief calculateNonFinalItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::calculateNonFinalItemCounts(BuilderRef b, Vec<Value *> & accessibleItems, Vec<Value *> & writableItems) {
    assert (mNumOfLinearStrides);
    Value * fixedRateFactor = nullptr;
    if (mFixedRateFactorPhi) {
        const Rational stride(mKernel->getStride());
        fixedRateFactor  = b->CreateMulRate(mNumOfLinearStrides, stride * mFixedRateLCM);
    }
    const auto numOfInputs = accessibleItems.size();
    for (unsigned i = 0; i < numOfInputs; ++i) {
        accessibleItems[i] = calculateNumOfLinearItems(b, StreamSetPort{PortType::Input, i}, mNumOfLinearStrides);
    }
    const auto numOfOutputs = writableItems.size();
    for (unsigned i = 0; i < numOfOutputs; ++i) {
        writableItems[i] = calculateNumOfLinearItems(b, StreamSetPort{PortType::Output, i}, mNumOfLinearStrides);
    }
    return fixedRateFactor;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief calculateFinalItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
std::pair<Value *, Value *> PipelineCompiler::calculateFinalItemCounts(BuilderRef b, Vec<Value *> & accessibleItems, Vec<Value *> & writableItems) {
    const auto numOfInputs = accessibleItems.size();

    auto summarizeItemCountAdjustment = [](const Binding & binding, int k) {
        for (const Attribute & attr : binding.getAttributes()) {
            switch (attr.getKind()) {
                case AttrId::Add:
                    k += attr.amount();
                    break;
                case AttrId::Truncate:
                    k -= attr.amount();
                    break;
                default: break;
            }
        }
        return k;
    };

    for (unsigned i = 0; i < numOfInputs; ++i) {
        const auto inputPort = StreamSetPort{ PortType::Input, i };
        Value * accessible = mAccessibleInputItems(inputPort);
        const Binding & input = getInputBinding(inputPort);
        const auto k = summarizeItemCountAdjustment(input, 0);
        if (LLVM_UNLIKELY(k != 0)) {
            Value * selected;
            if (LLVM_LIKELY(k > 0)) {
                selected = b->CreateAdd(accessible, b->getSize(k));
            } else  {
                selected = b->CreateSaturatingSub(accessible, b->getSize(-k));
            }
            accessible = b->CreateSelect(isClosedNormally(b, inputPort), selected, accessible);
        }
        accessibleItems[i] = accessible;
    }

    Value * principalFixedRateFactor = nullptr;
    for (unsigned i = 0; i < numOfInputs; ++i) {
        const auto inputPort = StreamSetPort{PortType::Input, i};
        const Binding & input = getInputBinding(inputPort);
        const ProcessingRate & rate = input.getRate();
        if (rate.isFixed() && LLVM_UNLIKELY(input.isPrincipal())) {
            Value * const accessible = accessibleItems[i];
            const auto factor = mFixedRateLCM / rate.getRate();
            principalFixedRateFactor = b->CreateMulRate(accessible, factor);
            break;
        }
    }

    #ifdef PRINT_DEBUG_MESSAGES
    if (principalFixedRateFactor) {
        debugPrint(b, makeKernelName(mKernelIndex) + "_principalFixedRateFactor = %" PRIu64, principalFixedRateFactor);
    }
    #endif

    for (unsigned i = 0; i < numOfInputs; ++i) {
        Value * accessible = accessibleItems[i];
        const auto inputPort = StreamSetPort{ PortType::Input, i };
        if (LLVM_UNLIKELY(mIsInputZeroExtended(inputPort) != nullptr)) {
            // If this input stream is zero extended, the current input items will be MAX_INT.
            // However, since we're now in the final stride, so we can bound the stream to:
            const Binding & input = getInputBinding(inputPort);
            const ProcessingRate & rate = input.getRate();
            if (principalFixedRateFactor && rate.isFixed()) {
                const auto factor = rate.getRate() / mFixedRateLCM;
                accessible = b->CreateCeilUMulRate(principalFixedRateFactor, factor);
            } else {
                Value * maxItems = b->CreateAdd(mAlreadyProcessedPhi(inputPort), mFirstInputStrideLength(inputPort));
                // But since we may not necessarily be in our zero extension region, we must first
                // test whether we are:
                accessible = b->CreateSelect(mIsInputZeroExtended(inputPort), maxItems, accessible);
            }
        }
        accessibleItems[i] = accessible;
    }

    Value * minFixedRateFactor = principalFixedRateFactor;
    if (principalFixedRateFactor == nullptr) {
        for (unsigned i = 0; i < numOfInputs; ++i) {
            const auto inputPort = StreamSetPort{PortType::Input, i};
            const Binding & input = getInputBinding(inputPort);
            const ProcessingRate & rate = input.getRate();
            if (rate.isFixed()) {
                Value * const fixedRateFactor =
                    b->CreateMulRate(accessibleItems[i], mFixedRateLCM / rate.getRate());
                minFixedRateFactor =
                    b->CreateUMin(minFixedRateFactor, fixedRateFactor);
            }
        }
    }

    if (minFixedRateFactor) {
        // truncate any fixed rate input down to the length of the shortest stream
        for (unsigned i = 0; i < numOfInputs; ++i) {
            const auto inputPort = StreamSetPort{PortType::Input, i};
            const Binding & input = getInputBinding(inputPort);
            const ProcessingRate & rate = input.getRate();

            if (rate.isFixed()) {
                const auto factor = rate.getRate() / mFixedRateLCM;
                Value * calculated = b->CreateCeilUMulRate(minFixedRateFactor, factor);
                auto addPort = in_edges(mKernelId, mAddGraph).first + i;
                const int k = mAddGraph[*addPort];

                // ... but ensure that it reflects whether it was produced with an
                // Add/Truncate attributed rate.
                if (k) {

                    const auto stride = mKernel->getStride();

                    // (x + (g/h)) * (c/d) = (xh + g) * c/hd
                    Constant * const h = b->getSize(stride);
                    Value * const xh = b->CreateMul(minFixedRateFactor, h);
                    Constant * const g = b->getSize(std::abs(k));
                    Value * y;
                    if (k > 0) {
                        y = b->CreateAdd(xh, g);
                    } else {
                        y = b->CreateSub(xh, g);
                    }
                    const Rational r = factor / Rational{stride};
                    Value * const z = b->CreateCeilUMulRate(y, r);
                    calculated = b->CreateSelect(isClosedNormally(b, inputPort), z, calculated);
                }
//                if (LLVM_UNLIKELY(mCheckAssertions)) {
//                    Value * const accessible = accessibleItems[i];
//                    Value * correctItemCount = b->CreateICmpULE(calculated, accessible);
//                    Value * const zeroExtended = mIsInputZeroExtended(mKernelIndex, inputPort);
//                    if (LLVM_UNLIKELY(zeroExtended != nullptr)) {
//                        correctItemCount = b->CreateOr(correctItemCount, zeroExtended);
//                    }
//                    b->CreateAssert(correctItemCount,
//                                    "%s.%s: final calculated rate item count (%" PRIu64 ") "
//                                    "exceeds accessible item count (%" PRIu64 ")",
//                                    mKernelAssertionName,
//                                    b->GetString(input.getName()),
//                                    calculated, accessible);
//                }
                accessibleItems[i] = calculated;
            }
            #ifdef PRINT_DEBUG_MESSAGES
            const auto prefix = makeBufferName(mKernelIndex, inputPort);
            debugPrint(b, prefix + ".accessible' = %" PRIu64, accessibleItems[i]);
            #endif
        }
    }

    Constant * const ONE = b->getSize(1);
    Value * maxNumOfOutputStrides = ONE;
    if (!mHasExplicitFinalPartialStride && minFixedRateFactor) {
        maxNumOfOutputStrides = b->CreateAdd(mNumOfLinearStrides, ONE);
    }

    const auto numOfOutputs = writableItems.size();
    for (unsigned i = 0; i < numOfOutputs; ++i) {
        const StreamSetPort port{ PortType::Output, i };
        const Binding & output = getOutputBinding(port);
        const ProcessingRate & rate = output.getRate();

        Value * writable = nullptr;
        if (rate.isFixed() && minFixedRateFactor) {
            const auto factor = rate.getRate() / mFixedRateLCM;
            writable = b->CreateCeilUMulRate(minFixedRateFactor, factor);
        } else {
            writable = calculateNumOfLinearItems(b, port, maxNumOfOutputStrides);
        }

        // update the final item counts with any Add/RoundUp attributes
        for (const Attribute & attr : output.getAttributes()) {
            switch (attr.getKind()) {
                case AttrId::Add:
                    writable = b->CreateAdd(writable, b->getSize(attr.amount()));
                    break;
                case AttrId::Truncate:
                    writable = b->CreateSaturatingSub(writable, b->getSize(attr.amount()));
                    break;
                case AttrId::RoundUpTo:
                    writable = b->CreateRoundUp(writable, b->getSize(attr.amount()));
                    break;
                default: break;
            }
        }
        writableItems[i] = writable;
        #ifdef PRINT_DEBUG_MESSAGES
        const auto prefix = makeBufferName(mKernelIndex, StreamSetPort{PortType::Output, i});
        debugPrint(b, prefix + ".writable' = %" PRIu64, writableItems[i]);
        #endif
    }
    return std::make_pair(minFixedRateFactor, maxNumOfOutputStrides);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getInputStrideLength
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getInputStrideLength(BuilderRef b, const StreamSetPort inputPort) {
    if (mFirstInputStrideLength(inputPort)) {
        return mFirstInputStrideLength(inputPort);
    } else {
        Value * const strideLength = getFirstStrideLength(b, mKernelId, inputPort);
        mFirstInputStrideLength(inputPort) = strideLength;
        return strideLength;
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getOutputStrideLength
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getOutputStrideLength(BuilderRef b, const StreamSetPort outputPort) {
    if (mFirstOutputStrideLength(outputPort)) {
        return mFirstOutputStrideLength(outputPort);
    } else {
        Value * const strideLength = getFirstStrideLength(b, mKernelId, outputPort);
        mFirstOutputStrideLength(outputPort) = strideLength;
        return strideLength;
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getPartialSumItemCount
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getPartialSumItemCount(BuilderRef b, const size_t kernel, const StreamSetPort port, Value * const offset) const {
    const auto ref = getReference(kernel, port);
    assert (ref.Type == PortType::Input);

    const StreamSetBuffer * const buffer = getInputBuffer(kernel, ref);
    Value * prior = nullptr;
    if (port.Type == PortType::Input) {
        prior = mAlreadyProcessedPhi(kernel, port);
    } else { // if (port.Type == PortType::Output) {
        prior = mAlreadyProducedPhi(kernel, port);
    }
    assert (prior);

    Constant * const ZERO = b->getSize(0);
    Value * position = mAlreadyProcessedPhi(mKernelId, ref);

    if (offset) {
        if (LLVM_UNLIKELY(mCheckAssertions)) {
            const auto & binding = getBinding(port);
            b->CreateAssert(b->CreateICmpNE(offset, ZERO),
                            "%s.%s: partial sum offset must be non-zero",
                            mKernelAssertionName,
                            b->GetString(binding.getName()));
        }
        Constant * const ONE = b->getSize(1);
        position = b->CreateAdd(position, b->CreateSub(offset, ONE));
    }

    Value * const currentPtr = buffer->getRawItemPointer(b, ZERO, position);
    Value * current = b->CreateLoad(currentPtr);

    if (mBranchToLoopExit) {
        current = b->CreateSelect(mBranchToLoopExit, prior, current);
    }
    if (LLVM_UNLIKELY(mCheckAssertions)) {
        const auto & binding = getBinding(port);
        b->CreateAssert(b->CreateICmpULE(prior, current),
                        "%s.%s: partial sum is not non-decreasing "
                        "(prior %" PRIu64 " > current %" PRIu64 ")",
                        mKernelAssertionName,
                        b->GetString(binding.getName()),
                        prior, current);
    }
    return b->CreateSub(current, prior);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getMaximumNumOfPartialSumStrides
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getMaximumNumOfPartialSumStrides(BuilderRef b, const StreamSetPort port) {
    IntegerType * const sizeTy = b->getSizeTy();
    Constant * const ZERO = b->getSize(0);
    Constant * const ONE = b->getSize(1);
    Constant * const MAX_INT = ConstantInt::getAllOnesValue(sizeTy);


    Value * initialItemCount = nullptr;
    Value * sourceItemCount = nullptr;
    Value * peekableItemCount = nullptr;
    Value * minimumItemCount = MAX_INT;

    if (port.Type == PortType::Input) {
        initialItemCount = mAlreadyProcessedPhi(mKernelId, port);
        Value * const accessible = mAccessibleInputItems(mKernelId, port);
        if (requiresLookAhead(getInputBufferVertex(port))) {
            Value * const nonOverflowItems = getAccessibleInputItems(b, port, false);
            sourceItemCount = b->CreateAdd(initialItemCount, nonOverflowItems);
            peekableItemCount = b->CreateAdd(initialItemCount, accessible);
            minimumItemCount = getInputStrideLength(b, port);
        } else {
            sourceItemCount = b->CreateAdd(initialItemCount, accessible);
        }
        sourceItemCount = subtractLookahead(b, port, sourceItemCount);
    } else { // if (port.Type == PortType::Output) {
        initialItemCount = mAlreadyProducedPhi(mKernelId, port);
        Value * const writable = getWritableOutputItems(b, port, true);
        if (requiresCopyBack(getOutputBufferVertex(port))) {
            Value * const nonOverflowItems = getWritableOutputItems(b, port, false);
            sourceItemCount = b->CreateAdd(initialItemCount, nonOverflowItems);
            peekableItemCount = b->CreateAdd(initialItemCount, writable);
            minimumItemCount = getOutputStrideLength(b, port);
        } else {
            sourceItemCount = b->CreateAdd(initialItemCount, writable);
        }
    }

    const auto ref = getReference(port);
    assert (ref.Type == PortType::Input);

    // get the popcount kernel's input rate so we can calculate the
    // step factor for this kernel's usage of pop count partial sum
    // stream.
    const auto refInput = getInput(mKernelId, ref);
    const BufferRateData & refInputRate = mBufferGraph[refInput];
    const auto refBufferVertex = getInputBufferVertex(ref);
    const auto refOuput = in_edge(refBufferVertex, mBufferGraph);
    const BufferRateData & refOutputRate = mBufferGraph[refOuput];
    const auto stepFactor = refInputRate.Maximum / refOutputRate.Maximum;

    assert (stepFactor.denominator() == 1);
    const auto step = stepFactor.numerator();
    Constant * const STEP = b->getSize(step);

    const StreamSetBuffer * const buffer = mBufferGraph[refBufferVertex].Buffer;
    const auto prefix = makeBufferName(mKernelId, ref) + "_readPartialSum";

    BasicBlock * const popCountLoop =
        b->CreateBasicBlock(prefix + "Loop", mKernelCheckOutputSpace);
    BasicBlock * const popCountLoopExit =
        b->CreateBasicBlock(prefix + "LoopExit", mKernelCheckOutputSpace);
    Value * const baseOffset = mAlreadyProcessedPhi(mKernelId, ref);
    Value * const baseAddress = buffer->getRawItemPointer(b, ZERO, baseOffset);
    BasicBlock * const popCountEntry = b->GetInsertBlock();
    Value * const initialStrideCount = b->CreateMul(mNumOfLinearStrides, STEP);

    b->CallPrintInt("initialStrideCount", initialStrideCount);

    if (peekableItemCount) {
        Value * const hasNonOverflowStride = b->CreateICmpUGE(sourceItemCount, minimumItemCount);
        b->CreateLikelyCondBr(hasNonOverflowStride, popCountLoop, popCountLoopExit);
    } else {
        b->CreateBr(popCountLoop);
    }

    // TODO: replace this with a parallel icmp check and bitscan? binary search with initial
    // check on the rightmost entry?

    b->SetInsertPoint(popCountLoop);
    PHINode * const numOfStrides = b->CreatePHI(sizeTy, 2);
    numOfStrides->addIncoming(initialStrideCount, popCountEntry);
    PHINode * const nextRequiredItems = b->CreatePHI(sizeTy, 2);
    nextRequiredItems->addIncoming(MAX_INT, popCountEntry);

    Value * const strideIndex = b->CreateSub(numOfStrides, STEP);

    b->CallPrintInt("strideIndex", strideIndex);

    Value * const ptr = b->CreateInBoundsGEP(baseAddress, strideIndex);
    Value * const requiredItems = b->CreateLoad(ptr);
    Value * const hasEnough = b->CreateICmpULE(requiredItems, sourceItemCount);

    // NOTE: popcount streams are produced with a 1 element lookbehind window.
    if (LLVM_UNLIKELY(mCheckAssertions)) {
        const Binding & input = getInputBinding(ref);
        Value * const inputName = b->GetString(input.getName());
        b->CreateAssert(b->CreateOr(b->CreateICmpUGE(numOfStrides, STEP), hasEnough),
                        "%s.%s: attempting to read invalid popcount entry",
                        mKernelAssertionName, inputName);
        b->CreateAssert(b->CreateICmpULE(initialItemCount, requiredItems),
                        "%s.%s: partial sum is not non-decreasing "
                        "(prior %" PRIu64 " > current %" PRIu64 ")",
                        mKernelAssertionName, inputName,
                        initialItemCount, requiredItems);
    }

    nextRequiredItems->addIncoming(requiredItems, popCountLoop);
    numOfStrides->addIncoming(strideIndex, popCountLoop);
    b->CreateCondBr(hasEnough, popCountLoopExit, popCountLoop);

    b->SetInsertPoint(popCountLoopExit);
    Value * finalNumOfStrides = numOfStrides;
    if (peekableItemCount) {
        PHINode * const numOfStridesPhi = b->CreatePHI(sizeTy, 2);
        numOfStridesPhi->addIncoming(ZERO, popCountEntry);
        numOfStridesPhi->addIncoming(numOfStrides, popCountLoop);
        PHINode * const requiredItemsPhi = b->CreatePHI(sizeTy, 2);
        requiredItemsPhi->addIncoming(ZERO, popCountEntry);
        requiredItemsPhi->addIncoming(requiredItems, popCountLoop);
        PHINode * const nextRequiredItemsPhi = b->CreatePHI(sizeTy, 2);
        nextRequiredItemsPhi->addIncoming(minimumItemCount, popCountEntry);
        nextRequiredItemsPhi->addIncoming(nextRequiredItems, popCountLoop);
        // Since we want to allow the stream to peek into the overflow but not start
        // in it, check to see if we can support one more stride by using it.
        Value * const endedPriorToBufferEnd = b->CreateICmpNE(requiredItemsPhi, sourceItemCount);
        Value * const canPeekIntoOverflow = b->CreateICmpULE(nextRequiredItemsPhi, peekableItemCount);
        Value * const useOverflow = b->CreateAnd(endedPriorToBufferEnd, canPeekIntoOverflow);
        finalNumOfStrides = b->CreateSelect(useOverflow, b->CreateAdd(numOfStridesPhi, ONE), numOfStridesPhi);
    }
    if (LLVM_UNLIKELY(mCheckAssertions)) {
        const Binding & binding = getInputBinding(ref);
        b->CreateAssert(b->CreateICmpNE(finalNumOfStrides, MAX_INT),
                        "%s.%s: attempting to use sentinal popcount entry",
                        mKernelAssertionName,
                        b->GetString(binding.getName()));
    }
    return finalNumOfStrides;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getFirstStrideLength
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getFirstStrideLength(BuilderRef b, const size_t kernel, const StreamSetPort port) {
    const Binding & binding = getBinding(kernel, port);
    const ProcessingRate & rate = binding.getRate();
    if (LLVM_LIKELY(rate.isFixed() || rate.isBounded())) {
        const Rational ub = rate.getUpperBound() * mKernel->getStride();
        if (LLVM_UNLIKELY(ub.denominator() != 1)) {
            std::string tmp;
            raw_string_ostream out(tmp);
            out << mKernel->getName() << "." << binding.getName()
                << ": rate upper-bound is not a multiple of kernel stride.";
            report_fatal_error(out.str());
        }
        return b->getSize(ub.numerator());
    } else if (rate.isPartialSum()) {
        return getPartialSumItemCount(b, kernel, port);
    } else if (rate.isGreedy()) {
        assert ("kernel cannot have a greedy output rate" && port.Type != PortType::Output);
        const Rational lb = rate.getLowerBound(); // * mKernel->getStride();
        const auto ilb = floor(lb);
        return b->getSize(ilb);
    } else if (rate.isRelative()) {
        Value * const baseRate = getFirstStrideLength(b, kernel, getReference(port));
        return b->CreateMulRate(baseRate, rate.getRate());
    }
    llvm_unreachable("unexpected rate type");
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief calculateNumOfLinearItems
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::calculateNumOfLinearItems(BuilderRef b, const StreamSetPort port, Value * const linearStrides) {
    const Binding & binding = getBinding(port);
    const ProcessingRate & rate = binding.getRate();
    if (rate.isFixed() || rate.isBounded()) {
        return b->CreateMulRate(linearStrides, rate.getUpperBound() * mKernel->getStride());
    } else if (rate.isGreedy()) {
        return mAccessibleInputItems(mKernelId, port);
    } else if (rate.isPartialSum()) {
        return getPartialSumItemCount(b, mKernelId, port, linearStrides);
    } else if (rate.isRelative()) {
        Value * const baseCount = calculateNumOfLinearItems(b, getReference(port), linearStrides);
        return b->CreateMulRate(baseCount, rate.getRate());
    }
    llvm_unreachable("unexpected rate type");
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief updatePHINodesForLoopExit
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::updatePHINodesForLoopExit(BuilderRef b) {

    BasicBlock * const exitBlock = b->GetInsertBlock();
    const auto numOfInputs = getNumOfStreamInputs(mKernelId);
    for (unsigned i = 0; i < numOfInputs; ++i) {
        const StreamSetPort port(PortType::Input, i);
        mUpdatedProcessedPhi(port)->addIncoming(mAlreadyProcessedPhi(port), exitBlock);
        if (mUpdatedProcessedDeferredPhi(port)) {
            mUpdatedProcessedDeferredPhi(port)->addIncoming(mAlreadyProcessedDeferredPhi(port), exitBlock);
        }
    }
    const auto numOfOutputs = getNumOfStreamOutputs(mKernelId);
    for (unsigned i = 0; i < numOfOutputs; ++i) {
        const StreamSetPort port(PortType::Output, i);
        mUpdatedProducedPhi(port)->addIncoming(mAlreadyProducedPhi(port), exitBlock);
        if (mUpdatedProducedDeferredPhi(port)) {
            mUpdatedProducedDeferredPhi(port)->addIncoming(mAlreadyProducedDeferredPhi(port), exitBlock);
        }
    }

}

}

#endif // IO_CALCULATION_LOGIC_HPP
