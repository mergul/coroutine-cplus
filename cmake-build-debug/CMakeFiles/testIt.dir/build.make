# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.17

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

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
CMAKE_COMMAND = /home/mesut/.local/share/JetBrains/Toolbox/apps/CLion/ch-0/203.7148.70/bin/cmake/linux/bin/cmake

# The command to remove a file.
RM = /home/mesut/.local/share/JetBrains/Toolbox/apps/CLion/ch-0/203.7148.70/bin/cmake/linux/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/mesut/CLionProjects/testIt

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/mesut/CLionProjects/testIt/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/testIt.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/testIt.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/testIt.dir/flags.make

CMakeFiles/testIt.dir/main.cpp.o: CMakeFiles/testIt.dir/flags.make
CMakeFiles/testIt.dir/main.cpp.o: ../main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/mesut/CLionProjects/testIt/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/testIt.dir/main.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/testIt.dir/main.cpp.o -c /home/mesut/CLionProjects/testIt/main.cpp

CMakeFiles/testIt.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/testIt.dir/main.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/mesut/CLionProjects/testIt/main.cpp > CMakeFiles/testIt.dir/main.cpp.i

CMakeFiles/testIt.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/testIt.dir/main.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/mesut/CLionProjects/testIt/main.cpp -o CMakeFiles/testIt.dir/main.cpp.s

# Object files for target testIt
testIt_OBJECTS = \
"CMakeFiles/testIt.dir/main.cpp.o"

# External object files for target testIt
testIt_EXTERNAL_OBJECTS =

testIt: CMakeFiles/testIt.dir/main.cpp.o
testIt: CMakeFiles/testIt.dir/build.make
testIt: CMakeFiles/testIt.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/mesut/CLionProjects/testIt/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable testIt"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/testIt.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/testIt.dir/build: testIt

.PHONY : CMakeFiles/testIt.dir/build

CMakeFiles/testIt.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/testIt.dir/cmake_clean.cmake
.PHONY : CMakeFiles/testIt.dir/clean

CMakeFiles/testIt.dir/depend:
	cd /home/mesut/CLionProjects/testIt/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/mesut/CLionProjects/testIt /home/mesut/CLionProjects/testIt /home/mesut/CLionProjects/testIt/cmake-build-debug /home/mesut/CLionProjects/testIt/cmake-build-debug /home/mesut/CLionProjects/testIt/cmake-build-debug/CMakeFiles/testIt.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/testIt.dir/depend

