/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <pablo/pe_sel.h>
#include <pablo/codegenstate.h>
#include <pablo/pe_ones.h>
#include <pablo/pe_zeroes.h>
#include <pablo/pe_not.h>


namespace pablo {

Sel::Sel(PabloAST* if_expr, PabloAST* t_expr, PabloAST* f_expr, PabloBlock * parent)
: Statement(ClassTypeId::Sel, {{if_expr, t_expr, f_expr}}, parent->makeName("sel"), parent)
{

}

PabloAST * OptimizeSel::operator()(PabloAST * if_expr, PabloAST * t_expr, PabloAST * f_expr, PabloBlock * pb) {

    assert (if_expr && t_expr && f_expr && pb);

    if (isa<Ones>(if_expr)) {
        return t_expr;
    }
    else if (isa<Zeroes>(if_expr)){
        return f_expr;
    }
    else if (isa<Ones>(t_expr)) {
        return pb->createOr(if_expr, f_expr);
    }
    else if (isa<Zeroes>(t_expr)){
        return pb->createAnd(pb->createNot(if_expr), f_expr);
    }
    else if (isa<Ones>(f_expr)) {
        return pb->createOr(pb->createNot(if_expr), t_expr);
    }
    else if (isa<Zeroes>(f_expr)){
        return pb->createAnd(if_expr, t_expr);
    }
    else if (equals(t_expr, f_expr)) {
        return t_expr;
    }
    else if (isa<Not>(t_expr) && equals(cast<Not>(t_expr)->getExpr(), f_expr)) {
        return pb->createXor(if_expr, f_expr);
    }
    else if (isa<Not>(f_expr) && equals(t_expr, cast<Not>(f_expr)->getExpr())){
        return pb->createXor(if_expr, f_expr);
    }
    return pb->createSelImm(if_expr, t_expr, f_expr);
}

}
