cmake_minimum_required(VERSION 3.10)
project(neutron)

find_program(HAS_GPERF "gperf")

if (HAS_GPERF)
    # If we have gperf installed we can regenerate the keywords.gperf.gen.h on need
    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/keywords.gperf.gen.h
        COMMAND gperf ${CMAKE_SOURCE_DIR}/keywords.gperf --output-file=${CMAKE_SOURCE_DIR}/keywords.gperf.gen.h
        MAIN_DEPENDENCY ${CMAKE_SOURCE_DIR}/keywords.gperf)
else()
    message(WARNING "gperf not found, keywords.gperf.gen.h won't be generated if you modify the keywords.gperf file")
endif()

find_package(LLVM
    PATHS deps/llvm-11.0.0-src/build
    CONFIG)

# If we don't find LLVM under our deps tree, try to use the system-provided LLVM
if (NOT LLVM_FOUND)
    find_package(LLVM 
        REQUIRED 
        CONFIG)
    message(WARNING "Using system LLVM at ${LLVM_DIR}")
endif()

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(neutron 
    all.cpp 
    ${CMAKE_SOURCE_DIR}/keywords.gperf.gen.h)

llvm_map_components_to_libnames(llvm_libs support core irreader)

target_link_libraries(neutron ${llvm_libs})
