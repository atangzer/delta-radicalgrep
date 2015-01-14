/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include "pabloAST.h"
#include "pe_advance.h"
#include "pe_and.h"
#include "pe_call.h"
#include "pe_matchstar.h"
#include "pe_not.h"
#include "pe_or.h"
#include "pabloAST.h"
#include "pe_scanthru.h"
#include "pe_sel.h"
#include "pe_var.h"
#include "pe_xor.h"
#include "pe_zeroes.h"
#include "pe_ones.h"
#include <pablo/codegenstate.h>
#include <llvm/Support/Compiler.h>

namespace pablo {

PabloAST::Allocator PabloAST::mAllocator;

/*

    Return true if expr1 and expr2 can be proven equivalent according to some rules,
    false otherwise.  Note that false may be returned i some cases when the exprs are
    equivalent.

*/

bool equals(const PabloAST * expr1, const PabloAST * expr2) {
    assert (expr1 && expr2);
    if (expr1->getClassTypeId() == expr2->getClassTypeId()) {
        if ((isa<const Zeroes>(expr1)) || (isa<const Ones>(expr1))) {
            return true;
        }
        else if (const Var * var1 = dyn_cast<const Var>(expr1)) {
            if (const Var * var2 = cast<const Var>(expr2)) {
                return (var1->getName() == var2->getName());
            }
        }
        else if (const Not* not1 = dyn_cast<const Not>(expr1)) {
            if (const Not* not2 = cast<const Not>(expr2)) {
                return equals(not1->getExpr(), not2->getExpr());
            }
        }
        else if (const And* and1 = dyn_cast<const And>(expr1)) {
            if (const And* and2 = cast<const And>(expr2)) {
                if (equals(and1->getExpr1(), and2->getExpr1())) {
                    return equals(and1->getExpr2(), and2->getExpr2());
                }
                else if (equals(and1->getExpr1(), and2->getExpr2())) {
                    return equals(and1->getExpr2(), and2->getExpr1());
                }
            }
        }
        else if (const Or * or1 = dyn_cast<const Or>(expr1)) {
            if (const Or* or2 = cast<const Or>(expr2)) {
                if (equals(or1->getExpr1(), or2->getExpr1())) {
                    return equals(or1->getExpr2(), or2->getExpr2());
                }
                else if (equals(or1->getExpr1(), or2->getExpr2())) {
                    return equals(or1->getExpr2(), or2->getExpr1());
                }
            }
        }
        else if (const Xor * xor1 = dyn_cast<const Xor>(expr1)) {
            if (const Xor * xor2 = cast<const Xor>(expr2)) {
                if (equals(xor1->getExpr1(), xor2->getExpr1())) {
                    return equals(xor1->getExpr2(), xor2->getExpr2());
                }
                else if (equals(xor1->getExpr1(), xor2->getExpr2())) {
                    return equals(xor1->getExpr2(), xor2->getExpr1());
                }
            }
        }
        else if (const Sel* sel1 = dyn_cast<const Sel>(expr1)) {
            if (const Sel* sel2 = cast<const Sel>(expr2)) {
                if (equals(sel1->getCondition(), sel2->getCondition())) {
                    if (equals(sel1->getTrueExpr(), sel2->getTrueExpr())) {
                        return equals(sel1->getFalseExpr(), sel2->getFalseExpr());
                    }
                }
            }
        }
    }
    return false;
}

void PabloAST::setMetadata(const std::string & name, PMDNode * node) {
    if (LLVM_UNLIKELY(mMetadataMap == nullptr)) {
        mMetadataMap = new PMDNodeMap();
    }
    mMetadataMap->insert(std::make_pair(name, node));
}

PMDNode * PabloAST::getMetadata(const std::string & name) {
    if (LLVM_UNLIKELY(mMetadataMap == nullptr)) {
        return nullptr;
    }
    auto f = mMetadataMap->find(name);
    if (f == mMetadataMap->end()) {
        return nullptr;
    }
    return f->second;
}

void PabloAST::replaceAllUsesWith(PabloAST * expr) {
    #ifndef NDEBUG
    unsigned __userCount = getNumUses();
    #endif
    while (!mUsers.empty()) {
        PabloAST * user = mUsers.pop_back_val();
        assert(--__userCount == getNumUses());
        if (isa<Statement>(user)) {
            cast<Statement>(user)->replaceUsesOfWith(this, expr);
        }
        assert(__userCount == getNumUses());
    }
    assert (getNumUses() == 0);
}

void Statement::setOperand(const unsigned index, PabloAST * value) {
    assert (index < getNumOperands());
    if (LLVM_UNLIKELY(mOperand[index] == value)) {
        return;
    }
    PabloAST * priorValue = mOperand[index];
    // Test just to be sure we don't have multiple operands pointing to
    // what we're replacing. If not, remove this from the prior value's
    // user list.
    unsigned count = 0;
    for (unsigned i = 0; i != getNumOperands(); ++i) {
        count += (mOperand[index] == priorValue) ? 1 : 0;
    }
    assert (count >= 1);
    if (LLVM_LIKELY(count == 1)) {
        priorValue->removeUser(this);
    }
    mOperand[index] = value;
    value->addUser(this);
}

void Statement::insertBefore(Statement * const statement) {
    assert (statement);
    assert (statement != this);
    assert (statement->mParent);
    removeFromParent();
    mParent = statement->mParent;
    if (LLVM_UNLIKELY(mParent->mFirst == statement)) {
        mParent->mFirst = this;
    }
    mNext = statement;
    mPrev = statement->mPrev;
    statement->mPrev = this;
    if (LLVM_LIKELY(mPrev != nullptr)) {
        mPrev->mNext = this;
    }
}

void Statement::insertAfter(Statement * const statement) {
    assert (statement);
    assert (statement != this);
    assert (statement->mParent);
    removeFromParent();
    mParent = statement->mParent;
    if (LLVM_UNLIKELY(mParent->mLast == statement)) {
        mParent->mLast = this;
    }
    mPrev = statement;
    mNext = statement->mNext;
    statement->mNext = this;
    if (LLVM_LIKELY(mNext != nullptr)) {
        mNext->mPrev = this;
    }
}

Statement * Statement::removeFromParent() {
    Statement * next = mNext;
    if (LLVM_LIKELY(mParent != nullptr)) {
        if (LLVM_UNLIKELY(mParent->mFirst == this)) {
            mParent->mFirst = mNext;
        }
        if (LLVM_UNLIKELY(mParent->mLast == this)) {
            mParent->mLast = mPrev;
        }
        if (mParent->mInsertionPoint == this) {
            mParent->mInsertionPoint = mParent->mInsertionPoint->mPrev;
        }
        if (LLVM_LIKELY(mPrev != nullptr)) {
            mPrev->mNext = mNext;
        }
        if (LLVM_LIKELY(mNext != nullptr)) {
            mNext->mPrev = mPrev;
        }
    }    
    mPrev = nullptr;
    mNext = nullptr;
    mParent = nullptr;
    return next;
}

Statement * Statement::eraseFromParent(const bool recursively) {
    Statement * next = removeFromParent();
    // remove this statement from its operands' users list
    for (PabloAST * op : mOperand) {
        op->removeUser(this);
        if (recursively && isa<Statement>(op) && op->getNumUses() == 0) {
            cast<Statement>(op)->eraseFromParent();
        }
    }
    return next;
}

void Statement::replaceWith(PabloAST * const expr) {

    if (LLVM_UNLIKELY(expr == this)) {
        return;
    }

    if (isa<Statement>(expr)) {
        Statement * stmt = cast<Statement>(expr);
        stmt->removeFromParent();
        stmt->mParent = mParent;
        stmt->mNext = mNext;
        stmt->mPrev = mPrev;
        if (LLVM_LIKELY(mPrev != nullptr)) {
            mPrev->mNext = stmt;
        }
        if (LLVM_LIKELY(mNext != nullptr)) {
            mNext->mPrev = stmt;
        }
        mParent=nullptr;
        mNext=nullptr;
        mPrev=nullptr;
    }
    else {
        removeFromParent();
    }

    // remove this statement from its operands' users list
    for (PabloAST * op : mOperand) {
        op->removeUser(this);
    }

    while (!mUsers.empty()) {
        PabloAST * user = mUsers.pop_back_val();
        if (isa<Statement>(user)) {
            assert(std::count(cast<Statement>(user)->mOperand.begin(), cast<Statement>(user)->mOperand.end(), this) == 1);
            cast<Statement>(user)->replaceUsesOfWith(this, expr);
        }
    }

}

Statement::~Statement() {

}

void StatementList::insert(Statement * const statement) {
    if (LLVM_UNLIKELY(mInsertionPoint == nullptr)) {
        statement->mNext = mFirst;
        if (mFirst) {
            assert (mFirst->mPrev == nullptr);
            mFirst->mPrev = statement;
        }
        mLast = (mLast == nullptr) ? statement : mLast;
        mInsertionPoint = mFirst = statement;
    }
    else {
        statement->insertAfter(mInsertionPoint);
        mLast = (mLast == mInsertionPoint) ? statement : mLast;
        mInsertionPoint = statement;
    }
}

}
