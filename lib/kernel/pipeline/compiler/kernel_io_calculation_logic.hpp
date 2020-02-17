#ifndef IO_CALCULATION_LOGIC_HPP
#define IO_CALCULATION_LOGIC_HPP

#include "pipeline_compiler.hpp"
#include <llvm/Support/ErrorHandling.h>

// TODO: add in assertions to prove whether all countable rate pipeline I/O was satisfied in the single iteration
// Is it sufficient to verify symbolic rate of the pipeline matches the rate of the I/O?


namespace kernel {

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief determineNumOfLinearStrides
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::determineNumOfLinearStrides(BuilderRef b) {

    // bound the number of strides by the maximum expected
    Constant * const maxStrides = b->getSize(mMaximumNumOfStrides);
    if (mMayHaveNonLinearIO) {
        mNumOfLinearStrides = b->CreateSub(maxStrides, mCurrentNumOfStrides);
    } else {
        mNumOfLinearStrides = maxStrides;
    }

    // If this kernel does not have an explicit do final segment, then we want to know whether this stride will
    // be the final stride of the kernel. (i.e., that it will be flagged as terminated after executing the kernel
    // code.) For this to occur, at least one of the inputs must be closed and we must pass all of the data from
    // that closed input stream to the kernel. It is possible for a stream to be closed but to have more data
    // left to process (either due to some data being divided across a buffer boundary or because another stream
    // has less data (relatively speaking) than the closed stream.

    if (LLVM_UNLIKELY(DebugOptionIsSet(codegen::EnableBlockingIOCounter) || DebugOptionIsSet(codegen::TraceBlockedIO))) {
        mBranchToLoopExit = b->getFalse();
    }
    for (const auto e : make_iterator_range(in_edges(mKernelIndex, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];
        checkForSufficientInputData(b, br.Port);
    }

    mIsBounded = false;
    for (const auto e : make_iterator_range(in_edges(mKernelIndex, mBufferGraph))) {
        const BufferRateData & br = mBufferGraph[e];
        Value * const strides = getNumOfAccessibleStrides(b, br.Port);
        if (strides) {
            mIsBounded = true;
        }
        mNumOfLinearStrides = b->CreateUMin(mNumOfLinearStrides, strides);
    }

    if (mMayHaveNonLinearIO) {
        Value * numOfOutputStrides = mNumOfLinearStrides;
        for (const auto e : make_iterator_range(out_edges(mKernelIndex, mBufferGraph))) {
            const BufferRateData & br = mBufferGraph[e];
            Value * const strides = getNumOfWritableStrides(b, br.Port);
            numOfOutputStrides = b->CreateUMin(numOfOutputStrides, strides);
        }
        Value * const anyInput = b->CreateIsNotNull(mNumOfLinearStrides);
        Value * const noOutput = b->CreateIsNull(numOfOutputStrides);
        Value * const fullyExpand = b->CreateAnd(anyInput, noOutput);
        mNumOfLinearStrides = b->CreateSelect(fullyExpand, mNumOfLinearStrides, numOfOutputStrides);
    }

    calculateItemCounts(b);

    for (const auto e : make_iterator_range(out_edges(mKernelIndex, mBufferGraph))) {
        const BufferRateData & output = mBufferGraph[e];
        ensureSufficientOutputSpace(b, output.Port);
    }

    // When tracing blocking I/O, test all I/O streams but do not execute the
    // kernel if any stream is insufficient.
    if (mBranchToLoopExit) {
        BasicBlock * const noStreamIsInsufficient = b->CreateBasicBlock("", mKernelLoopCall);
        b->CreateUnlikelyCondBr(mBranchToLoopExit, mKernelInsufficientIOExit, noStreamIsInsufficient);

        BasicBlock * const exitBlock = b->GetInsertBlock();
        mInsufficientIOHaltingPhi->addIncoming(mHalted, exitBlock);

        updatePHINodesForLoopExit(b, mHalted);
        b->SetInsertPoint(noStreamIsInsufficient);
    }

    b->CreateBr(mKernelLoopCall);

}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief calculateItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::calculateItemCounts(BuilderRef b) {

    // --- lambda function start
    auto phiOutItemCounts = [&](const Vec<Value *> & accessibleItems,
                               const Vec<Value *> & inputEpoch,
                               const Vec<Value *> & writableItems,
                               Value * const fixedRateFactor,
                               Constant * const terminationSignal) {
        BasicBlock * const exitBlock = b->GetInsertBlock();
        const auto numOfInputs = accessibleItems.size();
        for (unsigned i = 0; i < numOfInputs; ++i) {
			const auto port = StreamSetPort{ PortType::Input, i };
            mLinearInputItemsPhi(port)->addIncoming(accessibleItems[i], exitBlock);
            mInputEpochPhi(port)->addIncoming(inputEpoch[i], exitBlock);
        }
        const auto numOfOutputs = writableItems.size();
        for (unsigned i = 0; i < numOfOutputs; ++i) {
			const auto port = StreamSetPort{ PortType::Output, i };
            mLinearOutputItemsPhi(port)->addIncoming(writableItems[i], exitBlock);
        }
        if (fixedRateFactor) { assert (mFixedRateFactorPhi);
            mFixedRateFactorPhi->addIncoming(fixedRateFactor, exitBlock);
        }
        mIsFinalInvocationPhi->addIncoming(terminationSignal, exitBlock);
    };
    // --- lambda function end

    const auto numOfInputs = in_degree(mKernelIndex, mBufferGraph);
    Vec<Value *> accessibleItems(numOfInputs);

    const auto numOfOutputs = out_degree(mKernelIndex, mBufferGraph);
    Vec<Value *> writableItems(numOfOutputs);

    calculateInputEpochAddresses(b);

    Constant * const unterminated = getTerminationSignal(b, TerminationSignal::None);

    if (mIsBounded) {

        const auto prefix = makeKernelName(mKernelIndex);
        BasicBlock * const enteringNonFinalSegment = b->CreateBasicBlock(prefix + "_nonFinalSegment", mKernelLoopCall);
        BasicBlock * const enteringFinalStride = b->CreateBasicBlock(prefix + "_finalStride", mKernelLoopCall);
        Value * isFinal = nullptr;
        if (mKernelHasAnExplicitFinalPartialStride) {
            isFinal = b->CreateIsNull(mNumOfLinearStrides);
        } else {
            // Check whether we have any stream that's closed and would be exhausted if we processed one
            // more stride than we're currently processing. If so, we could include that stride and mark
            // this as our final stride.
            ConstantInt * const ONE = b->getSize(1);
            for (const auto e : make_iterator_range(in_edges(mKernelIndex, mBufferGraph))) {
                const BufferRateData & br =  mBufferGraph[e];
                if (LLVM_UNLIKELY(br.ZeroExtended)) {
                    continue;
                }
                Value * const closed = isClosed(b, br.Port);
                Value * const requiredForNextStride = calculateNumOfLinearItems(b, br.Port, ONE);
                Value * const total = mLocallyAvailableItems[getBufferIndex(source(e, mBufferGraph))];

                Value * const unprocessed = b->CreateSub(total, mAlreadyProcessedPhi(br.Port));
                Value * const consumed = b->CreateICmpULE(unprocessed, requiredForNextStride);
                Value * const fullyConsumed = b->CreateAnd(closed, consumed);

                if (isFinal) {
                    isFinal = b->CreateOr(isFinal, fullyConsumed);
                } else {
                    isFinal = fullyConsumed;
                }
            }
            assert (isFinal && "non-zero-extended stream is required");
        }
        b->CreateUnlikelyCondBr(isFinal, enteringFinalStride, enteringNonFinalSegment);

        /// -------------------------------------------------------------------------------------
        /// KERNEL ENTERING FINAL STRIDE
        /// -------------------------------------------------------------------------------------

        b->SetInsertPoint(enteringFinalStride);
        Value * const finalFactor = calculateFinalItemCounts(b, accessibleItems, writableItems);
        Vec<Value *> inputEpochPhi(numOfInputs);
        zeroInputAfterFinalItemCount(b, accessibleItems, inputEpochPhi);
        Constant * const completed = getTerminationSignal(b, TerminationSignal::Completed);
        phiOutItemCounts(accessibleItems, inputEpochPhi, writableItems, finalFactor, completed);
        b->CreateBr(mKernelCheckOutputSpace);

        /// -------------------------------------------------------------------------------------
        /// KERNEL ENTERING NON-FINAL SEGMENT
        /// -------------------------------------------------------------------------------------

        b->SetInsertPoint(enteringNonFinalSegment);
    }

    Value * const nonFinalFactor = calculateNonFinalItemCounts(b, accessibleItems, writableItems);
    Vec<Value *> inputEpoch(numOfInputs);
    for (unsigned  i = 0; i < numOfInputs; ++i) {
        inputEpoch[i] = mInputEpoch(StreamSetPort{PortType::Input, i});
    }
    phiOutItemCounts(accessibleItems, inputEpoch, writableItems, nonFinalFactor, unterminated);
    b->CreateBr(mKernelCheckOutputSpace);

    b->SetInsertPoint(mKernelCheckOutputSpace);
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief checkForSufficientInputData
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::checkForSufficientInputData(BuilderRef b, const StreamSetPort inputPort) {
    // TODO: we could eliminate some checks if we can prove a particular input
    // must have enough data based on its already tested inputs and ignore
    // checking whether an input kernel is terminated if a stronger test has
    // already been done. Work out the logic for these tests globally.
    Value * const accessible = getAccessibleInputItems(b, inputPort); assert (accessible);
    Value * const strideLength = getInputStrideLength(b, inputPort);
    Value * const required = addLookahead(b, inputPort, strideLength); assert (required);
    const auto prefix = makeBufferName(mKernelIndex, inputPort);
    #ifdef PRINT_DEBUG_MESSAGES
    debugPrint(b, prefix + "_required = %" PRIu64, required);
    #endif
    const Binding & input = getInputBinding(inputPort);
    const ProcessingRate & rate = input.getRate();
    Value * sufficientInput = nullptr;
    if (LLVM_UNLIKELY(rate.isGreedy())) {
        sufficientInput = b->CreateICmpUGE(accessible, required);
    } else {
        Value * const hasEnough = b->CreateICmpUGE(accessible, required);
        Value * const closed = isClosed(b, inputPort);
        #ifdef PRINT_DEBUG_MESSAGES
        debugPrint(b, prefix + "_closed = %" PRIu8, closed);
        #endif
        sufficientInput = b->CreateOr(hasEnough, closed);
    }
    Value * const halting = isPipelineInput(inputPort) ? b->getTrue() : mHalted;
    BasicBlock * const target = b->CreateBasicBlock(prefix + "_hasInputData", mKernelCheckOutputSpace);
    branchToTargetOrLoopExit(b, inputPort, sufficientInput, target, halting);
    mAccessibleInputItems(mKernelIndex, inputPort) = accessible;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getAccessibleInputItems
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getAccessibleInputItems(BuilderRef b, const StreamSetPort inputPort, const bool useOverflow) {
    const StreamSetBuffer * const buffer = getInputBuffer(inputPort);
    Value * const available = getLocallyAvailableItemCount(b, inputPort);
    Value * const processed = mAlreadyProcessedPhi(mKernelIndex, inputPort);
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
    const BufferRateData & rateData = mBufferGraph[getInput(mKernelIndex, inputPort)];


    const Binding & input = rateData.Binding;
    Value * accessible = buffer->getLinearlyAccessibleItems(b, processed, available, lookAhead);
    #ifndef DISABLE_ZERO_EXTEND

    // TODO: if a stream is zero extended but it is produced within the same partition as another
    // input to this kernel with an equivalent production *and* consumption rate and the stream is
    // not also zero extended, ignore the zero extension attribute for this stream.

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
        mIsInputZeroExtended(mKernelIndex, inputPort) = useZeroExtend;
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
        if (mIsInputZeroExtended(mKernelIndex, inputPort)) {
            sanityCheck = b->CreateOr(mIsInputZeroExtended(mKernelIndex, inputPort), sanityCheck);
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

    #ifdef PRINT_DEBUG_MESSAGES
    const auto prefix = makeBufferName(mKernelIndex, outputPort);
    debugPrint(b, prefix + "_produced = %" PRIu64, produced);
    debugPrint(b, prefix + "_consumed = %" PRIu64, consumed);
    debugPrint(b, prefix + "_required = %" PRIu64, required);
    debugPrint(b, prefix + "_remaining = %" PRIu64, remaining);
    #endif

    BasicBlock * const expandBuffer = b->CreateBasicBlock("expandBuffer", mKernelLoopCall);
    BasicBlock * const expanded = b->CreateBasicBlock("expanded", mKernelLoopCall);

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

//    Value * const remaining2 = buffer->getLinearlyWritableItems(b, produced, consumed, copyBack);

    recordBufferExpansionHistory(b, outputPort, buffer);
    if (cycleCounterAccumulator) {
        Value * const cycleCounterEnd = b->CreateReadCycleCounter();
        Value * const duration = b->CreateSub(cycleCounterEnd, cycleCounterStart);
        Value * const accum = b->CreateAdd(b->CreateLoad(cycleCounterAccumulator), duration);
        b->CreateStore(accum, cycleCounterAccumulator);
    }

//    const Binding & binding = getOutputBinding(outputPort);
//    Value * const hasEnoughSpace2 = b->CreateICmpULE(required, remaining2);
//    b->CreateAssert(hasEnoughSpace2,
//                    "%s: failed to expand the buffer correctly. "
//                    "Requires %" PRIu64 " items but only has %" PRIu64,
//                    b->GetString(binding.getName()),
//                    required, remaining2);

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
    const auto prefix = makeBufferName(mKernelIndex, outputPort);
    debugPrint(b, prefix + "_produced = %" PRIu64, produced);
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
    Value * const ze = mIsInputZeroExtended(mKernelIndex, inputPort);
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
        mWritableOutputItems(mKernelIndex, outputPort) = writable;
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
        accessibleItems[i] = calculateNumOfLinearItems(b, StreamSetPort{PortType::Input, i}, nullptr);
    }
    const auto numOfOutputs = writableItems.size();
    for (unsigned i = 0; i < numOfOutputs; ++i) {
        writableItems[i] = calculateNumOfLinearItems(b, StreamSetPort{PortType::Output, i}, nullptr);
    }
    return fixedRateFactor;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief calculateFinalItemCounts
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::calculateFinalItemCounts(BuilderRef b, Vec<Value *> & accessibleItems, Vec<Value *> & writableItems) {
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
        if (LLVM_UNLIKELY(mIsInputZeroExtended(mKernelIndex, inputPort) != nullptr)) {
            // If this input stream is zero extended, the current input items will be MAX_INT.
            // However, since we're now in the final stride, so we can bound the stream to:
            const Binding & input = getInputBinding(inputPort);
            const ProcessingRate & rate = input.getRate();
            if (principalFixedRateFactor && rate.isFixed()) {
                const auto factor = rate.getRate() / mFixedRateLCM;
                accessible = b->CreateCeilUMulRate(principalFixedRateFactor, factor);
            } else {
                Value * maxItems = b->CreateAdd(mAlreadyProcessedPhi(mKernelIndex, inputPort), mFirstInputStrideLength(mKernelIndex, inputPort));
                // But since we may not necessarily be in our zero extension region, we must first
                // test whether we are:
                accessible = b->CreateSelect(mIsInputZeroExtended(mKernelIndex, inputPort), maxItems, accessible);
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
                auto addPort = in_edges(mKernelIndex, mAddGraph).first + i;
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
                if (LLVM_UNLIKELY(mCheckAssertions)) {
                    Value * const accessible = accessibleItems[i];
                    Value * correctItemCount = b->CreateICmpULE(calculated, accessible);
                    Value * const zeroExtended = mIsInputZeroExtended(mKernelIndex, inputPort);
                    if (LLVM_UNLIKELY(zeroExtended != nullptr)) {
                        correctItemCount = b->CreateOr(correctItemCount, zeroExtended);
                    }
                    b->CreateAssert(correctItemCount,
                                    "%s.%s: final calculated rate item count (%" PRIu64 ") "
                                    "exceeds accessible item count (%" PRIu64 ")",
                                    mKernelAssertionName,
                                    b->GetString(input.getName()),
                                    calculated, accessible);
                }
                accessibleItems[i] = calculated;
            }
            #ifdef PRINT_DEBUG_MESSAGES
            const auto prefix = makeBufferName(mKernelIndex, inputPort);
            debugPrint(b, prefix + ".accessible' = %" PRIu64, accessibleItems[i]);
            #endif
        }
    }

    const auto numOfOutputs = writableItems.size();
    for (unsigned i = 0; i < numOfOutputs; ++i) {
		const StreamSetPort port{ PortType::Output, i };
        const Binding & output = getOutputBinding(port);
        const ProcessingRate & rate = output.getRate();

        Value * writable = getOutputStrideLength(b, port);
        if (rate.isFixed() && minFixedRateFactor) {

            const auto factor = rate.getRate() / mFixedRateLCM;
            Value * calculated = b->CreateCeilUMulRate(minFixedRateFactor, factor);

            if (LLVM_UNLIKELY(mCheckAssertions && writable)) {
                b->CreateAssert(b->CreateICmpULE(calculated, writable),
                                "%s.%s: final calculated fixed rate item count (%" PRIu64 ") "
                                "exceeds writable item count (%" PRIu64 ")",
                                mKernelAssertionName,
                                b->GetString(output.getName()),
                                calculated, writable);
            }
            writable = calculated;
        }
        assert (writable);

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
    return minFixedRateFactor;
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getInputStrideLength
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getInputStrideLength(BuilderRef b, const StreamSetPort inputPort) {
    if (mFirstInputStrideLength(inputPort)) {
        return mFirstInputStrideLength(inputPort);
    } else {
        Value * const strideLength = getFirstStrideLength(b, inputPort);
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
        Value * const strideLength = getFirstStrideLength(b, outputPort);
        mFirstOutputStrideLength(outputPort) = strideLength;
        return strideLength;
    }
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief getPartialSumItemCount
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::getPartialSumItemCount(BuilderRef b, const StreamSetPort port, Value * const offset) const {
    const auto ref = getReference(port);
    assert (ref.Type == PortType::Input);

    const StreamSetBuffer * const buffer = getInputBuffer(ref);
    Value * prior = nullptr;
    if (port.Type == PortType::Input) {
        prior = mAlreadyProcessedPhi(mKernelIndex, port);
    } else { // if (port.Type == PortType::Output) {
        prior = mAlreadyProducedPhi(mKernelIndex, port);
    }
    assert (prior);

    Constant * const ZERO = b->getSize(0);
    Value * position = mAlreadyProcessedPhi(mKernelIndex, ref);

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
        initialItemCount = mAlreadyProcessedPhi(mKernelIndex, port);
        Value * const accessible = mAccessibleInputItems(mKernelIndex, port);
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
        initialItemCount = mAlreadyProducedPhi(mKernelIndex, port);
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
    const auto refInput = getInput(mKernelIndex, ref);
    const BufferRateData & refInputRate = mBufferGraph[refInput];
    const auto refBufferVertex = getInputBufferVertex(ref);
    const auto refOuput = in_edge(refBufferVertex, mBufferGraph);
    const BufferRateData & refOutputRate = mBufferGraph[refOuput];
    const auto stepFactor = refInputRate.Maximum / refOutputRate.Maximum;

    assert (stepFactor.denominator() == 1);
    const auto step = stepFactor.numerator();
    Constant * const STEP = b->getSize(step);

    const StreamSetBuffer * const buffer = mBufferGraph[refBufferVertex].Buffer;
    const auto prefix = makeBufferName(mKernelIndex, ref) + "_readPartialSum";

    BasicBlock * const popCountLoop =
        b->CreateBasicBlock(prefix + "Loop", mKernelCheckOutputSpace);
    BasicBlock * const popCountLoopExit =
        b->CreateBasicBlock(prefix + "LoopExit", mKernelCheckOutputSpace);
    Value * const baseOffset = mAlreadyProcessedPhi(mKernelIndex, ref);
    Value * const baseAddress = buffer->getRawItemPointer(b, ZERO, baseOffset);
    BasicBlock * const popCountEntry = b->GetInsertBlock();
    Value * const initialStrideCount = b->CreateMul(mNumOfLinearStrides, STEP);
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
Value * PipelineCompiler::getFirstStrideLength(BuilderRef b, const StreamSetPort port) {
    const Binding & binding = getBinding(port);
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
        return getPartialSumItemCount(b, port);
    } else if (rate.isGreedy()) {
        assert ("kernel cannot have a greedy output rate" && port.Type != PortType::Output);
        const Rational lb = rate.getLowerBound(); // * mKernel->getStride();
        const auto ilb = floor(lb);
        Value * firstBound = b->getSize(ilb);
        if (LLVM_UNLIKELY(ilb > 0)) {
            Constant * const ZERO = b->getSize(0);
            firstBound = b->CreateSelect(isClosed(b, port), ZERO, firstBound);
        }
        if (mMayHaveNonLinearIO) {
            Constant * const subsequentBound = b->getSize(ceiling(lb) + 1);
            return b->CreateSelect(mExecutedAtLeastOncePhi, subsequentBound, firstBound);
        } else {
            return firstBound;
        }
    } else if (rate.isRelative()) {
        Value * const baseRate = getFirstStrideLength(b, getReference(port));
        return b->CreateMulRate(baseRate, rate.getRate());
    }
    llvm_unreachable("unexpected rate type");
}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief calculateNumOfLinearItems
 ** ------------------------------------------------------------------------------------------------------------- */
Value * PipelineCompiler::calculateNumOfLinearItems(BuilderRef b, const StreamSetPort port, Value * const adjustment) {
    const Binding & binding = getBinding(port);
    const ProcessingRate & rate = binding.getRate();
    if (rate.isFixed() || rate.isBounded()) {
        Value * linearStrides = mNumOfLinearStrides;
        if (adjustment) {
            linearStrides = b->CreateAdd(linearStrides, adjustment);
        }
        return b->CreateMulRate(linearStrides, rate.getUpperBound() * mKernel->getStride());
    } else if (rate.isGreedy()) {
        return mAccessibleInputItems(mKernelIndex, port);
    } else if (rate.isPartialSum()) {
        Value * linearStrides = mNumOfLinearStrides;
        if (adjustment) {
            linearStrides = b->CreateAdd(linearStrides, adjustment);
        }
        return getPartialSumItemCount(b, port, linearStrides);
    } else if (rate.isRelative()) {
        Value * const baseCount = calculateNumOfLinearItems(b, getReference(port), adjustment);
        return b->CreateMulRate(baseCount, rate.getRate());
    }
    llvm_unreachable("unexpected rate type");
}


/** ------------------------------------------------------------------------------------------------------------- *
 * @brief branchToTargetOrLoopExit
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::branchToTargetOrLoopExit(BuilderRef b, const StreamSetPort port,
                                                Value * const cond, BasicBlock * const target,
                                                Value * const halting) {

    BasicBlock * recordBlockedIO = nullptr;
    BasicBlock * insufficentIO = mKernelInsufficientIOExit;

    if (mBranchToLoopExit) {
        const auto prefix = makeBufferName(mKernelIndex, port);
        recordBlockedIO = b->CreateBasicBlock(prefix + "_recordBlockedIO", mKernelInsufficientIOExit);
        insufficentIO = recordBlockedIO;
    }

    BasicBlock * const entryBlock = b->GetInsertBlock();


    Value * test = cond;
    Value * insufficient = mBranchToLoopExit;
    if (mBranchToLoopExit) {
        // do not record the block if this not the first execution of the
        // kernel but ensure that the system knows at least one failed.
        test = b->CreateOr(cond, mExecutedAtLeastOncePhi);
        insufficient = b->CreateOr(mBranchToLoopExit, b->CreateNot(cond));
    }

    b->CreateLikelyCondBr(test, target, insufficentIO);

    // When tracing blocking I/O, test all I/O streams but do not execute
    // the kernel if any stream is insufficient.
    if (mBranchToLoopExit) {
        b->SetInsertPoint(recordBlockedIO);
        recordBlockingIO(b, port);
        BasicBlock * const exitBlock = b->GetInsertBlock();
        b->CreateBr(target);

        b->SetInsertPoint(target);
        IntegerType * const boolTy = b->getInt1Ty();

        PHINode * const anyInsufficient = b->CreatePHI(boolTy, 2);
        anyInsufficient->addIncoming(insufficient, entryBlock);
        anyInsufficient->addIncoming(b->getTrue(), exitBlock);
        mBranchToLoopExit = anyInsufficient;
        PHINode * const halted = b->CreatePHI(boolTy, 2);
        halted->addIncoming(mHalted, entryBlock);
        halted->addIncoming(halting, exitBlock);
        mHalted = halted;

    } else {

        BasicBlock * const exitBlock = b->GetInsertBlock();
        mInsufficientIOHaltingPhi->addIncoming(halting, exitBlock);
        b->SetInsertPoint(target);
    }

}

/** ------------------------------------------------------------------------------------------------------------- *
 * @brief updatePHINodesForLoopExit
 ** ------------------------------------------------------------------------------------------------------------- */
void PipelineCompiler::updatePHINodesForLoopExit(BuilderRef b, Value * halting) {

    BasicBlock * const exitBlock = b->GetInsertBlock();
    mHaltingPhi->addIncoming(halting, exitBlock);
    const auto numOfInputs = getNumOfStreamInputs(mKernelIndex);
    for (unsigned i = 0; i < numOfInputs; ++i) {
		const StreamSetPort port(PortType::Input, i);
        mUpdatedProcessedPhi(port)->addIncoming(mAlreadyProcessedPhi(port), exitBlock);
        if (mUpdatedProcessedDeferredPhi(port)) {
            mUpdatedProcessedDeferredPhi(port)->addIncoming(mAlreadyProcessedDeferredPhi(port), exitBlock);
        }
    }
    const auto numOfOutputs = getNumOfStreamOutputs(mKernelIndex);
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
