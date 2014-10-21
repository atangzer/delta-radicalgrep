/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <pablo/codegenstate.h>

namespace pablo {

/// UNARY CREATE FUNCTIONS

PabloAST * PabloBlock::createAdvance(PabloAST * expr) {
//    if (isa<Zeroes>(expr)) {
//        return createZeroes();
//    }
    return mUnary.findOrMake<Advance>(PabloAST::ClassTypeId::Advance, expr);
}

Call * PabloBlock::createCall(const std::string name) {
    return mUnary.findOrMake<Call>(PabloAST::ClassTypeId::Call, mSymbolGenerator.get(name));
}

PabloAST * PabloBlock::createNot(PabloAST * expr) {
//    if (isa<Zeroes>(expr)) {
//        return createOnes();
//    }
//    else if (isa<Ones>(expr)) {
//        return createZeroes();
//    }
    return mUnary.findOrCall<OptimizeNot>(PabloAST::ClassTypeId::Not, expr);
}

Var * PabloBlock::createVar(String * name) {
    return mUnary.findOrMake<Var>(PabloAST::ClassTypeId::Var, name);
}

/// BINARY CREATE FUNCTIONS

Next * PabloBlock::createNext(Assign * assign, PabloAST * expr) {
    Next * next = mBinary.findOrMake<Next>(PabloAST::ClassTypeId::Next, assign, expr);
    mStatements.push_back(next);
    return next;
}

PabloAST * PabloBlock::createMatchStar(PabloAST * marker, PabloAST * charclass) {
//    if (isa<Zeroes>(marker)) {
//        return createZeroes();
//    }
//    else if (isa<Zeroes>(charclass)) {
//        return marker;
//    }
    return mBinary.findOrMake<MatchStar>(PabloAST::ClassTypeId::MatchStar, marker, charclass);
}

PabloAST * PabloBlock::createScanThru(PabloAST * from, PabloAST * thru) {
//    if (isa<Zeroes>(from)) {
//        return createZeroes();
//    }
//    else if (isa<Zeroes>(thru)) {
//        return marker;
//    }
    return mBinary.findOrMake<ScanThru>(PabloAST::ClassTypeId::ScanThru, from, thru);
}

PabloAST * PabloBlock::createAnd(PabloAST * expr1, PabloAST * expr2) {
    if (expr1 < expr2) {
        std::swap(expr1, expr2);
    }
    return mBinary.findOrCall<OptimizeAnd>(PabloAST::ClassTypeId::And, expr1, expr2);
}

PabloAST * PabloBlock::createOr(PabloAST * expr1, PabloAST * expr2) {
    if (expr1 < expr2) {
        std::swap(expr1, expr2);
    }
    return mBinary.findOrCall<OptimizeOr>(PabloAST::ClassTypeId::Or, expr1, expr2);
}

PabloAST * PabloBlock::createXor(PabloAST * expr1, PabloAST * expr2) {
    if (expr1 < expr2) {
        std::swap(expr1, expr2);
    }
    return mBinary.findOrCall<OptimizeXor>(PabloAST::ClassTypeId::Xor, expr1, expr2);
}

/// TERNARY CREATE FUNCTION

PabloAST *PabloBlock::createSel(PabloAST * condition, PabloAST * trueExpr, PabloAST * falseExpr) {
    return mTernary.findOrCall<OptimizeSel>(PabloAST::ClassTypeId::Sel, condition, trueExpr, falseExpr);
}

}
