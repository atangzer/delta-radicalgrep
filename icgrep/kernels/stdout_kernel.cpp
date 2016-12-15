/*
 *  Copyright (c) 2016 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */
#include <kernels/stdout_kernel.h>
#include <kernels/kernel.h>
#include <IDISA/idisa_builder.h>

namespace kernel {

static Function * create_write(Module * const mod, IDISA::IDISA_Builder * builder) {
    Function * write = mod->getFunction("write");
    if (write == nullptr) {
        IntegerType * sizeTy = builder->getSizeTy();
        IntegerType * int32Ty = builder->getInt32Ty();
        PointerType * int8PtrTy = builder->getInt8PtrTy();
        write = cast<Function>(mod->getOrInsertFunction("write",
                                 AttributeSet().addAttribute(mod->getContext(), 2U, Attribute::NoAlias),
                                 sizeTy, int32Ty, int8PtrTy, sizeTy, nullptr));
    }
    return write;
}

// The doBlock method is deprecated.   But in case it is used, just call doSegment with
// 1 as the number of blocks to do.
void stdOutKernel::generateDoBlockMethod() {
    auto savePoint = iBuilder->saveIP();
    Module * m = iBuilder->getModule();
    Function * doBlockFunction = m->getFunction(mKernelName + doBlock_suffix);
    Function * doSegmentFunction = m->getFunction(mKernelName + doSegment_suffix);
    iBuilder->SetInsertPoint(BasicBlock::Create(iBuilder->getContext(), "entry", doBlockFunction, 0));
    Value * self = getParameter(doBlockFunction, "self");
    iBuilder->CreateCall(doSegmentFunction, {self, ConstantInt::get(iBuilder->getSizeTy(), 1)});
    iBuilder->CreateRetVoid();
    iBuilder->restoreIP(savePoint);
}
            
// Rather than using doBlock logic to write one block at a time, this custom
// doSegment method, writes the entire segment with a single write call.
void stdOutKernel::generateDoSegmentMethod() {
    auto savePoint = iBuilder->saveIP();
    Module * m = iBuilder->getModule();
    Function * writefn = create_write(m, iBuilder);
    Function * doSegmentFunction = m->getFunction(mKernelName + doSegment_suffix);
    Type * i8PtrTy = iBuilder->getInt8PtrTy();
    
    iBuilder->SetInsertPoint(BasicBlock::Create(iBuilder->getContext(), "entry", doSegmentFunction, 0));
    BasicBlock * setTermination = BasicBlock::Create(iBuilder->getContext(), "setTermination", doSegmentFunction, 0);
    BasicBlock * stdOutexit = BasicBlock::Create(iBuilder->getContext(), "stdOutexit", doSegmentFunction, 0);
    Constant * blockItems = ConstantInt::get(iBuilder->getSizeTy(), iBuilder->getBitBlockWidth());
    Constant * itemBytes = ConstantInt::get(iBuilder->getSizeTy(), mCodeUnitWidth/8);
    
    Function::arg_iterator args = doSegmentFunction->arg_begin();
    Value * self = &*(args++);
    Value * blocksToDo = &*(args);
    ////iBuilder->CallPrintInt("blocksToDo", blocksToDo);
    Value * streamStructPtr = getStreamSetStructPtr(self, "codeUnitBuffer");
    //iBuilder->CallPrintInt("streamStructPtr", iBuilder->CreatePtrToInt(streamStructPtr, iBuilder->getInt64Ty()));

    LoadInst * producerPos = iBuilder->CreateAtomicLoadAcquire(mStreamSetInputBuffers[0]->getProducerPosPtr(streamStructPtr));
    //iBuilder->CallPrintInt("producerPos", producerPos);
    Value * processed = getProcessedItemCount(self);
    Value * itemsAvail = iBuilder->CreateSub(producerPos, processed);
    Value * itemsMax = iBuilder->CreateMul(blocksToDo, blockItems);
    Value * lessThanFullSegment = iBuilder->CreateICmpULT(itemsAvail, itemsMax);
    Value * itemsToDo = iBuilder->CreateSelect(lessThanFullSegment, itemsAvail, itemsMax);
    
    Value * blockNo = getScalarField(self, blockNoScalar);
    //iBuilder->CallPrintInt("blockNo", blockNo);
    Value * basePtr = getStreamSetBlockPtr(self, "codeUnitBuffer", blockNo);
    //iBuilder->CallPrintInt("basePtr", iBuilder->CreatePtrToInt(basePtr, iBuilder->getInt64Ty()));
    Value * byteOffset = iBuilder->CreateMul(iBuilder->CreateURem(processed, blockItems), itemBytes);
    Value * bytePtr = iBuilder->CreateGEP(iBuilder->CreateBitCast(basePtr, i8PtrTy), byteOffset);

    iBuilder->CreateCall(writefn, std::vector<Value *>({iBuilder->getInt32(1), bytePtr, iBuilder->CreateMul(itemsToDo, itemBytes)}));
    
    processed = iBuilder->CreateAdd(processed, itemsToDo);
    setProcessedItemCount(self, processed);
    setScalarField(self, blockNoScalar, iBuilder->CreateUDiv(processed, blockItems));
    mStreamSetInputBuffers[0]->setConsumerPos(streamStructPtr, processed);

    Value * endSignal = iBuilder->CreateLoad(mStreamSetInputBuffers[0]->getEndOfInputPtr(streamStructPtr));
    Value * inFinalSegment = iBuilder->CreateAnd(endSignal, lessThanFullSegment);
    
    iBuilder->CreateCondBr(inFinalSegment, setTermination, stdOutexit);
    iBuilder->SetInsertPoint(setTermination);

    setTerminationSignal(self);

    iBuilder->CreateBr(stdOutexit);
    iBuilder->SetInsertPoint(stdOutexit);
    iBuilder->CreateRetVoid();
    iBuilder->restoreIP(savePoint);
}

void stdOutKernel::generateFinalBlockMethod() {
    auto savePoint = iBuilder->saveIP();
    Module * m = iBuilder->getModule();
    Function * writefn = create_write(m, iBuilder);
    Function * finalBlockFunction = m->getFunction(mKernelName + finalBlock_suffix);
    Type * i8PtrTy = iBuilder->getInt8PtrTy();
    
    iBuilder->SetInsertPoint(BasicBlock::Create(iBuilder->getContext(), "fb_flush", finalBlockFunction, 0));
    Constant * blockItems = ConstantInt::get(iBuilder->getSizeTy(), iBuilder->getBitBlockWidth());
    Constant * itemBytes = ConstantInt::get(iBuilder->getSizeTy(), mCodeUnitWidth/8);
    Value * self = getParameter(finalBlockFunction, "self");
    Value * streamStructPtr = getStreamSetStructPtr(self, "codeUnitBuffer");
    LoadInst * producerPos = iBuilder->CreateAtomicLoadAcquire(mStreamSetInputBuffers[0]->getProducerPosPtr(streamStructPtr));
    Value * processed = getProcessedItemCount(self);
    Value * itemsAvail = iBuilder->CreateSub(producerPos, processed);
    Value * blockNo = getScalarField(self, blockNoScalar);
    Value * basePtr = getStreamSetBlockPtr(self, "codeUnitBuffer", blockNo);
    Value * byteOffset = iBuilder->CreateMul(iBuilder->CreateURem(processed, blockItems), itemBytes);
    Value * bytePtr = iBuilder->CreateGEP(iBuilder->CreateBitCast(basePtr, i8PtrTy), byteOffset);
    iBuilder->CreateCall(writefn, std::vector<Value *>({iBuilder->getInt32(1), bytePtr, iBuilder->CreateMul(itemsAvail, itemBytes)}));
    setProcessedItemCount(self, producerPos);
    mStreamSetInputBuffers[0]->setConsumerPos(streamStructPtr, producerPos);
    setTerminationSignal(self);
    iBuilder->CreateRetVoid();
    iBuilder->restoreIP(savePoint);
}
}
