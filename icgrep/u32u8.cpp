/*
 *  Copyright (c) 2018 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <IR_Gen/idisa_target.h>                   // for GetIDISA_Builder
#include <cc/cc_compiler.h>                        // for CC_Compiler
#include <kernels/deletion.h>                      // for DeletionKernel
#include <kernels/source_kernel.h>
#include <kernels/p2s_kernel.h>                    // for P2S16KernelWithCom...
#include <kernels/s2p_kernel.h>                    // for S2PKernel
#include <kernels/stdout_kernel.h>                 // for StdOutKernel_
#include <kernels/pdep_kernel.h>
#include <llvm/IR/Function.h>                      // for Function, Function...
#include <llvm/IR/Module.h>                        // for Module
#include <llvm/Support/CommandLine.h>              // for ParseCommandLineOp...
#include <llvm/Support/Debug.h>                    // for dbgs
#include <pablo/pablo_kernel.h>                    // for PabloKernel
#include <pablo/pablo_toolchain.h>                 // for pablo_function_passes
#include <kernels/kernel_builder.h>
#include <pablo/pe_zeroes.h>
#include <toolchain/toolchain.h>
#include <toolchain/cpudriver.h>
#include <kernels/core/streamset.h>
#include <kernels/hex_convert.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/CallingConv.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/Support/Compiler.h>
#include <llvm/Support/raw_ostream.h>
#include <pablo/builder.hpp>
#include <fcntl.h>
#include <kernels/pipeline_builder.h>

using namespace pablo;
using namespace kernel;
using namespace llvm;
using namespace codegen;

static cl::OptionCategory u32u8Options("u32u8 Options", "Transcoding control options.");
static cl::opt<std::string> inputFile(cl::Positional, cl::desc("<input file>"), cl::Required, cl::cat(u32u8Options));

//
// UTF-8 encoding requires one to four bytes per Unicode character.
// To generate UTF-8 encoded output from sets of basis bit streams
// representing Unicode characters (that is, codepoint-indexed streams
// having one bit position per codepoint), deposit masks are needed
// to identify the positions at which bits for each character are
// to be deposited.   A UTF-8 deposit mask will have one to four bit
// positions per character depending on the character being encoded, that is,
// depending on the number of bytes needed to encode the character.   Within
// each group of one to four positions for a single character, a deposit mask
// must have exactly one 1 bit set.  Different deposit masks are used for
// depositing bits, depending on the destination byte position within the
// ultimate 4 byte sequencE->
//
// The following deposit masks (shown in little-endian representation) are
// used for depositing bits.
//
//  UTF-8 sequence length:          1     2     3       4
//  Unicode bit position:
//  Unicode codepoint bits 0-5      1    10   100    1000    u8final
//  Bits 6-11                       1    01   010    0100    u8mask6_11
//  Bits 12-17                      1    01   001    0010    u8mask12_17
//  Bits 18-20                      1    01   001    0001    u8initial
//
//  To compute UTF-8 deposit masks, we begin by constructing an extraction
//  mask having 4 bit positions per character, but with the number of
//  1 bits to be kept dependent on the sequence length.  When this extraction
//  mask is applied to the repeating constant 4-bit mask 1000, u8final above
//  is produced.
//
//  UTF-8 sequence length:             1     2     3       4
//  extraction mask                 1000  1100  1110    1111
//  constant mask                   1000  1000  1000    1000
//  final position mask             1     10    100     1000
//  From this mask, other masks may subsequently computed by
//  bitwise logic and shifting.
//
//  The UTF8fieldDepositMask kernel produces this deposit mask
//  within 64-bit fields.

class UTF8fieldDepositMask final : public BlockOrientedKernel {
public:
    UTF8fieldDepositMask(const std::unique_ptr<KernelBuilder> & b, StreamSet * u32basis, StreamSet * u8fieldMask, StreamSet * u8unitCounts, unsigned depositFieldWidth = sizeof(size_t) * 8);
private:
    void generateDoBlockMethod(const std::unique_ptr<KernelBuilder> & b) override;
    void generateFinalBlockMethod(const std::unique_ptr<KernelBuilder> & b, llvm::Value * const remainingBytes) override;
    const unsigned mDepositFieldWidth;
};

UTF8fieldDepositMask::UTF8fieldDepositMask(const std::unique_ptr<KernelBuilder> & b, StreamSet * u32basis, StreamSet * u8fieldMask, StreamSet * u8unitCounts, unsigned depositFieldWidth)
: BlockOrientedKernel(b, "u8depositMask",
{Binding{"basis", u32basis}},
{Binding{"fieldDepositMask", u8fieldMask, FixedRate(4)},
Binding{"extractionMask", u8unitCounts, FixedRate(4)}},
{}, {},
{InternalScalar{ScalarType::NonPersistent, b->getBitBlockType(), "EOFmask"}})
, mDepositFieldWidth(depositFieldWidth) {

}


void UTF8fieldDepositMask::generateDoBlockMethod(const std::unique_ptr<KernelBuilder> & b) {
    Value * fileExtentMask = b->CreateNot(b->getScalarField("EOFmask"));
    // If any of bits 16 through 20 are 1, a four-byte UTF-8 sequence is required.
    Value * u8len4 = b->loadInputStreamBlock("basis", b->getSize(16), b->getSize(0));
    u8len4 = b->CreateOr(u8len4, b->loadInputStreamBlock("basis", b->getSize(17), b->getSize(0)));
    u8len4 = b->CreateOr(u8len4, b->loadInputStreamBlock("basis", b->getSize(18), b->getSize(0)));
    u8len4 = b->CreateOr(u8len4, b->loadInputStreamBlock("basis", b->getSize(19), b->getSize(0)));
    u8len4 = b->CreateOr(u8len4, b->loadInputStreamBlock("basis", b->getSize(20), b->getSize(0)), "u8len4");
    u8len4 = b->CreateAnd(u8len4, fileExtentMask);
    Value * u8len34 = u8len4;
    // Otherwise, if any of bits 11 through 15 are 1, a three-byte UTF-8 sequence is required.
    u8len34 = b->CreateOr(u8len34, b->loadInputStreamBlock("basis", b->getSize(11), b->getSize(0)));
    u8len34 = b->CreateOr(u8len34, b->loadInputStreamBlock("basis", b->getSize(12), b->getSize(0)));
    u8len34 = b->CreateOr(u8len34, b->loadInputStreamBlock("basis", b->getSize(13), b->getSize(0)));
    u8len34 = b->CreateOr(u8len34, b->loadInputStreamBlock("basis", b->getSize(14), b->getSize(0)));
    u8len34 = b->CreateOr(u8len34, b->loadInputStreamBlock("basis", b->getSize(15), b->getSize(0)));
    u8len34 = b->CreateAnd(u8len34, fileExtentMask);
    Value * nonASCII = u8len34;
    // Otherwise, if any of bits 7 through 10 are 1, a two-byte UTF-8 sequence is required.
    nonASCII = b->CreateOr(nonASCII, b->loadInputStreamBlock("basis", b->getSize(7), b->getSize(0)));
    nonASCII = b->CreateOr(nonASCII, b->loadInputStreamBlock("basis", b->getSize(8), b->getSize(0)));
    nonASCII = b->CreateOr(nonASCII, b->loadInputStreamBlock("basis", b->getSize(9), b->getSize(0)));
    nonASCII = b->CreateOr(nonASCII, b->loadInputStreamBlock("basis", b->getSize(10), b->getSize(0)), "nonASCII");
    nonASCII = b->CreateAnd(nonASCII, fileExtentMask);
    //
    //  UTF-8 sequence length:    1     2     3       4
    //  extraction mask        1000  1100  1110    1111
    //  interleave u8len3|u8len4, allOnes() for bits 1, 3:  x..., ..x.
    //  interleave prefix4, u8len2|u8len3|u8len4 for bits 0, 2:  .x.., ...x

    Value * maskA_lo = b->esimd_mergel(1, u8len34, fileExtentMask);
    Value * maskA_hi = b->esimd_mergeh(1, u8len34, fileExtentMask);
    Value * maskB_lo = b->esimd_mergel(1, u8len4, nonASCII);
    Value * maskB_hi = b->esimd_mergeh(1, u8len4, nonASCII);
    Value * extraction_mask[4];
    extraction_mask[0] = b->esimd_mergel(1, maskB_lo, maskA_lo);
    extraction_mask[1] = b->esimd_mergeh(1, maskB_lo, maskA_lo);
    extraction_mask[2] = b->esimd_mergel(1, maskB_hi, maskA_hi);
    extraction_mask[3] = b->esimd_mergeh(1, maskB_hi, maskA_hi);
    const unsigned bw = b->getBitBlockWidth();
    Constant * mask1000 = Constant::getIntegerValue(b->getIntNTy(bw), APInt::getSplat(bw, APInt::getHighBitsSet(4, 1)));
    for (unsigned j = 0; j < 4; ++j) {
        Value * deposit_mask = b->simd_pext(mDepositFieldWidth, mask1000, extraction_mask[j]);
        b->storeOutputStreamBlock("fieldDepositMask", b->getSize(0), b->getSize(j), deposit_mask);
        b->storeOutputStreamBlock("extractionMask", b->getSize(0), b->getSize(j), extraction_mask[j]);
    }
}
void UTF8fieldDepositMask::generateFinalBlockMethod(const std::unique_ptr<KernelBuilder> & b, Value * const remainingBytes) {
    // Standard Pablo convention for final block processing: set a bit marking
    // the position just past EOF, as well as a mask marking all positions past EOF.
    b->setScalarField("EOFmask", b->bitblock_mask_from(remainingBytes));
    CreateDoBlockMethodCall(b);
}


//
// Given a u8-indexed bit stream marking the final code unit position
// of each UTF-8 sequence, this kernel computes the deposit masks
// u8initial, u8mask12_17, and u8mask6_11.
//
class UTF8_DepositMasks : public pablo::PabloKernel {
public:
    UTF8_DepositMasks(const std::unique_ptr<KernelBuilder> & kb, StreamSet * u8final, StreamSet * u8initial, StreamSet * u8mask12_17, StreamSet * u8mask6_11);
    bool isCachable() const override { return true; }
    bool hasSignature() const override { return false; }
protected:
    void generatePabloMethod() override;
};

UTF8_DepositMasks::UTF8_DepositMasks (const std::unique_ptr<KernelBuilder> & iBuilder, StreamSet * u8final, StreamSet * u8initial, StreamSet * u8mask12_17, StreamSet * u8mask6_11)
: PabloKernel(iBuilder, "UTF8_DepositMasks",
              {Binding{"u8final", u8final, FixedRate(1), LookAhead(2)}},
              {Binding{"u8initial", u8initial},
               Binding{"u8mask12_17", u8mask12_17},
               Binding{"u8mask6_11", u8mask6_11}}) {}

void UTF8_DepositMasks::generatePabloMethod() {
    PabloBuilder pb(getEntryScope());
    PabloAST * u8final = pb.createExtract(getInputStreamVar("u8final"), pb.getInteger(0));
    PabloAST * nonFinal = pb.createNot(u8final, "nonFinal");
    PabloAST * initial = pb.createInFile(pb.createNot(pb.createAdvance(nonFinal, 1)), "u8initial");
    PabloAST * ASCII = pb.createAnd(u8final, initial);
    PabloAST * lookAheadFinal = pb.createLookahead(u8final, 1, "lookaheadFinal");
    // Eliminate lookahead positions that are the final position of the prior unit.
    PabloAST * secondLast = pb.createAnd(lookAheadFinal, nonFinal);
    PabloAST * u8mask6_11 = pb.createInFile(pb.createOr(secondLast, ASCII, "u8mask6_11"));
    PabloAST * prefix2 = pb.createAnd(secondLast, initial);
    PabloAST * lookAhead2 = pb.createLookahead(u8final, 2, "lookahead2");
    PabloAST * thirdLast = pb.createAnd(pb.createAnd(lookAhead2, nonFinal), pb.createNot(secondLast));
    PabloAST * u8mask12_17 = pb.createInFile(pb.createOr(thirdLast, pb.createOr(prefix2, ASCII), "u8mask12_17"));
    pb.createAssign(pb.createExtract(getOutputStreamVar("u8initial"), pb.getInteger(0)), initial);
    pb.createAssign(pb.createExtract(getOutputStreamVar("u8mask6_11"), pb.getInteger(0)), u8mask6_11);
    pb.createAssign(pb.createExtract(getOutputStreamVar("u8mask12_17"), pb.getInteger(0)), u8mask12_17);
}

// This kernel assembles the UTF-8 basis bit data, given four sets of deposited
// bits bits 18-20, 11-17, 6-11 and 0-5, as weil as the marker streams u8initial,
// u8final, u8prefix3 and u8prefix4.
//
class UTF8assembly : public pablo::PabloKernel {
public:
    UTF8assembly(const std::unique_ptr<KernelBuilder> & kb,
                 StreamSet * deposit18_20, StreamSet * deposit12_17, StreamSet * deposit6_11, StreamSet * deposit0_5,
                 StreamSet * u8initial, StreamSet * u8final, StreamSet * u8mask6_11, StreamSet * u8mask12_17,
                 StreamSet * u8basis);
    bool isCachable() const override { return true; }
    bool hasSignature() const override { return false; }
protected:
    void generatePabloMethod() override;
};

UTF8assembly::UTF8assembly (const std::unique_ptr<KernelBuilder> & b,
                            StreamSet * deposit18_20, StreamSet * deposit12_17, StreamSet * deposit6_11, StreamSet * deposit0_5,
                            StreamSet * u8initial, StreamSet * u8final, StreamSet * u8mask6_11, StreamSet * u8mask12_17,
                            StreamSet * u8basis)
: PabloKernel(b, "UTF8assembly",
{Binding{"dep0_5", deposit0_5, FixedRate(1)},
 Binding{"dep6_11", deposit6_11, FixedRate(1), ZeroExtended()},
 Binding{"dep12_17", deposit12_17, FixedRate(1), ZeroExtended()},
 Binding{"dep18_20", deposit18_20, FixedRate(1), ZeroExtended()},
 Binding{"u8initial", u8initial},
 Binding{"u8final", u8final},
 Binding{"u8mask6_11", u8mask6_11},
 Binding{"u8mask12_17", u8mask12_17}},
{Binding{"u8basis", u8basis}}) {

}

void UTF8assembly::generatePabloMethod() {
    PabloBuilder pb(getEntryScope());
    std::vector<PabloAST *> dep18_20 = getInputStreamSet("dep18_20");
    std::vector<PabloAST *> dep12_17 = getInputStreamSet("dep12_17");
    std::vector<PabloAST *> dep6_11 = getInputStreamSet("dep6_11");
    std::vector<PabloAST *> dep0_5 = getInputStreamSet("dep0_5");
    PabloAST * u8initial = pb.createExtract(getInputStreamVar("u8initial"), pb.getInteger(0));
    PabloAST * u8final = pb.createExtract(getInputStreamVar("u8final"), pb.getInteger(0));
    PabloAST * u8mask6_11 = pb.createExtract(getInputStreamVar("u8mask6_11"), pb.getInteger(0));
    PabloAST * u8mask12_17 = pb.createExtract(getInputStreamVar("u8mask12_17"), pb.getInteger(0));
    PabloAST * ASCII = pb.createAnd(u8initial, u8final);
    PabloAST * nonASCII = pb.createNot(ASCII, "nonASCII");
    PabloAST * u8basis[8];
    //
    // Deposit bit 6 is either used for bit 6 of an ASCII code unit, or
    // bit 0 for nonASCII units.   Extract the ASCII case separately.
    PabloAST * ASCIIbit6 = pb.createAnd(dep6_11[0], ASCII);
    dep6_11[0] = pb.createAnd(dep6_11[0], nonASCII);
    for (unsigned i = 0; i < 6; i++) {
        u8basis[i] = pb.createOr(dep0_5[i], dep6_11[i]);
        u8basis[i] = pb.createOr(u8basis[i], dep12_17[i], "basis" + std::to_string(i));
        if (i < 3) u8basis[i] = pb.createOr(u8basis[i], dep18_20[i]);
    }
    // The high bit of UTF-8 prefix and suffix bytes (any nonASCII byte) is always 1.
    u8basis[7] = nonASCII;
    // The second highest bit of UTF-8 units is 1 for any prefix, or ASCII byte with
    // a 1 in bit 6 of the Unicode representation.
    u8basis[6] = pb.createOr(pb.createAnd(u8initial, nonASCII), ASCIIbit6, "basis6");
    //
    // For any prefix of a 3-byte or 4-byte sequence the third highest bit is set to 1.
    u8basis[5] = pb.createOr(u8basis[5], pb.createAnd(u8initial, pb.createNot(u8mask6_11)), "basis5");
    // For any prefix of a 4-byte sequence the fourth highest bit is set to 1.
    u8basis[4] = pb.createOr(u8basis[4], pb.createAnd(u8initial, pb.createNot(u8mask12_17)), "basis4");
    for (unsigned i = 0; i < 8; i++) {
        pb.createAssign(pb.createExtract(getOutputStreamVar("u8basis"), pb.getInteger(i)), u8basis[i]);
    }
}

void deposit(const std::unique_ptr<ProgramBuilder> & P, const unsigned base, const unsigned count, StreamSet * mask, StreamSet * inputs, StreamSet * outputs) {
    StreamSet * const expanded = P->CreateStreamSet(count);
    P->CreateKernelCall<StreamExpandKernel>(inputs, base, mask, expanded);
    if (AVX2_available() && BMI2_available()) {
        P->CreateKernelCall<PDEPFieldDepositKernel>(mask, expanded, outputs);
    } else {
        P->CreateKernelCall<FieldDepositKernel>(mask, expanded, outputs);
    }
}

typedef void (*u32u8FunctionType)(uint32_t fd);

u32u8FunctionType u32u8_gen (CPUDriver & pxDriver) {

    auto & iBuilder = pxDriver.getBuilder();
    Type * const int32Ty = iBuilder->getInt32Ty();
    auto P = pxDriver.makePipeline({Binding{int32Ty, "fd"}});

    Scalar * const fileDescriptor = P->getInputScalar("fd");

    // Source data
    StreamSet * const codeUnitStream = P->CreateStreamSet(1, 32);
    P->CreateKernelCall<MMapSourceKernel>(fileDescriptor, codeUnitStream);

    // Source buffers for transposed UTF-32 basis bits.
    StreamSet * const u32basis = P->CreateStreamSet(21);
    P->CreateKernelCall<S2P_21Kernel>(codeUnitStream, u32basis);

    // Buffers for calculated deposit masks.
    StreamSet * const u8fieldMask = P->CreateStreamSet();
    StreamSet * const u8final = P->CreateStreamSet();
    StreamSet * const u8initial = P->CreateStreamSet();
    StreamSet * const u8mask12_17 = P->CreateStreamSet();
    StreamSet * const u8mask6_11 = P->CreateStreamSet();

    // Intermediate buffers for deposited bits
    StreamSet * const deposit18_20 = P->CreateStreamSet(3);
    StreamSet * const deposit12_17 = P->CreateStreamSet(6);
    StreamSet * const deposit6_11 = P->CreateStreamSet(6);
    StreamSet * const deposit0_5 = P->CreateStreamSet(6);

    // Final buffers for computed UTF-8 basis bits and byte stream.
    StreamSet * const u8basis = P->CreateStreamSet(8);
    StreamSet * const u8bytes = P->CreateStreamSet(1, 8);

    // Calculate the u8final deposit mask.
    StreamSet * const extractionMask = P->CreateStreamSet();
    P->CreateKernelCall<UTF8fieldDepositMask>(u32basis, u8fieldMask, extractionMask);
    P->CreateKernelCall<StreamCompressKernel>(u8fieldMask, extractionMask, u8final);

    P->CreateKernelCall<UTF8_DepositMasks>(u8final, u8initial, u8mask12_17, u8mask6_11);

    deposit(P, 18, 3, u8initial, u32basis, deposit18_20);
    deposit(P, 12, 6, u8mask12_17, u32basis, deposit12_17);
    deposit(P, 6, 6, u8mask6_11, u32basis, deposit6_11);
    deposit(P, 0, 6, u8final, u32basis, deposit0_5);

    P->CreateKernelCall<UTF8assembly>(deposit18_20, deposit12_17, deposit6_11, deposit0_5,
                                      u8initial, u8final, u8mask6_11, u8mask12_17,
                                      u8basis);

    P->CreateKernelCall<P2SKernel>(u8basis, u8bytes);

    P->CreateKernelCall<StdOutKernel>(u8bytes);

    return reinterpret_cast<u32u8FunctionType>(P->compile());
}

int main(int argc, char *argv[]) {
    codegen::ParseCommandLineOptions(argc, argv, {&u32u8Options, pablo::pablo_toolchain_flags(), codegen::codegen_flags()});
    CPUDriver pxDriver("u32u8");
    auto u32u8Function = u32u8_gen(pxDriver);
    const int fd = open(inputFile.c_str(), O_RDONLY);
    if (LLVM_UNLIKELY(fd == -1)) {
        errs() << "Error: cannot open " << inputFile << " for processing. Skipped.\n";
    } else {
        u32u8Function(fd);
        close(fd);
    }
    return 0;
}
