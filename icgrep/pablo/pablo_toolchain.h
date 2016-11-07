/*
 *  Copyright (c) 2015 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#ifndef PABLO_TOOLCHAIN_H
#define PABLO_TOOLCHAIN_H

#include <pablo/function.h>
#include <llvm/Support/CommandLine.h>

namespace pablo {
    
enum PabloDebugFlags {
    PrintOptimizedREcode, PrintCompiledCCcode, PrintCompiledREcode, DumpTrace, PrintUnloweredCode
};

enum PabloCompilationFlags {
    DisableSimplification, EnableCodeMotion, 
    EnableMultiplexing, EnableLowering, EnablePreDistribution, EnablePostDistribution, EnablePrePassScheduling
};
    
const cl::OptionCategory * pablo_toolchain_flags();

bool DebugOptionIsSet(PabloDebugFlags flag);

void pablo_function_passes(PabloFunction * function);

}
#endif
