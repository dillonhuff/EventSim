cmake_minimum_required(VERSION 2.6)

project(EventSim)

SET(EXTRA_CXX_COMPILE_FLAGS "-std=c++11 -I./src -I./test -I/opt/local/include -O2 -Werror -Wall")
SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${EXTRA_CXX_COMPILE_FLAGS}")

SET(CXX_OCL_LINK_FLAGS "-lcoreir -lcoreir-rtlil -lcoreir-commonlib -L/Users/dillon/CppWorkspace/coreir/lib/")
SET( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${CXX_OCL_LINK_FLAGS}")


INCLUDE_DIRECTORIES(./src/)

SET(CPP_FILES ./src/simulator.cpp)

SET(TEST_FILES ./test/test_simulator.cpp)

add_executable(all-tests ${TEST_FILES} ${CPP_FILES})
