/*
 *  Copyright (c) 2014 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include "re_compiler.h"

RE_Compiler::RE_Compiler(){}

LLVM_Gen_RetVal RE_Compiler::compile(bool show_compile_time,
                                     bool ascii_only,
                                     std::string basis_pattern,
                                     std::string gensym_pattern,
                                     UTF_Encoding encoding,
                                     std::string input_string)
{

    ParseResult* parse_result = RE_Parser::parse_re(input_string);

    RE* re_ast = 0;
    if (ParseSuccess* success = dynamic_cast<ParseSuccess*>(parse_result))
    {
        re_ast = success->getRE();
    }
    else if (ParseFailure* failure = dynamic_cast<ParseFailure*>(parse_result))
    {
        std::cout << failure->getErrorMsg() << std::endl;
        exit(1);
    }
    else
    {
        std::cout << "An unexepected parser error has occured!" << std::endl;
        exit(1);
    }

    //Print to the terminal the AST that was generated by the parser before adding the UTF encoding:
    std::cout << "\nParser:\n" + Printer_RE::PrintRE(re_ast) + "\n" << std::endl;

    //Add the UTF encoding.
    if (!ascii_only)
    {
        if (encoding.getName().compare("UTF-8") == 0)
        {
            re_ast = UTF8_Encoder::toUTF8(re_ast);
        }
        else
        {
            std::cout << "Invalid encoding!" << std::endl;
            exit(1);
        }
    }

    //Print to the terminal the AST that was generated by the utf8 encoder.
    std::cout << "\nUTF8-encoder:\n" + Printer_RE::PrintRE(re_ast) + "\n" << std::endl;

    //Optimization passes to simplify the AST.
    re_ast = RE_Simplifier::simplify(RE_Nullable::removeNullableSuffix(RE_Nullable::removeNullablePrefix(re_ast)));

    //Print to the terminal the AST that was generated by the simplifier.
    std::cout << "\nSimplifier:\n" + Printer_RE::PrintRE(re_ast) + "\n" << std::endl;

    //Map all of the unique character classes in order to reduce redundancy.
    std::map<std::string, RE*> re_map;
    re_ast = RE_Reducer::reduce(re_ast, re_map);

    //Print to the terminal the AST with the reduced REs.
    std::cout << "\nReducer:\n" + Printer_RE::PrintRE(re_ast) + "\n" << std::endl;

    //Build our list of predefined characters.
    std::list<CC*> predefined_characters;
    CC* cc_lf = new CC('\n');
    std::string lf_ccname = cc_lf->getName();
    re_map.insert(make_pair(lf_ccname, cc_lf));

    CC_Compiler cc_compiler(encoding);
    std::list<PabloS*> cc_stmtsl = cc_compiler.compile(basis_pattern, gensym_pattern, re_map, predefined_characters);

    //Print to the terminal the AST that was generated by the character class compiler.
    //std::cout << "\n" << "(" << StatementPrinter::Print_CC_PabloStmts(cc_stmtsl) << ")" << "\n" << std::endl;

    Pbix_Compiler pbix_compiler(lf_ccname);
    CodeGenState cg_state = pbix_compiler.compile(re_ast);

    //Print to the terminal the AST that was generated by the pararallel bit-stream compiler.
    //std::cout << "\n" << "(" << StatementPrinter::PrintStmts(cg_state) << ")" << "\n" << std::endl;

    //Print a count of the Pablo statements and expressions that are contained in the AST from the pbix compiler.
    //std::cout << "\nPablo Statement Count: " << Pbix_Counter::Count_PabloStatements(cg_state.stmtsl) <<  "\n" << std::endl;

    LLVM_Generator irgen(basis_pattern, lf_ccname, encoding.getBits());

    unsigned long long cycles = 0;
    double timer = 0;
    if (show_compile_time)
    {
        cycles = get_hrcycles();
        timer = getElapsedTime();
    }

    LLVM_Gen_RetVal retVal = irgen.Generate_LLVMIR(cg_state, cc_stmtsl);
    if (show_compile_time)
    {
        cycles = get_hrcycles() - cycles;
        timer = getElapsedTime() - timer;
        std::cout << "LLVM compile time -  cycles:       " << cycles  << std::endl;
        std::cout << "LLVM compile time -  milliseconds: " << timer << std::endl;
    }

    return  retVal;  //irgen.Generate_LLVMIR(cg_state, cc_stmtsl);
}

