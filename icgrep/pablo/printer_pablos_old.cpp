/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include "printer_pablos.h"
#include <pablo/arithmetic.h>
#include <pablo/boolean.h>
#include <pablo/branch.h>
#include <pablo/codegenstate.h>
#include <pablo/pablo_kernel.h>
#include <pablo/pe_advance.h>
#include <pablo/pe_count.h>
#include <pablo/pe_infile.h>
#include <pablo/pe_integer.h>
#include <pablo/pablo_intrinsic.h>
#include <pablo/pe_lookahead.h>
#include <pablo/pe_matchstar.h>
#include <pablo/pe_ones.h>
#include <pablo/pe_pack.h>
#include <pablo/pe_repeat.h>
#include <pablo/pe_scanthru.h>
#include <pablo/pe_string.h>
#include <pablo/pe_var.h>
#include <pablo/pe_zeroes.h>
#include <pablo/ps_assign.h>
#include <pablo/ps_terminate.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>  // for get getSequentialElementType
#include <llvm/Support/raw_os_ostream.h>

using namespace pablo;
using namespace llvm;
using TypeId = PabloAST::ClassTypeId;

const unsigned BlockIndenting = 2;

void PabloPrinter::print(const PabloKernel * kernel, raw_ostream & out) {
    out << kernel->getName() << "\n\n";
    print(kernel->getEntryScope(), out, true);
}

void PabloPrinter::print(const Statement * stmt, raw_ostream & out, const bool expandNested, const unsigned indent) {
    out.indent(indent);
    if (stmt == nullptr) {
        out << "<null-stmt>";
    } else if (const Assign * assign = dyn_cast<Assign>(stmt)) {
        print(assign->getVariable(), out);
        out << " = ";
        print(assign->getValue(), out);
    } else if (const Branch * br = dyn_cast<Branch>(stmt)) {
        if (isa<If>(br)) {
            out << "If ";
        } else if (isa<While>(br)) {
            out << "While ";
        }
        print(br->getCondition(), out);
        if (expandNested) {
            out << ":\n";
            print(br->getBody(), out, true, indent + BlockIndenting);
        }
    } else {

        print(cast<PabloAST>(stmt), out);

        if (const And * andNode = dyn_cast<And>(stmt)) {
            out << " = (";
            for (unsigned i = 0; i != andNode->getNumOperands(); ++i) {
                if (i) out << " & ";
                print(andNode->getOperand(i), out);
            }
            out << ")";
        } else if (const Or * orNode = dyn_cast<Or>(stmt)) {
            out << " = (";
            for (unsigned i = 0; i != orNode->getNumOperands(); ++i) {
                if (i) out << " | ";
                print(orNode->getOperand(i), out);
            }
            out << ")";
        } else if (const Xor * xorNode = dyn_cast<Xor>(stmt)) {
            out << " = (";
            for (unsigned i = 0; i != xorNode->getNumOperands(); ++i) {
                if (i) out << " ^ ";
                print(xorNode->getOperand(i), out);
            }
            out << ")";
        } else if (const Sel * selNode = dyn_cast<Sel>(stmt)) {
            out << " = (";
            print(selNode->getCondition(), out);
            out << " ? ";
            print(selNode->getTrueExpr(), out);
            out << " : ";
            print(selNode->getFalseExpr(), out);
            out << ")";
        } else if (const Not * notNode = dyn_cast<Not>(stmt)) {
            out << " = (~";
            print(notNode->getExpr(), out);
            out << ")";
        } else if (const Advance * adv = dyn_cast<Advance>(stmt)) {
            out << " = pablo.Advance(";
            print(adv->getExpression(), out);
            out << ", " << std::to_string(adv->getAmount()) << ")";
        } else if (const IndexedAdvance * adv = dyn_cast<IndexedAdvance>(stmt)) {
            out << " = pablo.IndexedAdvance(";
            print(adv->getExpression(), out);
            out << ", ";
            print(adv->getIndex(), out);
            out << ", " << std::to_string(adv->getAmount()) << ")";
        } else if (const Lookahead * adv = dyn_cast<Lookahead>(stmt)) {
            out << " = pablo.Lookahead(";
            print(adv->getExpression(), out);
            out << ", " << std::to_string(adv->getAmount()) << ")";
        } else if (const MatchStar * mstar = dyn_cast<MatchStar>(stmt)) {
            out << " = pablo.MatchStar(";
            print(mstar->getMarker(), out);
            out << ", ";
            print(mstar->getCharClass(), out);
            out << ")";
        } else if (const ScanThru * sthru = dyn_cast<ScanThru>(stmt)) {
            out << " = pablo.ScanThru(";
            print(sthru->getScanFrom(), out);
            out << ", ";
            print(sthru->getScanThru(), out);
            out << ")";
        } else if (const ScanTo * sto = dyn_cast<ScanTo>(stmt)) {
            out << " = pablo.ScanTo(";
            print(sto->getScanFrom(), out);
            out << ", ";
            print(sto->getScanTo(), out);
            out << ")";
        } else if (const AdvanceThenScanThru * sthru = dyn_cast<AdvanceThenScanThru>(stmt)) {
            out << " = pablo.AdvanceThenScanThru(";
            print(sthru->getScanFrom(), out);
            out << ", ";
            print(sthru->getScanThru(), out);
            out << ")";
        } else if (const AdvanceThenScanTo * sto = dyn_cast<AdvanceThenScanTo>(stmt)) {
            out << " = pablo.AdvanceThenScanTo(";
            print(sto->getScanFrom(), out);
            out << ", ";
            print(sto->getScanTo(), out);
            out << ")";
        } else if (const Ternary * tern = dyn_cast<Ternary>(stmt)) {
            out << " = pablo.Ternary(";
            out.write_hex(tern->getMask()->value());
            out << ", ";
            print(tern->getA(), out);
            out << ", ";
            print(tern->getB(), out);
            out << ", ";
            print(tern->getC(), out);
            out << ")";
        } else if (const Count * count = dyn_cast<Count>(stmt)) {
            out << " = pablo.Count(";
            print(count->getExpr(), out);
            out << ")";
        } else if (const Repeat * splat = dyn_cast<Repeat>(stmt)) {
            out << " = pablo.Repeat(";
            print(splat->getFieldWidth(), out);
            out << ", ";
            auto fw = splat->getValue()->getType()->getIntegerBitWidth();
            out << "Int" << fw << "(";
            print(splat->getValue(), out);
            out << "))";
        } else if (const PackH * p = dyn_cast<PackH>(stmt)) {
            out << " = PackH(";
            print(p->getFieldWidth(), out);
            out << ", ";
            print(p->getValue(), out);
            out << ")";
        } else if (const PackL * p = dyn_cast<PackL>(stmt)) {
            out << " = PackL(";
            print(p->getFieldWidth(), out);
            out << ", ";
            print(p->getValue(), out);
            out << ")";
        } else if (const InFile * e = dyn_cast<InFile>(stmt)) {
            out << " = pablo.InFile(";
            print(e->getExpr(), out);
            out << ")";
        } else if (const AtEOF * e = dyn_cast<AtEOF>(stmt)) {
            out << " = pablo.AtEOF(";
            print(e->getExpr(), out);
            out << ")";
        } else if (const IntrinsicCall * intr = dyn_cast<IntrinsicCall>(stmt)) {
            out << " = <Intrinsic>{";
            for (unsigned i = 0; i < intr->getNumOperands(); ++i) {
                if (i) out << ", ";
                print(intr->getOperand(i), out);
            }
            out << "}";
        } else if (const TerminateAt * s = dyn_cast<TerminateAt>(stmt)) {
            out << " = pablo.TerminateAt(";
            print(s->getExpr(), out);
            out << ", " << std::to_string(s->getSignalCode()) << ")";
        } else {
            out << "???";
        }
    }
}

void PabloPrinter::print(const PabloAST * expr, llvm::raw_ostream & out) {
    if (expr == nullptr) {
        out << "<null-expr>";
    } else if (isa<Integer>(expr)) {
        out << cast<Integer>(expr)->value();
    } else if (isa<Zeroes>(expr)) {
        out << "0";
    } else if (isa<Ones>(expr)) {
        out << "1";
    } else if (const Extract * extract = dyn_cast<Extract>(expr)) {
        print(extract->getArray(), out);
        out << "[";
        print(extract->getIndex(), out);
        out << "]";
    } else if (const Var * var = dyn_cast<Var>(expr)) {
        out << var->getName();
    } else if (const If * ifstmt = dyn_cast<If>(expr)) {
        out << "If ";
        print(ifstmt->getCondition(), out);
    } else if (const While * whl = dyn_cast<While>(expr)) {
        out << "While ";
        print(whl->getCondition(), out);
    } else if (const Assign * assign = dyn_cast<Assign>(expr)) {
        print(assign->getVariable(), out);
        out << " = ";
        print(assign->getValue(), out);
    } else if (const Add * op = dyn_cast<Add>(expr)) {
        print(op->getLH(), out);
        out << " + ";
        print(op->getRH(), out);
    } else if (const Subtract * op = dyn_cast<Subtract>(expr)) {
        print(op->getLH(), out);
        out << " - ";
        print(op->getRH(), out);
    } else if (const LessThan * op = dyn_cast<LessThan>(expr)) {
        print(op->getLH(), out);
        out << " < ";
        print(op->getRH(), out);
    } else if (const LessThanEquals * op = dyn_cast<LessThanEquals>(expr)) {
        print(op->getLH(), out);
        out << " <= ";
        print(op->getRH(), out);
    } else if (const Equals * op = dyn_cast<Equals>(expr)) {
        print(op->getLH(), out);
        out << " == ";
        print(op->getRH(), out);
    } else if (const GreaterThanEquals * op = dyn_cast<GreaterThanEquals>(expr)) {
        print(op->getLH(), out);
        out << " >= ";
        print(op->getRH(), out);
    } else if (const GreaterThan * op = dyn_cast<GreaterThan>(expr)) {
        print(op->getLH(), out);
        out << " > ";
        print(op->getRH(), out);
    } else if (const NotEquals * op = dyn_cast<NotEquals>(expr)) {
        print(op->getLH(), out);
        out << " != ";
        print(op->getRH(), out);
    } else if (const Statement * stmt = dyn_cast<Statement>(expr)) {
        assert (stmt->getParent());
        out << stmt->getName();
    } else if (LLVM_UNLIKELY(isa<PabloKernel>(expr))) {
        print(cast<PabloKernel>(expr), out);
    } else {
        out << "???";
    }
}

void PabloPrinter::print(const PabloBlock * block, raw_ostream & strm, const bool expandNested, const unsigned indent) {
    for (const Statement * stmt : *block) {
        assert (stmt->getParent() == block);
        print(stmt, strm, expandNested, indent);
        if (LLVM_LIKELY(!isa<Branch>(stmt) || !expandNested)) {
            strm << "\n";
        }
    }
}

