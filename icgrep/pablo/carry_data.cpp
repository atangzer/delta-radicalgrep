/*
 *  Copyright (c) 2015 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */


#include <include/simd-lib/bitblock.hpp>
#include <pablo/pablo_compiler.h>
#include <pablo/codegenstate.h>
#include <pablo/carry_data.h>
#include <iostream>

namespace pablo {

void PabloBlockCarryData::enumerateLocal() {
    for (Statement * stmt : *theScope) {
        if (Advance * adv = dyn_cast<Advance>(stmt)) {
            unsigned shift_amount = adv->getAdvanceAmount();
            if (!adv->isMod64()) {
                if (shift_amount == 1) {
                    adv->setLocalAdvanceIndex(advance1.entries);
                    advance1.entries++;                
                }
                else if (shift_amount < LongAdvanceBase) {
                    // short Advance
                    if (mITEMS_PER_PACK >= LongAdvanceBase) {
                        // Packing is possible.   We will use the allocated bit position as
                        // the index.
                        if (roomInFinalPack(shortAdvance.allocatedBits) < shift_amount) {
                            // Start a new pack.
                            shortAdvance.allocatedBits = alignCeiling(shortAdvance.allocatedBits, mPACK_SIZE);
                        }
                        adv->setLocalAdvanceIndex(shortAdvance.allocatedBits);
                    }
                    else {
                        adv->setLocalAdvanceIndex(shortAdvance.entries);
                    }
                    shortAdvance.entries++;
                    shortAdvance.allocatedBits += shift_amount;
                }
                else {
                    adv->setLocalAdvanceIndex(longAdvance.allocatedBitBlocks);
                    longAdvance.entries++;
                    longAdvance.allocatedBitBlocks += longAdvanceBufferSize(shift_amount);
                }
            }
        }
        else if (MatchStar * m = dyn_cast<MatchStar>(stmt)) {
            if (!m->isMod64()) {
                m->setLocalCarryIndex(addWithCarry.entries);
                ++addWithCarry.entries;
            }
        }
        else if (ScanThru * s = dyn_cast<ScanThru>(stmt)) {
            if (!s->isMod64()) {
                s->setLocalCarryIndex(addWithCarry.entries);
                ++addWithCarry.entries;
            }
        }
    }
    longAdvance.frameOffset = 0;
    shortAdvance.frameOffset = longAdvance.frameOffset + longAdvance.allocatedBitBlocks * mPOSITIONS_PER_BLOCK;
    if (mITEMS_PER_PACK == mPACK_SIZE) {
        addWithCarry.frameOffset = shortAdvance.frameOffset + shortAdvance.allocatedBits;
        if (roomInFinalPack(addWithCarry.frameOffset) < addWithCarry.entries) {
            addWithCarry.frameOffset = alignCeiling(addWithCarry.frameOffset, mPACK_SIZE);
        }
        advance1.frameOffset = addWithCarry.frameOffset + addWithCarry.entries;
        if (roomInFinalPack(advance1.frameOffset) < advance1.entries) {
            advance1.frameOffset = alignCeiling(advance1.frameOffset, mPACK_SIZE);
        }
    }
    else {
        addWithCarry.frameOffset = shortAdvance.frameOffset + shortAdvance.entries;
        advance1.frameOffset = addWithCarry.frameOffset + addWithCarry.entries;

    }
    nested.frameOffset = advance1.frameOffset + advance1.entries;
}
        
void PabloBlockCarryData::dumpCarryData(llvm::raw_ostream & strm) {
    unsigned totalDepth = ifDepth + whileDepth;
    for (int i = 0; i < totalDepth; i++) strm << "  ";
    strm << "scope index = " << theScope->getScopeIndex();
    strm << " framePosition: " << framePosition << ", ifDepth: " << ifDepth << ", whileDepth:" << whileDepth << ", maxNestingDepth: " << maxNestingDepth << "\n";
    for (int i = 0; i < totalDepth; i++) strm << "  ";
    strm << "longAdvance: offset = " << longAdvance.frameOffset << ", entries = " << longAdvance.entries << "\n";
    for (int i = 0; i < totalDepth; i++) strm << "  ";
    strm << "shortAdvance: offset = " << shortAdvance.frameOffset << ", entries = " << shortAdvance.entries << "\n";
    for (int i = 0; i < totalDepth; i++) strm << "  ";
    strm << "advance1: offset = " << advance1.frameOffset << ", entries = " << advance1.entries << "\n";
    for (int i = 0; i < totalDepth; i++) strm << "  ";
    strm << "addWithCarry: offset = " << addWithCarry.frameOffset << ", entries = " << addWithCarry.entries << "\n";
    for (int i = 0; i < totalDepth; i++) strm << "  ";
    strm << "nested: offset = " << nested.frameOffset << ", allocatedBits = " << nested.allocatedBits << "\n";
    for (int i = 0; i < totalDepth; i++) strm << "  ";
    strm << "summary: offset = " << summary.frameOffset << "\n";
    for (int i = 0; i < totalDepth; i++) strm << "  ";
    strm << "scopeCarryDataSize = " << scopeCarryDataSize  << "\n";
    strm.flush();
    
}

}
