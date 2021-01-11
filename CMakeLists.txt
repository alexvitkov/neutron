cmake_minimum_required(VERSION 3.10)
project(neutron)


# If we have gperf installed we can regenerate the keywords.gperf.gen.h on need
find_program(HAS_GPERF "gperf")
if (HAS_GPERF)
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/keywords.gperf.gen.h
        COMMAND gperf ${CMAKE_SOURCE_DIR}/keywords.gperf --output-file=${CMAKE_SOURCE_DIR}/keywords.gperf.gen.h
        MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/keywords.gperf)
else()
    message(WARNING "gperf not found, keywords.gperf.gen.h won't be generated if you modify the keywords.gperf file")
endif()

add_executable(neutron
	main.cpp
	ast.h ast.cpp
	context.h context.cpp
	ds.h ds.cpp
	error.h error.cpp
	parser.cpp
	sourcefile.h sourcefile.cpp
	typer.h typer.cpp
    backend/tir/tir.h backend/tir/tir.cpp
	backend/llvm/llvm.h backend/llvm/llvm.cpp
	util.h util.cpp
	${CMAKE_SOURCE_DIR}/keywords.gperf.gen.h
)

set_property(
	TARGET neutron 
	PROPERTY CXX_STANDARD 20
)


# LLVM_CONFIG_PATH must be a valid executable
if (NOT LLVM_CONFIG_PATH)
	message(FATAL_ERROR "You must pass -DLLVM_CONFIG_PATH=path/to/llvm-config")
endif()

if (NOT EXISTS "${LLVM_CONFIG_PATH}")
	message(FATAL_ERROR "Invalid llvm-config path: '${LLVM_CONFIG_PATH}' does not exist")
endif()


# Get the libraries we need to link to
execute_process(
	COMMAND ${LLVM_CONFIG_PATH} --libfiles
	OUTPUT_VARIABLE LLVM_LIBS
)
separate_arguments(LLVM_LIBS_SPLIT NATIVE_COMMAND ${LLVM_LIBS})
target_link_libraries(neutron ${LLVM_LIBS_SPLIT})


# Get the LLVM include directories
execute_process(
	COMMAND ${LLVM_CONFIG_PATH} --includedir
	OUTPUT_VARIABLE LLVM_INCLUDE
)
include_directories(${LLVM_INCLUDE})
