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

set(NEUTRON_SOURCE_FILES
    linker.h    linker.cpp
	ast.h       ast.cpp
	context.h   context.cpp
	ds.h        ds.cpp
	error.h     error.cpp
	            parser.cpp
                job.cpp
    number.h    number.cpp
    default_casts.h default_casts.cpp
	cmdargs.h   cmdargs.cpp
	typer.h     typer.cpp
    backend/tir/tir.h   backend/tir/tir.cpp
	backend/llvm/llvm.h backend/llvm/llvm.cpp
	util.h              util_common.cpp
	${CMAKE_SOURCE_DIR}/keywords.gperf.gen.h)

if(WIN32)
	set(NEUTRON_PLATFORM_SPECIFIC_FILES
		util_windows.cpp)
else()
	set(NEUTRON_PLATFORM_SPECIFIC_FILES
		util_posix.cpp)
endif()

add_executable(neutron
	main.cpp ${NEUTRON_SOURCE_FILES} ${NEUTRON_PLATFORM_SPECIFIC_FILES})

add_executable(test
	test.cpp ${NEUTRON_SOURCE_FILES} ${NEUTRON_PLATFORM_SPECIFIC_FILES})

# Link against the thing that finds Visual Studio on Windows hosts
if(WIN32)
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		target_link_libraries(neutron
			${CMAKE_SOURCE_DIR}/support/Microsoft.VisualStudio.Setup.Configuration.Native/v141/x64/Microsoft.VisualStudio.Setup.Configuration.Native.lib)
	else()
		target_link_libraries(neutron 
			${CMAKE_SOURCE_DIR}/support/Microsoft.VisualStudio.Setup.Configuration.Native/v141/x86/Microsoft.VisualStudio.Setup.Configuration.Native.lib)
	endif()
	target_include_directories(neutron PUBLIC
		${CMAKE_SOURCE_DIR}/support/Microsoft.VisualStudio.Setup.Configuration.Native/include)
endif()

# TODO this is ridiculous
# 
# MSVC requires C++20 for designated initializers which are used all around the codebase
# This is fine because they are in fact a C++20 feature, 
# but the some LLVM headers don't compile under C++20 on GCC, and GCC supports designated initializers
# as an extension even in older standards, so for compilers other than MSVC we set the standard to C++14
if(MSVC)
	set_property(TARGET neutron PROPERTY CXX_STANDARD 20)
	set_property(TARGET test    PROPERTY CXX_STANDARD 20)
else()
	set_property(TARGET neutron PROPERTY CXX_STANDARD 14)
	set_property(TARGET test    PROPERTY CXX_STANDARD 14)
endif()


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
target_link_libraries(test    ${LLVM_LIBS_SPLIT})


# Get the LLVM include directories
execute_process(
	COMMAND ${LLVM_CONFIG_PATH} --includedir
	OUTPUT_VARIABLE LLVM_INCLUDE
)
include_directories(${LLVM_INCLUDE})
