/*
 *  Copyright (c) 2015 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include "pablo_toolchain.h"
#include <pablo/pablo_kernel.h>
#include <pablo/optimizers/pablo_simplifier.hpp>
#include <pablo/optimizers/codemotionpass.h>
#include <pablo/passes/flattenassociativedfg.h>
#include <pablo/passes/flattenif.hpp>
#include <pablo/passes/factorizedfg.h>
#ifdef ENABLE_MULTIPLEXING
#include <pablo/optimizers/pablo_automultiplexing.hpp>
#include <pablo/optimizers/pablo_bddminimization.h>
#include <pablo/optimizers/booleanreassociationpass.h>
#include <pablo/optimizers/distributivepass.h>
#include <pablo/optimizers/schedulingprepass.h>
#endif
#include <pablo/analysis/pabloverifier.hpp>
#include <pablo/printer_pablos.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#ifdef PRINT_TIMING_INFORMATION
#include <hrtime.h>
#endif


using namespace llvm;

namespace pablo {

static cl::OptionCategory PabloOptions("Pablo Options", "These options control printing, generation and instrumentation of Pablo intermediate code.");

const cl::OptionCategory * pablo_toolchain_flags() {
    return &PabloOptions;
}
    
    
static cl::bits<PabloDebugFlags> 
DebugOptions(cl::values(clEnumVal(ShowPablo, "Print generated Pablo code"),
                        clEnumVal(ShowOptimizedPablo, "Print optimizeed Pablo code"),
                        clEnumVal(ShowUnloweredPablo, "Print Pablo code prior to lowering."),
                        clEnumVal(DumpTrace, "Generate dynamic traces of executed Pablo assignments."),
                        clEnumValEnd), cl::cat(PabloOptions));
    
static cl::opt<std::string> PabloOutputFilename("print-pablo-output", cl::init(""), cl::desc("output Pablo filename"), cl::cat(PabloOptions));
static cl::opt<bool> Flatten("flatten-if", cl::init(false), cl::desc("Flatten all the Ifs in the Pablo AST"), cl::cat(PabloOptions));

static cl::bits<PabloCompilationFlags> 
    PabloOptimizationsOptions(cl::values(clEnumVal(DisableSimplification, "Disable Pablo Simplification pass (not recommended)"),
                                         clEnumVal(EnableCodeMotion, "Moves statements into the innermost legal If-scope and moves invariants out of While-loops."),
#ifdef ENABLE_MULTIPLEXING
                                         clEnumVal(EnableMultiplexing, "combine Advances whose inputs are mutual exclusive into the fewest number of advances possible (expensive)."),
                                         clEnumVal(EnableLowering, "coalesce associative functions prior to optimization passes."),
                                         clEnumVal(EnablePreDistribution, "apply distribution law optimization prior to multiplexing."),
                                         clEnumVal(EnablePostDistribution, "apply distribution law optimization after multiplexing."),
                                         clEnumVal(EnablePrePassScheduling, "apply pre-pass scheduling prior to LLVM IR generation."),
#endif                                         
                            clEnumValEnd), cl::cat(PabloOptions));

bool DebugOptionIsSet(PabloDebugFlags flag) {return DebugOptions.isSet(flag);}
    


#ifdef PRINT_TIMING_INFORMATION
#define READ_CYCLE_COUNTER(name) name = read_cycle_counter();
#else
#define READ_CYCLE_COUNTER(name)
#endif

#ifdef PRINT_TIMING_INFORMATION
unsigned COUNT_STATEMENTS(const PabloKernel * const entry) {
    std::stack<const Statement *> scope;
    unsigned statements = 0;
    // Scan through and collect all the advances, scanthrus and matchstars ...
    for (const Statement * stmt = entry->getEntryBlock()->front(); ; ) {
        while ( stmt ) {
            ++statements;
            if (LLVM_UNLIKELY(isa<If>(stmt) || isa<While>(stmt))) {
                // Set the next statement to be the first statement of the inner scope and push the
                // next statement of the current statement into the scope stack.
                const PabloBlock * const nested = isa<If>(stmt) ? cast<If>(stmt)->getBody() : cast<While>(stmt)->getBody();
                scope.push(stmt->getNextNode());
                stmt = nested->front();
                assert (stmt);
                continue;
            }
            stmt = stmt->getNextNode();
        }
        if (scope.empty()) {
            break;
        }
        stmt = scope.top();
        scope.pop();
    }
    return statements;
}

unsigned COUNT_ADVANCES(const PabloKernel * const entry) {

    std::stack<const Statement *> scope;
    unsigned advances = 0;

    // Scan through and collect all the advances, scanthrus and matchstars ...
    for (const Statement * stmt = entry->getEntryBlock()->front(); ; ) {
        while ( stmt ) {
            if (isa<Advance>(stmt)) {
                ++advances;
            }
            else if (LLVM_UNLIKELY(isa<If>(stmt) || isa<While>(stmt))) {
                // Set the next statement to be the first statement of the inner scope and push the
                // next statement of the current statement into the scope stack.
                const PabloBlock * const nested = isa<If>(stmt) ? cast<If>(stmt)->getBody() : cast<While>(stmt)->getBody();
                scope.push(stmt->getNextNode());
                stmt = nested->front();
                assert (stmt);
                continue;
            }
            stmt = stmt->getNextNode();
        }
        if (scope.empty()) {
            break;
        }
        stmt = scope.top();
        scope.pop();
    }
    return advances;
}

using DistributionMap = boost::container::flat_map<unsigned, unsigned>;

DistributionMap SUMMARIZE_VARIADIC_DISTRIBUTION(const PabloKernel * const entry) {
    std::stack<const Statement *> scope;
    DistributionMap distribution;
    // Scan through and collect all the advances, scanthrus and matchstars ...
    for (const Statement * stmt = entry->getEntryBlock()->front(); ; ) {
        while ( stmt ) {
            if (isa<Variadic>(stmt)) {
                auto f = distribution.find(stmt->getNumOperands());
                if (f == distribution.end()) {
                    distribution.emplace(stmt->getNumOperands(), 1);
                } else {
                    f->second += 1;
                }
            }
            else if (LLVM_UNLIKELY(isa<If>(stmt) || isa<While>(stmt))) {
                // Set the next statement to be the first statement of the inner scope and push the
                // next statement of the current statement into the scope stack.
                const PabloBlock * const nested = isa<If>(stmt) ? cast<If>(stmt)->getBody() : cast<While>(stmt)->getBody();
                scope.push(stmt->getNextNode());
                stmt = nested->front();
                assert (stmt);
                continue;
            }
            stmt = stmt->getNextNode();
        }
        if (scope.empty()) {
            break;
        }
        stmt = scope.top();
        scope.pop();
    }
    return distribution;
}
#endif

void pablo_function_passes(PabloKernel * kernel) {

    if (DebugOptions.isSet(ShowPablo)) {
        //Print to the terminal the AST that was generated by the pararallel bit-stream compiler.
        errs() << "Initial Pablo AST:\n";
        PabloPrinter::print(kernel, errs());
    }

    #ifndef NDEBUG
    PabloVerifier::verify(kernel, "creation");
    #endif

    // Scan through the pablo code and perform DCE and CSE

#ifdef PRINT_TIMING_INFORMATION
    timestamp_t simplification_start = 0, simplification_end = 0;
    timestamp_t flattenif_start = 0, flattenif_end = 0;
    timestamp_t coalescing_start = 0, coalescing_end = 0;
    timestamp_t sinking_start = 0, sinking_end = 0;
    timestamp_t pre_distribution_start = 0, pre_distribution_end = 0;
    timestamp_t multiplexing_start = 0, multiplexing_end = 0;
    timestamp_t post_distribution_start = 0, post_distribution_end = 0;
    timestamp_t lowering_start = 0, lowering_end = 0;
    timestamp_t scheduling_start = 0, scheduling_end = 0;
    DistributionMap distribution;
    const timestamp_t optimization_start = read_cycle_counter();
#endif
    if (!PabloOptimizationsOptions.isSet(DisableSimplification)) {
        READ_CYCLE_COUNTER(simplification_start);
        Simplifier::optimize(kernel);
        READ_CYCLE_COUNTER(simplification_end);
    }
    if (Flatten){
        READ_CYCLE_COUNTER(flattenif_start);
        FlattenIf::transform(kernel);
        READ_CYCLE_COUNTER(flattenif_end);
    }
#ifdef ENABLE_MULTIPLEXING
//    if (PabloOptimizationsOptions.isSet(EnableLowering) || PabloOptimizationsOptions.isSet(EnablePreDistribution) || PabloOptimizationsOptions.isSet(EnablePostDistribution)) {
//        READ_CYCLE_COUNTER(coalescing_start);
//        CanonicalizeDFG::transform(kernel);
//        READ_CYCLE_COUNTER(coalescing_end);
//    }
    if (PabloOptimizationsOptions.isSet(EnablePreDistribution)) {
        READ_CYCLE_COUNTER(pre_distribution_start);
        BooleanReassociationPass::optimize(kernel);
        READ_CYCLE_COUNTER(pre_distribution_end);
    }
    if (PabloOptimizationsOptions.isSet(EnableMultiplexing)) {
        READ_CYCLE_COUNTER(multiplexing_start);
        MultiplexingPass::optimize(kernel);
        READ_CYCLE_COUNTER(multiplexing_end);
//        if (PabloOptimizationsOptions.isSet(EnableLowering) || PabloOptimizationsOptions.isSet(EnablePreDistribution) || PabloOptimizationsOptions.isSet(EnablePostDistribution)) {
//            CanonicalizeDFG::transform(kernel);
//        }
    }
    if (PabloOptimizationsOptions.isSet(EnablePostDistribution)) {
        READ_CYCLE_COUNTER(post_distribution_start);
        BooleanReassociationPass::optimize(kernel);
        READ_CYCLE_COUNTER(post_distribution_end);
    }
#endif
    if (PabloOptimizationsOptions.isSet(EnableCodeMotion)) {
        READ_CYCLE_COUNTER(sinking_start);
        CodeMotionPass::optimize(kernel);
        READ_CYCLE_COUNTER(sinking_end);
    }
#ifdef ENABLE_MULTIPLEXING
    if (DebugOptions.isSet(ShowUnloweredPablo)) {
        //Print to the terminal the AST that was generated by the pararallel bit-stream compiler.
        errs() << "Unlowered Pablo AST:\n";
        PabloPrinter::print(kernel, errs());
    }
    #ifdef PRINT_TIMING_INFORMATION
    distribution = SUMMARIZE_VARIADIC_DISTRIBUTION(function);
    #endif
//    if (PabloOptimizationsOptions.isSet(EnableLowering) || PabloOptimizationsOptions.isSet(EnablePreDistribution) || PabloOptimizationsOptions.isSet(EnablePostDistribution)) {
//        READ_CYCLE_COUNTER(lowering_start);
//        FactorizeDFG::transform(kernel);
//        READ_CYCLE_COUNTER(lowering_end);
//    }
//    if (PabloOptimizationsOptions.isSet(EnablePrePassScheduling)) {
//        READ_CYCLE_COUNTER(scheduling_start);
//        SchedulingPrePass::optimize(kernel);
//        READ_CYCLE_COUNTER(scheduling_end);
//    }
#endif
#ifdef PRINT_TIMING_INFORMATION
    const timestamp_t optimization_end = read_cycle_counter();
#endif
    if (DebugOptions.isSet(ShowOptimizedPablo)) {
        if (PabloOutputFilename.empty()) {
            //Print to the terminal the AST that was generated by the pararallel bit-stream compiler.
            errs() << "Final Pablo AST:\n";
            PabloPrinter::print(kernel, errs());
        } else {
            std::error_code error;
            llvm::raw_fd_ostream out(PabloOutputFilename, error, sys::fs::OpenFlags::F_None);
            PabloPrinter::print(kernel, out);
        }
    }
#ifdef PRINT_TIMING_INFORMATION
    errs() << "PABLO OPTIMIZATION TIME: " << (optimization_end - optimization_start) << "\n";
    errs() << "  SIMPLIFICATION TIME: " << (simplification_end - simplification_start) << "\n";
    errs() << "  COALESCING TIME: " << (coalescing_end - coalescing_start) << "\n";
    errs() << "  SINKING TIME: " << (sinking_end - sinking_start) << "\n";
    errs() << "  PRE-DISTRIBUTION TIME: " << (pre_distribution_end - pre_distribution_start) << "\n";
    errs() << "  MULTIPLEXING TIME: " << (multiplexing_end - multiplexing_start) << "\n";
    errs() << "  LOWERING TIME: " << (lowering_end - lowering_start) << "\n";
    errs() << "  FLATTENIF TIME: " << (flattenif_end - flattenif_start) << "\n";
    errs() << "  POST-DISTRIBUTION TIME: " << (post_distribution_end - post_distribution_start) << "\n";
    errs() << "  SCHEDULING TIME: " << (scheduling_end - scheduling_start) << "\n";
    errs() << "PABLO STATEMENTS: " << COUNT_STATEMENTS(function) << "\n";
    errs() << "PABLO ADVANCES: " << COUNT_ADVANCES(function) << "\n";
    errs() << "PRE-LOWERING VARIADIC DISTRIBUTION: ";
    bool join = false;
    for (auto dist : distribution) {
        if (join) {
            errs() << ';';
        }
        errs() << dist.first << '|' << dist.second;
        join = true;
    }
    errs() << "\n";
#endif
}

}
