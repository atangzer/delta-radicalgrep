/*
 *  Copyright (c) 2019 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */

#include "scanmatchgen.h"
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Module.h>
#include <kernels/kernel_builder.h>
#include <llvm/Support/raw_ostream.h>
#include <grep/grep_engine.h>

using namespace llvm;

namespace kernel {

void ScanMatchKernel::generateMultiBlockLogic(const std::unique_ptr<KernelBuilder> & b, Value * const numOfStrides) {

    Module * const m = b->getModule();

    BasicBlock * const entryBlock = b->GetInsertBlock();
    BasicBlock * const scanMatchStride = b->CreateBasicBlock("scanMatchStride");
    BasicBlock * const scanWordIteration = b->CreateBasicBlock("ScanWordIteration");
    BasicBlock * const matches_test_block = b->CreateBasicBlock("matches_test_block");
    BasicBlock * const processMatchesEntry = b->CreateBasicBlock("process_matches_loop");
    BasicBlock * const prior_breaks_block = b->CreateBasicBlock("prior_breaks_block");
    BasicBlock * const loop_final_block = b->CreateBasicBlock("loop_final_block");
    BasicBlock * const processMatchesExit = b->CreateBasicBlock("matches_done_block");
    BasicBlock * const remaining_breaks_block = b->CreateBasicBlock("remaining_breaks_block");
    BasicBlock * const return_block = b->CreateBasicBlock("return_block");
    BasicBlock * const finalizeStride = b->CreateBasicBlock("finalizeStride");
    BasicBlock * const stridesDone = b->CreateBasicBlock("stridesDone");
    BasicBlock * const callFinalizeScan = b->CreateBasicBlock("callFinalizeScan");
    BasicBlock * const scanReturn = b->CreateBasicBlock("scanReturn");
    IntegerType * const sizeTy = b->getSizeTy();
    const unsigned scansPerStride = mStride / sizeTy->getBitWidth();
    PointerType * const scanwordPointerType =  sizeTy->getPointerTo();
    Constant * const ZERO = b->getSize(0);
    Constant * const ONE = b->getSize(1);
    Constant * const BITBLOCK_WIDTH = b->getSize(b->getBitBlockWidth());
    Value * const initialPos = b->getProcessedItemCount("matchResult");
    Value * accumulator = b->getScalarField("accumulator_address");

    b->CreateBr(scanMatchStride);

    b->SetInsertPoint(scanMatchStride);
    PHINode * const positionOffset = b->CreatePHI(sizeTy, 2);
    positionOffset->addIncoming(ZERO, entryBlock);
    Value * blockOffset = b->CreateUDiv(positionOffset, BITBLOCK_WIDTH);
    Value * matches = b->getInputStreamBlockPtr("matchResult", ZERO, blockOffset);
    matches = b->CreateBitCast(matches, scanwordPointerType);
    Value * linebreaks = b->getInputStreamBlockPtr("lineBreak", ZERO, blockOffset);
    linebreaks = b->CreateBitCast(linebreaks, scanwordPointerType);
    Value * const scanwordPos = b->CreateAdd(initialPos, positionOffset);
    Value * const consumed = b->getProcessedItemCount("InputStream");
    Value * const consumedLines = b->getScalarField("LineNum");
    Value * const avail = b->getAvailableItemCount("InputStream");
    b->CreateBr(scanWordIteration);

    b->SetInsertPoint(scanWordIteration);

        // while (phiIndex < words per stride)
        PHINode * const phiIndex = b->CreatePHI(sizeTy, 2, "index");
        phiIndex->addIncoming(ZERO, scanMatchStride);
        PHINode * const phiScanwordPos = b->CreatePHI(scanwordPos->getType(), 2, "pos");
        phiScanwordPos->addIncoming(scanwordPos, scanMatchStride);
        PHINode * const phiLineStart = b->CreatePHI(consumed->getType(), 2, "recordstart");
        phiLineStart->addIncoming(consumed, scanMatchStride);
        PHINode * const phiLineNum = b->CreatePHI(consumedLines->getType(), 2, "recordnum");
        phiLineNum->addIncoming(consumedLines, scanMatchStride);

        Value * const matchWord = b->CreateLoad(b->CreateGEP(matches, phiIndex));
        Value * const recordBreaks = b->CreateLoad(b->CreateGEP(linebreaks, phiIndex));
        // The match scanner works with a loop involving four variables:
        // (a) the bit stream scanword of matches marking the ends of selected records,
        // (b) the bit stream scanword of record_breaks marking the ends of all records,
        // (c) the integer lastRecordNum indicating the number of records processed so far,
        // (d) the index lastRecordStart indicating the file position of the last record.
        // We set up a loop structure, in which a set of 4 phi nodes initialize these
        // variables from either the input to the scanner or the computed values within
        // the loop body.

        b->CreateBr(matches_test_block);

        // LOOP Test Block
        b->SetInsertPoint(matches_test_block);
        PHINode * const phiMatchWord = b->CreatePHI(sizeTy, 2, "matches");
        PHINode * const phiRecordBreaks = b->CreatePHI(sizeTy, 2, "recordbreaks");
        PHINode * const phiRecordStart = b->CreatePHI(sizeTy, 2, "recordstart");
        PHINode * const phiRecordNum = b->CreatePHI(sizeTy, 2, "recordnum");
        phiMatchWord->addIncoming(matchWord, scanWordIteration);
        phiRecordBreaks->addIncoming(recordBreaks, scanWordIteration);
        phiRecordStart->addIncoming(phiLineStart, scanWordIteration);
        phiRecordNum->addIncoming(phiLineNum, scanWordIteration);
        Value * const anyMatches = b->CreateICmpNE(phiMatchWord, ZERO);
        b->CreateCondBr(anyMatches, processMatchesEntry, processMatchesExit);

            // LOOP BODY
            // The loop body is entered if we have more matches to process.
            b->SetInsertPoint(processMatchesEntry);
            Value * prior_breaks = b->CreateAnd(b->CreateMaskToLowestBitExclusive(phiMatchWord), phiRecordBreaks);
            // Within the loop we have a conditional block that is executed if there are any prior record breaks.
            Value * prior_breaks_cond = b->CreateICmpNE(prior_breaks, ZERO);
            b->CreateCondBr(prior_breaks_cond, prior_breaks_block, loop_final_block);

                // PRIOR_BREAKS_BLOCK
                // If there are prior breaks, we count them and compute the record start position.
                b->SetInsertPoint(prior_breaks_block);
                Value * matchedRecordNum = b->CreateAdd(b->CreatePopcount(prior_breaks), phiRecordNum);
                Value * reverseDistance = b->CreateCountReverseZeroes(prior_breaks, true);
                Value * width = ConstantInt::get(sizeTy, sizeTy->getBitWidth());
                Value * priorRecordStart = b->CreateAdd(phiScanwordPos, b->CreateSub(width, reverseDistance));
                b->CreateBr(loop_final_block);

            // LOOP FINAL BLOCK
            // The prior breaks, if any have been counted.  Set up phi nodes for the recordNum
            // and recortStart depending on whether the conditional execution of prior_breaks_block.
            b->SetInsertPoint(loop_final_block);
            PHINode * matchRecordNum = b->CreatePHI(sizeTy, 2, "matchRecordNum");
            matchRecordNum->addIncoming(phiRecordNum, processMatchesEntry);
            matchRecordNum->addIncoming(matchedRecordNum, prior_breaks_block);
            phiRecordNum->addIncoming(matchRecordNum, loop_final_block);

            PHINode * const matchRecordStart = b->CreatePHI(sizeTy, 2, "matchRecordStart");
            matchRecordStart->addIncoming(phiRecordStart, processMatchesEntry);
            matchRecordStart->addIncoming(priorRecordStart, prior_breaks_block);
            phiRecordStart->addIncoming(matchRecordStart, loop_final_block);
            Value * matchRecordEnd = b->CreateAdd(phiScanwordPos, b->CreateCountForwardZeroes(phiMatchWord, true));
            // It is possible that the matchRecordEnd position is one past EOF.  Make sure not
            // to access past EOF.
            Value * const bufLimit = b->CreateSub(b->CreateAdd(scanwordPos, avail), ONE);
            matchRecordEnd = b->CreateUMin(matchRecordEnd, bufLimit);
            Function * const dispatcher = m->getFunction("accumulate_match_wrapper"); assert (dispatcher);
            Value * const startPtr = b->getRawInputPointer("InputStream", matchRecordStart);
            Value * const endPtr = b->getRawInputPointer("InputStream", matchRecordEnd);
            auto argi = dispatcher->arg_begin();
            const auto matchRecNumArg = &*(argi++);
            Value * const matchRecNum = b->CreateZExtOrTrunc(matchRecordNum, matchRecNumArg->getType());
            b->CreateCall(dispatcher, {accumulator, matchRecNum, startPtr, endPtr});
            Value * remaining_matches = b->CreateResetLowestBit(phiMatchWord);
            phiMatchWord->addIncoming(remaining_matches, loop_final_block);

            Value * remaining_breaks = b->CreateXor(phiRecordBreaks, prior_breaks);
            phiRecordBreaks->addIncoming(remaining_breaks, loop_final_block);

            b->CreateBr(matches_test_block);

        // LOOP EXIT/MATCHES_DONE
        b->SetInsertPoint(processMatchesExit);
        // When the matches are done, there may be additional record breaks remaining
        Value * more_breaks_cond = b->CreateICmpNE(phiRecordBreaks, ZERO);
        b->CreateCondBr(more_breaks_cond, remaining_breaks_block, return_block);

            // REMAINING_BREAKS_BLOCK: process remaining record breaks after all matches are processed
            b->SetInsertPoint(remaining_breaks_block);
            Value * break_count = b->CreatePopcount(phiRecordBreaks);
            Value * final_record_num = b->CreateAdd(phiRecordNum, break_count);
            Value * reverseZeroes = b->CreateCountReverseZeroes(phiRecordBreaks);
            Value * pendingLineStart = b->CreateAdd(phiScanwordPos, b->CreateSub(width, reverseZeroes));
            b->CreateBr(return_block);

        // RETURN block
        b->SetInsertPoint(return_block);
        PHINode * phiFinalRecordNum = b->CreatePHI(sizeTy, 2, "finalRecordCount");
        PHINode * phiFinalRecordStart = b->CreatePHI(sizeTy, 2, "finalRecordStart");

        phiFinalRecordNum->addIncoming(phiRecordNum, processMatchesExit);
        phiFinalRecordNum->addIncoming(final_record_num, remaining_breaks_block);
        phiLineNum->addIncoming(phiFinalRecordNum, return_block);

        phiFinalRecordStart->addIncoming(phiRecordStart, processMatchesExit);
        phiFinalRecordStart->addIncoming(pendingLineStart, remaining_breaks_block);
        phiLineStart->addIncoming(phiFinalRecordStart, return_block);

        Value * nextScanwordPos = b->CreateAdd(phiScanwordPos, ConstantInt::get(sizeTy, sizeTy->getBitWidth()));
        phiScanwordPos->addIncoming(nextScanwordPos, return_block);
        Value * nextIndex = b->CreateAdd(phiIndex, ONE);
        phiIndex->addIncoming(nextIndex, return_block);
        b->CreateLikelyCondBr(b->CreateICmpNE(nextIndex, b->getSize(scansPerStride)), scanWordIteration, finalizeStride);

    b->SetInsertPoint(finalizeStride);
    b->setScalarField("LineNum", phiFinalRecordNum);
    b->setProcessedItemCount("InputStream", phiFinalRecordStart);
    Value * const nextPositionOffset = b->CreateAdd(positionOffset, b->getSize(mStride));
    Value * const nextStride = b->CreateUDiv(nextPositionOffset, b->getSize(mStride));
    positionOffset->addIncoming(nextPositionOffset, finalizeStride);
    b->CreateLikelyCondBr(b->CreateICmpNE(nextStride, numOfStrides), scanMatchStride, stridesDone);

    b->SetInsertPoint(stridesDone);
    b->CreateCondBr(mIsFinal, callFinalizeScan, scanReturn);

    b->SetInsertPoint(callFinalizeScan);
    Function * finalizer = m->getFunction("finalize_match_wrapper"); assert (finalizer);
    Value * const bufferEnd = b->getRawInputPointer("InputStream", avail);
    b->CreateCall(finalizer, {accumulator, bufferEnd});
    b->CreateBr(scanReturn);

    b->SetInsertPoint(scanReturn);
}

ScanMatchKernel::ScanMatchKernel(const std::unique_ptr<kernel::KernelBuilder> & b, StreamSet * const Matches, StreamSet * const LineBreakStream, StreamSet * const ByteStream, Scalar * const callbackObject)
: MultiBlockKernel(b, "scanMatch",
// inputs
{Binding{"matchResult", Matches, FixedRate(), Principal()}
,Binding{"lineBreak", LineBreakStream}
,Binding{"InputStream", ByteStream, FixedRate(), Deferred()}},
// outputs
{},
// input scalars
{Binding{"accumulator_address", callbackObject}},
// output scalars
{},
// kernel state
{InternalScalar{b->getSizeTy(), "LineNum"}}) {
    addAttribute(SideEffecting());
    setStride(b->getBitBlockWidth() * 2);
}

}
