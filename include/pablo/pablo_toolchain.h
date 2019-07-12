/*
 *  Copyright (c) 2015 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#ifndef PABLO_TOOLCHAIN_H
#define PABLO_TOOLCHAIN_H

#include <string>

namespace llvm { namespace cl { class OptionCategory; } }
namespace pablo { class PabloKernel; }

namespace pablo {

enum PabloDebugFlags {
    VerifyPablo, DumpTrace,
};
extern std::string ShowOptimizedPabloOption;
extern std::string ShowPabloOption;

enum PabloCompilationFlags {
    Flatten,
    DisableSimplification,
    DisableCodeMotion,
    EnableDistribution,
    EnableSchedulingPrePass,
    EnableProfiling,
    EnableTernaryOpt
};

enum class PabloCarryMode {
    BitBlock,
    Compressed
};
extern PabloCarryMode CarryMode;

const llvm::cl::OptionCategory * pablo_toolchain_flags();

bool DebugOptionIsSet(const PabloDebugFlags flag);

bool CompileOptionIsSet(const PabloCompilationFlags flag);

void pablo_function_passes(PabloKernel * kernel);

}
#endif
