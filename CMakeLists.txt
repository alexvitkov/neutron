cmake_minimum_required(VERSION 3.10)

project(neutron)

# If we don't have g

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

add_executable(neutron 
    all.cpp 
    ${CMAKE_SOURCE_DIR}/keywords.gperf.gen.h)
