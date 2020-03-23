/*
 *  Copyright (c) 2019 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */

#ifndef UTF16_GEN_H
#define UTF16_GEN_H

#include <pablo/pablo_kernel.h>
#include <kernel/core/kernel_builder.h>

//
// UTF-16 encoding requires one or two code units per Unicode character.
// To generate UTF-16 encoded output from sets of basis bit streams
// representing Unicode characters (that is, codepoint-indexed streams
// having one bit position per codepoint), deposit masks are needed
// to identify the positions at which bits for each character are
// to be deposited.   A UTF-16 deposit mask will have one or two bit
// positions per character depending on the character being encoded, that is,
// depending on the number of code units needed to encode the character.   Within
// each group of one to two positions for a single character, a deposit mask
// must have exactly one 1 bit set.  Different deposit masks are used for
// depositing bits, depending on the destination code unit position within the
// ultimate code unit sequence.
//
// The following deposit masks (shown in little-endian representation) are
// used for depositing bits.
//
//  UTF-16 sequence length:          1     2
//  Unicode bit position:
//  Unicode codepoint bits 0-9       1    10   u16final
//  Bits 9-15, 16-20                 1    01   u16initial
//
//  To compute UTF-16 deposit masks, we begin by constructing an extraction
//  mask having 3 bit positions per character, but with the number of
//  1 bits to be kept dependent on the sequence length.  When this extraction
//  mask is applied to the repeating constant 2-bit mask 10, u16final above
//  is produced.
//
//  UTF-16 sequence length:          1     2
//  extraction mask                 10    11
//  constant mask                   10    10
//  final position mask             1     10
//  From this mask, u16initial may subsequently computed by
//  bitwise logic and shifting.
//
//  The UTF16fieldDepositMask kernel produces this deposit mask
//  within 64-bit fields.


class UTF16fieldDepositMask final : public kernel::BlockOrientedKernel {
public:
    UTF16fieldDepositMask(BuilderRef b, kernel::StreamSet * u32basis, kernel::StreamSet * u16fieldMask, kernel::StreamSet * extractionMask, unsigned depositFieldWidth = sizeof(size_t) * 8);
private:
    void generateDoBlockMethod(BuilderRef b) override;
    void generateFinalBlockMethod(BuilderRef b, llvm::Value * const remainingBytes) override;
    const unsigned mDepositFieldWidth;
};

//
// Given a u16-indexed bit stream marking the final code unit position
// of each UTF-8 sequence, this kernel computes the deposit mask
// u16initial.
//
class UTF16_InitialMask : public pablo::PabloKernel {
public:
    UTF16_InitialMask(BuilderRef kb, kernel::StreamSet * u16final, kernel::StreamSet * u16initial);
protected:
    void generatePabloMethod() override;
};

// The high 5 bits of the 21-bit Unicode basis encode the Unicode plane number
// with the basic multilingual plane being plane 0, and supplementary planes
// number 1 through 16.   UTF-16 uses a 4-bit numbering of 0-15 for the supplementary
// planes, determined by subtracting 1 from the Unicode plane number.
class UTF16_SupplementaryBasis : public pablo::PabloKernel {
public:
    UTF16_SupplementaryBasis(BuilderRef kb, kernel::StreamSet * u32basis, kernel::StreamSet * u16_SMP_basis);
protected:
    void generatePabloMethod() override;
};

// This kernel assembles the UTF-16 basis bit data, given three sets of deposited
// bits: SMPbits4_0, u16bits15_10, u16bits9_0, as well as the mask_lo stream
// (having bits set at all but surrogate1 positions).
//
class UTF16assembly : public pablo::PabloKernel {
public:
    UTF16assembly(BuilderRef kb,
                 kernel::StreamSet * SMPbits4_0, kernel::StreamSet * u16bits15_10, kernel::StreamSet * u16bits9_0, kernel::StreamSet * u16final,
                 kernel::StreamSet * u16basis);
protected:
    void generatePabloMethod() override;
};

#endif
