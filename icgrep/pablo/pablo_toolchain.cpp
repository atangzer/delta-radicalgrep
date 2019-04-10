/*
 *  Copyright (c) 2015 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <toolchain/toolchain.h>
#include "pablo_toolchain.h"
#include <pablo/pablo_kernel.h>
#include <pablo/optimizers/pablo_simplifier.hpp>
#include <pablo/optimizers/codemotionpass.h>
#include <pablo/optimizers/distributivepass.h>
#include <pablo/optimizers/schedulingprepass.h>
#include <pablo/passes/flattenif.hpp>
#include <pablo/analysis/pabloverifier.hpp>
#include <pablo/printer_pablos.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace pablo {

static cl::OptionCategory PabloOptions("Pablo Options", "These options control printing, generation and instrumentation of Pablo intermediate code.");

const cl::OptionCategory * pablo_toolchain_flags() {
    return &PabloOptions;
}
    
    
static cl::bits<PabloDebugFlags> 
DebugOptions(cl::values(clEnumVal(VerifyPablo, "Run the Pablo verifier"),
                        clEnumVal(DumpTrace, "Generate dynamic traces of executed Pablo assignments.")
                        CL_ENUM_VAL_SENTINEL), cl::cat(PabloOptions));
    
std::string ShowPabloOption = codegen::OmittedOption;
static cl::opt<std::string, true> PabloOutputOption("ShowPablo", cl::location(ShowPabloOption), cl::ValueOptional,
                                                    cl::desc("Print generated Pablo code to stderr (by omitting =<filename>) or a file"), cl::value_desc("filename"), cl::cat(PabloOptions));
sys::fs::OpenFlags PabloOutputFileFlag = sys::fs::OpenFlags::F_None;
    
std::string ShowOptimizedPabloOption = codegen::OmittedOption;
static cl::opt<std::string, true> OptimizedPabloOutputOption("ShowOptimizedPablo", cl::location(ShowOptimizedPabloOption), cl::ValueOptional,
                                                    cl::desc("Print optimized Pablo code to stderr (by omitting =<filename>) or a file"), cl::value_desc("filename"), cl::cat(PabloOptions));
sys::fs::OpenFlags PabloOptimizedOutputFileFlag = sys::fs::OpenFlags::F_None;

static cl::bits<PabloCompilationFlags> 
    PabloOptimizationsOptions(cl::values(clEnumVal(Flatten, "Flatten all the Ifs in the Pablo AST"),
                                         clEnumVal(DisableSimplification, "Disable Pablo Simplification pass (not recommended)"),
                                         clEnumVal(DisableCodeMotion, "Moves statements into the innermost legal If-scope and moves invariants out of While-loops."),
                                         clEnumVal(EnableDistribution, "Apply distribution law optimization."),                                         
                                         clEnumVal(EnableSchedulingPrePass, "Pablo Statement Scheduling Pre-Pass"),
                                         clEnumVal(EnableProfiling, "Profile branch statistics."),
                                         clEnumVal(EnableTernaryOpt, "Enable ternary optimization.")
                                         CL_ENUM_VAL_SENTINEL), cl::cat(PabloOptions));

bool DebugOptionIsSet(const PabloDebugFlags flag) {return DebugOptions.isSet(flag);}
    
bool CompileOptionIsSet(const PabloCompilationFlags flag) {return PabloOptimizationsOptions.isSet(flag);}
    


void pablo_function_passes(PabloKernel * kernel) {

    if (ShowPabloOption != codegen::OmittedOption) {
        //Print to the terminal the AST that was generated by the pararallel bit-stream compiler.
        if (ShowPabloOption.empty()) {
            errs() << "Initial Pablo AST:\n";
            PabloPrinter::print(kernel, errs());
        } else {
            std::error_code error;
            llvm::raw_fd_ostream out(ShowPabloOption, error, PabloOutputFileFlag);
            PabloPrinter::print(kernel, out);
            PabloOutputFileFlag = sys::fs::OpenFlags::F_Append;   // append subsequent Pablo kernels
        }
    }

#ifdef NDEBUG
    if (DebugOptions.isSet(VerifyPablo)) {
#endif
        PabloVerifier::verify(kernel, "creation");
#ifdef NDEBUG
    }
#endif

    // Scan through the pablo code and perform DCE and CSE
    if (Flatten){
        FlattenIf::transform(kernel);
    }
    if (LLVM_LIKELY(!PabloOptimizationsOptions.isSet(DisableSimplification))) {
        Simplifier::optimize(kernel);
    }
    if (PabloOptimizationsOptions.isSet(EnableDistribution)) {
        DistributivePass::optimize(kernel);
    }
    if (LLVM_LIKELY(!PabloOptimizationsOptions.isSet(DisableCodeMotion))) {
        CodeMotionPass::optimize(kernel);
    }
    if (PabloOptimizationsOptions.isSet(EnableSchedulingPrePass)) {
        SchedulingPrePass::optimize(kernel);
    }
    if (ShowOptimizedPabloOption != codegen::OmittedOption) {
        if (ShowOptimizedPabloOption.empty()) {
            //Print to the terminal the final Pablo AST after optimization.
            errs() << "Final Pablo AST:\n";
            PabloPrinter::print(kernel, errs());
        } else {
            std::error_code error;
            llvm::raw_fd_ostream out(ShowOptimizedPabloOption, error, PabloOptimizedOutputFileFlag);
            PabloPrinter::print(kernel, out);
            PabloOptimizedOutputFileFlag = sys::fs::OpenFlags::F_Append;  // append subsequent Pablo kernels
        }
    }
}

}
