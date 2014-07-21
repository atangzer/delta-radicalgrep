/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#ifndef CC_COMPILER_H
#define CC_COMPILER_H

#include "ps_pablos.h"
#include "cc_codegenobject.h"
#include "utf_encoding.h"
#include "cc_compiler_helper.h"

#include <math.h>
#include <utility>
#include <iostream>
#include <sstream>
#include <string>
#include <list>

#include <cassert>
#include <stdlib.h>

//***********************************
//TODO: Just for development
//#include "printer_pablos.h"
//***********************************

#define INT2STRING(i) static_cast<std::ostringstream*>(&(std::ostringstream() << i))->str()

class CC_Compiler
{
public:
    CC_Compiler(UTF_Encoding encoding);
    std::list<PabloS*> compile(std::string basis_pattern, std::string gensym_pattern, RE* re, std::list<CC*> predefined);
private:
    void process_re(CC_CodeGenObject& cgo, RE* re);
    void process_predefined(CC_CodeGenObject& cgo, std::list<CC*> predefined);
    std::string bit_var(int n);
    PabloE* make_bitv(int n);
    PabloE* bit_pattern_expr(int pattern, int selected_bits);
    PabloE* char_test_expr(int ch);
    PabloE* make_range(int n1, int n2);
    PabloE* GE_Range(int N, int n);
    PabloE* LE_Range(int N, int n);
    PabloE* char_or_range_expr(CharSetItem charset_item);
    PabloE* charset_expr(CC* cc);
    Expression* expr2pabloe(CC_CodeGenObject& cgo, PabloE* expr);
    CC_CodeGenObject cc2pablos(CC_CodeGenObject cgo, CC* cc);

    UTF_Encoding mEncoding;
};

#endif // CC_COMPILER_H


