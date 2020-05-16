# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/local/Cellar/cmake/3.16.4/bin/cmake

# The command to remove a file.
RM = /usr/local/Cellar/cmake/3.16.4/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /Users/kevinq/Downloads/parabix-devel

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /Users/kevinq/Downloads/parabix-devel/pinyin_grep

# Include any dependencies generated for this target.
include tools/wc/CMakeFiles/wc.dir/depend.make

# Include the progress variables for this target.
include tools/wc/CMakeFiles/wc.dir/progress.make

# Include the compile flags for this target's objects.
include tools/wc/CMakeFiles/wc.dir/flags.make

tools/wc/CMakeFiles/wc.dir/wc.cpp.o: tools/wc/CMakeFiles/wc.dir/flags.make
tools/wc/CMakeFiles/wc.dir/wc.cpp.o: ../tools/wc/wc.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/kevinq/Downloads/parabix-devel/pinyin_grep/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object tools/wc/CMakeFiles/wc.dir/wc.cpp.o"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/tools/wc && /Library/Developer/CommandLineTools/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/wc.dir/wc.cpp.o -c /Users/kevinq/Downloads/parabix-devel/tools/wc/wc.cpp

tools/wc/CMakeFiles/wc.dir/wc.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/wc.dir/wc.cpp.i"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/tools/wc && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/kevinq/Downloads/parabix-devel/tools/wc/wc.cpp > CMakeFiles/wc.dir/wc.cpp.i

tools/wc/CMakeFiles/wc.dir/wc.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/wc.dir/wc.cpp.s"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/tools/wc && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/kevinq/Downloads/parabix-devel/tools/wc/wc.cpp -o CMakeFiles/wc.dir/wc.cpp.s

# Object files for target wc
wc_OBJECTS = \
"CMakeFiles/wc.dir/wc.cpp.o"

# External object files for target wc
wc_EXTERNAL_OBJECTS =

bin/wc: tools/wc/CMakeFiles/wc.dir/wc.cpp.o
bin/wc: tools/wc/CMakeFiles/wc.dir/build.make
bin/wc: lib/fileselect/libparabix_fileselect.a
bin/wc: lib/kernel/basis/libparabix_kernel_basis.a
bin/wc: lib/kernel/io/libparabix_kernel_io.a
bin/wc: lib/re/cc/libparabix_re_cc.a
bin/wc: lib/grep/libparabix_grep.a
bin/wc: lib/kernel/basis/libparabix_kernel_basis.a
bin/wc: lib/kernel/io/libparabix_kernel_io.a
bin/wc: lib/kernel/scan/libparabix_kernel_scan.a
bin/wc: lib/kernel/streamutils/libparabix_kernel_streamutils.a
bin/wc: lib/pablo/bixnum/libparabix_pablo_bixnum.a
bin/wc: lib/kernel/util/libparabix_kernel_util.a
bin/wc: lib/kernel/unicode/libparabix_kernel_unicode.a
bin/wc: lib/kernel/pipeline/libparabix_kernel_pipeline.a
bin/wc: lib/objcache/libparabix_objcache.a
bin/wc: lib/re/parse/libparabix_re_parse.a
bin/wc: lib/re/unicode/libparabix_re_unicode.a
bin/wc: lib/re/compile/libparabix_re_compile.a
bin/wc: lib/re/transforms/libparabix_re_transforms.a
bin/wc: lib/re/analysis/libparabix_re_analysis.a
bin/wc: lib/re/ucd/libparabix_re_ucd.a
bin/wc: lib/re/cc/libparabix_re_cc.a
bin/wc: lib/pablo/libparabix_pablo.a
bin/wc: lib/kernel/core/libparabix_kernel_core.a
bin/wc: lib/idisa/libparabix_idisa.a
bin/wc: lib/codegen/libparabix_codegen.a
bin/wc: /usr/local/lib/libboost_system.dylib
bin/wc: /usr/local/lib/libboost_filesystem.dylib
bin/wc: /usr/local/lib/libboost_iostreams.dylib
bin/wc: lib/re/adt/libparabix_re_adt.a
bin/wc: lib/re/alphabet/libparabix_re_alphabet.a
bin/wc: lib/re/toolchain/libparabix_re_toolchain.a
bin/wc: lib/unicode/utf/libparabix_unicode_utf.a
bin/wc: lib/unicode/data/libparabix_unicode_data.a
bin/wc: lib/unicode/core/libparabix_unicode_core.a
bin/wc: lib/toolchain/libparabix_toolchain.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMX86CodeGen.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMAsmPrinter.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMDebugInfoDWARF.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMGlobalISel.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMSelectionDAG.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMCodeGen.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMBitWriter.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMScalarOpts.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMAggressiveInstCombine.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMInstCombine.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMX86AsmParser.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMX86Desc.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMX86Disassembler.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMMCDisassembler.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMX86Info.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMX86Utils.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMMCJIT.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMExecutionEngine.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMTarget.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMRuntimeDyld.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMIRReader.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMAsmParser.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMLinker.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMTransformUtils.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMAnalysis.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMProfileData.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMObject.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMMCParser.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMMC.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMDebugInfoCodeView.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMDebugInfoMSF.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMBitReader.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMBitstreamReader.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMCore.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMBinaryFormat.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMRemarks.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMSupport.a
bin/wc: /usr/local/Cellar/llvm/9.0.1/lib/libLLVMDemangle.a
bin/wc: tools/wc/CMakeFiles/wc.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/kevinq/Downloads/parabix-devel/pinyin_grep/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable ../../bin/wc"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/tools/wc && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/wc.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tools/wc/CMakeFiles/wc.dir/build: bin/wc

.PHONY : tools/wc/CMakeFiles/wc.dir/build

tools/wc/CMakeFiles/wc.dir/clean:
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/tools/wc && $(CMAKE_COMMAND) -P CMakeFiles/wc.dir/cmake_clean.cmake
.PHONY : tools/wc/CMakeFiles/wc.dir/clean

tools/wc/CMakeFiles/wc.dir/depend:
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/kevinq/Downloads/parabix-devel /Users/kevinq/Downloads/parabix-devel/tools/wc /Users/kevinq/Downloads/parabix-devel/pinyin_grep /Users/kevinq/Downloads/parabix-devel/pinyin_grep/tools/wc /Users/kevinq/Downloads/parabix-devel/pinyin_grep/tools/wc/CMakeFiles/wc.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tools/wc/CMakeFiles/wc.dir/depend
