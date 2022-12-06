file(GLOB_RECURSE worlds_libs_src ${CMAKE_CURRENT_LIST_DIR}/*.cpp ${CMAKE_CURRENT_LIST_DIR}/*.cc ${CMAKE_CURRENT_LIST_DIR}/*.c)

if(NOT WORLDS_BUILD_EDITOR)
    list(FILTER worlds_libs_src EXCLUDE REGEX ".?crnlib")
    list(APPEND worlds_libs_src ${CMAKE_CURRENT_LIST_DIR}/crnlib/crn_decomp.cpp)
endif()