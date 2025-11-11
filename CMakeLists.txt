cmake_minimum_required(VERSION 3.16)
project(cpputils)

set(CMAKE_MODULE_PATH
    ${cpputils_SOURCE_DIR}/cmake
    ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(CMAKE_CXX_STANDARD 20)

include(cmake/CPM.cmake)

include_directories("include")


CPMAddPackage(
    NAME TooManyCooks
    GIT_REPOSITORY https://github.com/tzcnt/TooManyCooks.git
    GIT_TAG v1.1.0
    DOWNLOAD_ONLY)

# provided for the tests subproject
set(TMC_INCLUDE_PATH ${TooManyCooks_SOURCE_DIR}/include CACHE STRING "" FORCE)
include_directories(
    ${TMC_INCLUDE_PATH}
)

# Optional flags can be passed at the command line. Used by GitHub Actions workflow.
if(CMD_COMPILE_FLAGS)
    message(STATUS "CMD_COMPILE_FLAGS: ${CMD_COMPILE_FLAGS}")
    add_compile_options(${CMD_COMPILE_FLAGS})
endif()

if(CMD_LINK_FLAGS)
    message(STATUS "CMD_LINK_FLAGS: ${CMD_LINK_FLAGS}")
    add_link_options(${CMD_LINK_FLAGS})
endif()

# Safety / warnings checks

# add_definitions("-fsanitize=address")
# add_link_options("-fsanitize=address")
# add_definitions("-fsanitize=thread")
# add_link_options("-fsanitize=thread")
# add_definitions("-fsanitize=undefined -fno-sanitize-recover")
# add_link_options("-fsanitize=undefined")

# copy compile_commands.json to the build directory after build so clangd can find it
# clangd won't look in the /build/{config} directory, only in the top-level /build directory
function(make_exe target_name)
    add_executable(${target_name} ${ARGN})
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_LIST_DIR}/build
    )
endfunction()

make_exe(bitmap_object_pool_64
    examples/bitmap_object_pool_64.cpp
)

make_exe(bitmap_object_pool
    examples/bitmap_object_pool.cpp
)

add_subdirectory(tests)
