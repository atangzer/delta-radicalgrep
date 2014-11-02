/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include "re_cc.h"
#include <llvm/Support/Compiler.h>

namespace re {

CC::CC(const CC * cc1, const CC * cc2)
: RE(ClassTypeId::CC)
, mSparseCharSet(cc1->cbegin(), cc1->cend()) {
    for (const CharSetItem & i : cc2->mSparseCharSet) {
        insert_range(i.lo_codepoint, i.hi_codepoint);
    }
}

CC::CC(const CC & cc)
: RE(ClassTypeId::CC)
, mSparseCharSet(cc.cbegin(), cc.cend()) {

}

std::string CC::getName() const {
    std::string name = "CC";
    char seperator = '_';
    for (const CharSetItem & i : mSparseCharSet) {
        name += seperator;
        if (i.lo_codepoint == i.hi_codepoint) {
            name += std::to_string(i.lo_codepoint);
        }
        else {
            name += std::to_string(i.lo_codepoint) + "-" + std::to_string(i.hi_codepoint);
        }
        seperator = ',';
    }
    return name;
}

void CC::insert_range(const CodePointType lo_codepoint, const CodePointType hi_codepoint) {
    CharSetItem item(lo_codepoint, hi_codepoint);
    for (auto i = mSparseCharSet.begin(); i != mSparseCharSet.end(); ) {
        CharSetItem & range = *i;
        if (item.hi_codepoint < range.lo_codepoint - 1) {
            mSparseCharSet.insert(i, item);
            return;
        }
        else if (item.lo_codepoint > range.hi_codepoint + 1) {
            ++i;
        }
        else {
            // ranges overlap; expand the range to include the prior one and
            // remove the old one from the list
            range.lo_codepoint = std::min(range.lo_codepoint, item.lo_codepoint);
            range.hi_codepoint = std::max(range.hi_codepoint, item.hi_codepoint);
            return;
        }
    }
    mSparseCharSet.push_back(item);
}

void CC::remove_range(const CodePointType lo_codepoint, const CodePointType hi_codepoint) {
    for (auto i = mSparseCharSet.begin(); i != mSparseCharSet.end(); ) {
        CharSetItem & range = *i;
        if (lo_codepoint > range.hi_codepoint + 1) {
            ++i;
        }
        else if (hi_codepoint < range.lo_codepoint - 1) {
            break;
        }
        else if (lo_codepoint <= range.lo_codepoint && hi_codepoint >= range.hi_codepoint) {
            i = mSparseCharSet.erase(i);
        }
        else if (lo_codepoint <= range.lo_codepoint) {
            range.lo_codepoint = hi_codepoint + 1;
            break;
        }
        else if (hi_codepoint >= range.hi_codepoint) {
            range.hi_codepoint = lo_codepoint - 1;
            ++i;
        }
        else {
            CharSetItem item(hi_codepoint + 1, range.hi_codepoint);
            range.hi_codepoint = lo_codepoint - 1;
            mSparseCharSet.insert(++i, std::move(item));
            break;
        }
    }
}

CC::SetRelationship CC::compare(const CC * other) const {

    if (LLVM_UNLIKELY(other == this)) {
        return SetRelationship::EQUIVALENT;
    }

    auto ai = cbegin();
    const auto ai_end = cend();
    auto bi = other->cbegin();
    const auto bi_end = other->cend();

    bool nonSubset = false;
    bool nonSuperset = false;
    bool disjoint = true;

    while (ai != ai_end && bi != bi_end) {
        const CharSetItem & ra = *ai;
        const CharSetItem & rb = *bi;

        // _A_| ...
        //     |_B_

        // A may be a superset of B but it cannot be a subset of B

        if (ra.hi_codepoint < rb.lo_codepoint) {
            ++ai;
            nonSubset = true;
            continue;
        }

        //     |_A_
        // _B_| ...

        // A may be a subset of B but it cannot be a superset of B


        if (rb.hi_codepoint < ra.lo_codepoint) {
            ++bi;
            nonSuperset = true;
            continue;
        }

        disjoint = false;

        // |_A__
        //   |_B__

        // A may be a superset of B but it cannot be a subset of B nor can it be disjoint

        if (ra.lo_codepoint < rb.lo_codepoint) {
            nonSubset = true;
        }

        //   |_A__
        // |_B__

        // A may be a subset of B but it cannot be a superset of B nor can it be disjoint

        if (rb.lo_codepoint < ra.lo_codepoint) {
            nonSuperset = true;
        }

        // __A__| ...
        // __B______|

        // SUCC(A) may overlap B in some way; only increment A

        if (ra.hi_codepoint <= rb.hi_codepoint) {
            ++ai;
        }

        // __A______|
        // __B__| ...

        // SUCC(B) may overlap A in some way; only increment B

        if (rb.hi_codepoint <= ra.hi_codepoint) {
            ++bi;
        }

    }
    if (disjoint) {
        return SetRelationship::DISJOINT;
    }

    if (ai == ai_end && bi != bi_end) {
        nonSuperset = true;
    }
    else if (bi == bi_end && ai != ai_end) {
        nonSubset = true;
    }

    if (nonSubset && nonSuperset) {
        return SetRelationship::OVERLAPPING;
    }
    else if (nonSubset) {
        return SetRelationship::SUPERSET;
    }
    else if (nonSuperset) {
        return SetRelationship::SUBSET;
    }
    return SetRelationship::EQUIVALENT;
}

}
