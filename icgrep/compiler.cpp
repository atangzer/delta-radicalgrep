/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <compiler.h>
#include <re/re_nullable.h>
#include <re/re_simplifier.h>
#include <re/parsefailure.h>
#include <re/re_parser.h>
#include <re/re_compiler.h>
#include "hrtime.h"
#include <utf8_encoder.h>
#include <cc/cc_compiler.h>
#include <cc/cc_namemap.hpp>
#include <pablo/pablo_compiler.h>

//#define DEBUG_PRINT_RE_AST
//#define DEBUG_PRINT_PBIX_AST

#ifdef DEBUG_PRINT_RE_AST
#include <re/printer_re.h>
#endif
#ifdef DEBUG_PRINT_PBIX_AST
#include <pablo/printer_pablos.h>
#endif

using namespace re;
using namespace cc;
using namespace pablo;

namespace icgrep {

LLVM_Gen_RetVal compile(const Encoding encoding, const std::string input_string, const bool show_compile_time) {
    RE * re_ast = nullptr;
    try
    {
        re_ast = RE_Parser::parse(input_string);
    }
    catch (ParseFailure failure)
    {
        std::cerr << "REGEX PARSING FAILURE: " << failure.what() << std::endl;
        std::cerr << input_string << std::endl;
        exit(1);
    }

    #ifdef DEBUG_PRINT_RE_AST
    //Print to the terminal the AST that was generated by the parser before adding the UTF encoding:
    std::cerr << "Parser:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
    #endif

    //Optimization passes to simplify the AST.
    re_ast = RE_Nullable::removeNullablePrefix(re_ast);
    #ifdef DEBUG_PRINT_RE_AST
    std::cerr << "RemoveNullablePrefix:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
    #endif

    re_ast = RE_Nullable::removeNullableSuffix(re_ast);
    #ifdef DEBUG_PRINT_RE_AST
    std::cerr << "RemoveNullableSuffix:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
    #endif

    CC_NameMap nameMap;
    re_ast = nameMap.process(re_ast);

    #ifdef DEBUG_PRINT_RE_AST
    std::cerr << "Namer:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
    #endif

    //Add the UTF encoding.
    if (encoding.getType() == Encoding::Type::UTF_8) {
        re_ast = UTF8_Encoder::toUTF8(nameMap, re_ast);
        #ifdef DEBUG_PRINT_RE_AST
        //Print to the terminal the AST that was generated by the utf8 encoder.
        std::cerr << "UTF8-encoder:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
        #endif
    }

    // note: system is clumbersome at the moment; this needs to be done AFTER toUTF8.
    nameMap.addPredefined("LineFeed", makeCC(0x0A));

    re_ast = RE_Simplifier::simplify(re_ast);
    #ifdef DEBUG_PRINT_RE_AST
    //Print to the terminal the AST that was generated by the simplifier.
    std::cerr << "Simplifier:" << std::endl << Printer_RE::PrintRE(re_ast) << std::endl;
    #endif

    SymbolGenerator symgen;
    PabloBlock main(symgen);

    CC_Compiler cc_compiler(main, encoding);
    cc_compiler.compile(nameMap);
    #ifdef DEBUG_PRINT_PBIX_AST
    //Print to the terminal the AST that was generated by the character class compiler.
    std::cerr << "Pablo CC AST:" << std::endl << StatementPrinter::Print_CC_PabloStmts(main.expressions()) << std::endl;
    #endif

    RE_Compiler re_compiler(main, nameMap);
    re_compiler.compile(re_ast);
    #ifdef DEBUG_PRINT_PBIX_AST
    //Print to the terminal the AST that was generated by the pararallel bit-stream compiler.
    std::cerr << "Final Pablo AST:" << StatementPrinter::Print_CC_PabloStmts(main.expressions()) << ")" << std::endl;
    #endif

    PabloCompiler pablo_compiler(nameMap, cc_compiler.getBasisBitVars(), encoding.getBits());
    unsigned long long cycles = 0;
    double timer = 0;
    if (show_compile_time)
    {
        cycles = get_hrcycles();
        timer = getElapsedTime();
    }

    LLVM_Gen_RetVal retVal = pablo_compiler.compile(main);
    if (show_compile_time)
    {
        cycles = get_hrcycles() - cycles;
        timer = getElapsedTime() - timer;
        std::cout << "LLVM compile time -  cycles:       " << cycles  << std::endl;
        std::cout << "LLVM compile time -  milliseconds: " << timer << std::endl;
    }

    return retVal;
}

}
