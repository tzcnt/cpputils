cmake_minimum_required(VERSION 3.14)
project(cpputils_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS "1")
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

CPMAddPackage(
  NAME googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
  DOWNLOAD_ONLY)

enable_testing()

# Variables provided by parent project
include_directories(
  ${TMC_INCLUDE_PATH}
)

include_directories("include")

# copy compile_commands.json to the build directory after build so clangd can find it
# clangd won't look in the /build/{config} directory, only in the top-level /build directory
function(make_exe target_name)
  add_executable(${target_name} ${ARGN})
  target_link_libraries(${target_name} GTest::gtest_main)
  add_custom_command(TARGET ${target_name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/build
  )
endfunction()

make_exe(
  tests
  main.cpp
  standalone_compilation.cpp
  test_pool_lazy.cpp
)
target_compile_definitions(tests PRIVATE TMC_NO_UNKNOWN_AWAITABLES)

# GCC gives weird object linkage errors with the local lambdas used in some tests
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=114267
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(tests PRIVATE -Wno-subobject-linkage)
endif()

include(GoogleTest)

gtest_discover_tests(tests)
