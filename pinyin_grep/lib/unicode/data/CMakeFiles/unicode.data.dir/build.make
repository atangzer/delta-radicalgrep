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
include lib/unicode/data/CMakeFiles/unicode.data.dir/depend.make

# Include the progress variables for this target.
include lib/unicode/data/CMakeFiles/unicode.data.dir/progress.make

# Include the compile flags for this target's objects.
include lib/unicode/data/CMakeFiles/unicode.data.dir/flags.make

lib/unicode/data/CMakeFiles/unicode.data.dir/CaseFolding.cpp.o: lib/unicode/data/CMakeFiles/unicode.data.dir/flags.make
lib/unicode/data/CMakeFiles/unicode.data.dir/CaseFolding.cpp.o: ../lib/unicode/data/CaseFolding.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/kevinq/Downloads/parabix-devel/pinyin_grep/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object lib/unicode/data/CMakeFiles/unicode.data.dir/CaseFolding.cpp.o"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && /Library/Developer/CommandLineTools/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/unicode.data.dir/CaseFolding.cpp.o -c /Users/kevinq/Downloads/parabix-devel/lib/unicode/data/CaseFolding.cpp

lib/unicode/data/CMakeFiles/unicode.data.dir/CaseFolding.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/unicode.data.dir/CaseFolding.cpp.i"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/kevinq/Downloads/parabix-devel/lib/unicode/data/CaseFolding.cpp > CMakeFiles/unicode.data.dir/CaseFolding.cpp.i

lib/unicode/data/CMakeFiles/unicode.data.dir/CaseFolding.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/unicode.data.dir/CaseFolding.cpp.s"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/kevinq/Downloads/parabix-devel/lib/unicode/data/CaseFolding.cpp -o CMakeFiles/unicode.data.dir/CaseFolding.cpp.s

lib/unicode/data/CMakeFiles/unicode.data.dir/Equivalence.cpp.o: lib/unicode/data/CMakeFiles/unicode.data.dir/flags.make
lib/unicode/data/CMakeFiles/unicode.data.dir/Equivalence.cpp.o: ../lib/unicode/data/Equivalence.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/kevinq/Downloads/parabix-devel/pinyin_grep/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object lib/unicode/data/CMakeFiles/unicode.data.dir/Equivalence.cpp.o"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && /Library/Developer/CommandLineTools/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/unicode.data.dir/Equivalence.cpp.o -c /Users/kevinq/Downloads/parabix-devel/lib/unicode/data/Equivalence.cpp

lib/unicode/data/CMakeFiles/unicode.data.dir/Equivalence.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/unicode.data.dir/Equivalence.cpp.i"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/kevinq/Downloads/parabix-devel/lib/unicode/data/Equivalence.cpp > CMakeFiles/unicode.data.dir/Equivalence.cpp.i

lib/unicode/data/CMakeFiles/unicode.data.dir/Equivalence.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/unicode.data.dir/Equivalence.cpp.s"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/kevinq/Downloads/parabix-devel/lib/unicode/data/Equivalence.cpp -o CMakeFiles/unicode.data.dir/Equivalence.cpp.s

lib/unicode/data/CMakeFiles/unicode.data.dir/PropertyObjects.cpp.o: lib/unicode/data/CMakeFiles/unicode.data.dir/flags.make
lib/unicode/data/CMakeFiles/unicode.data.dir/PropertyObjects.cpp.o: ../lib/unicode/data/PropertyObjects.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/Users/kevinq/Downloads/parabix-devel/pinyin_grep/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object lib/unicode/data/CMakeFiles/unicode.data.dir/PropertyObjects.cpp.o"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && /Library/Developer/CommandLineTools/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/unicode.data.dir/PropertyObjects.cpp.o -c /Users/kevinq/Downloads/parabix-devel/lib/unicode/data/PropertyObjects.cpp

lib/unicode/data/CMakeFiles/unicode.data.dir/PropertyObjects.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/unicode.data.dir/PropertyObjects.cpp.i"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /Users/kevinq/Downloads/parabix-devel/lib/unicode/data/PropertyObjects.cpp > CMakeFiles/unicode.data.dir/PropertyObjects.cpp.i

lib/unicode/data/CMakeFiles/unicode.data.dir/PropertyObjects.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/unicode.data.dir/PropertyObjects.cpp.s"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && /Library/Developer/CommandLineTools/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /Users/kevinq/Downloads/parabix-devel/lib/unicode/data/PropertyObjects.cpp -o CMakeFiles/unicode.data.dir/PropertyObjects.cpp.s

# Object files for target unicode.data
unicode_data_OBJECTS = \
"CMakeFiles/unicode.data.dir/CaseFolding.cpp.o" \
"CMakeFiles/unicode.data.dir/Equivalence.cpp.o" \
"CMakeFiles/unicode.data.dir/PropertyObjects.cpp.o"

# External object files for target unicode.data
unicode_data_EXTERNAL_OBJECTS =

lib/unicode/data/libparabix_unicode_data.a: lib/unicode/data/CMakeFiles/unicode.data.dir/CaseFolding.cpp.o
lib/unicode/data/libparabix_unicode_data.a: lib/unicode/data/CMakeFiles/unicode.data.dir/Equivalence.cpp.o
lib/unicode/data/libparabix_unicode_data.a: lib/unicode/data/CMakeFiles/unicode.data.dir/PropertyObjects.cpp.o
lib/unicode/data/libparabix_unicode_data.a: lib/unicode/data/CMakeFiles/unicode.data.dir/build.make
lib/unicode/data/libparabix_unicode_data.a: lib/unicode/data/CMakeFiles/unicode.data.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/Users/kevinq/Downloads/parabix-devel/pinyin_grep/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX static library libparabix_unicode_data.a"
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && $(CMAKE_COMMAND) -P CMakeFiles/unicode.data.dir/cmake_clean_target.cmake
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/unicode.data.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
lib/unicode/data/CMakeFiles/unicode.data.dir/build: lib/unicode/data/libparabix_unicode_data.a

.PHONY : lib/unicode/data/CMakeFiles/unicode.data.dir/build

lib/unicode/data/CMakeFiles/unicode.data.dir/clean:
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data && $(CMAKE_COMMAND) -P CMakeFiles/unicode.data.dir/cmake_clean.cmake
.PHONY : lib/unicode/data/CMakeFiles/unicode.data.dir/clean

lib/unicode/data/CMakeFiles/unicode.data.dir/depend:
	cd /Users/kevinq/Downloads/parabix-devel/pinyin_grep && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /Users/kevinq/Downloads/parabix-devel /Users/kevinq/Downloads/parabix-devel/lib/unicode/data /Users/kevinq/Downloads/parabix-devel/pinyin_grep /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data /Users/kevinq/Downloads/parabix-devel/pinyin_grep/lib/unicode/data/CMakeFiles/unicode.data.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : lib/unicode/data/CMakeFiles/unicode.data.dir/depend
