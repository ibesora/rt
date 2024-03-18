cmake_minimum_required (VERSION 3.12)

project (RAY-TRACER LANGUAGES CXX C)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT DEFINED RT_SOURCE_CODE)
  set (RT_SOURCE_CODE ${CMAKE_SOURCE_DIR})
endif ()

# Configuration options

option (RT_DBG_LOGGING "Enable (very verbose!) debug logging" OFF)


if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message (STATUS "Setting build type to 'Release' as none was specified.")
  set (CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
    "MinSizeRel" "RelWithDebInfo")
endif ()

function (CHECK_EXT NAME DIR HASH)
  if (NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/ext/${DIR}")
    message (FATAL_ERROR "The ${NAME} submodule directory is missing! "
      "Either that submodule was recently added to rt or you did not clone the project with --recursive. "
      "In order to update the submodules, run:\n"
      "  \"git submodule update --init --recursive\"")
  endif ()

  find_package(Git)
  if (GIT_FOUND)
    execute_process (
      COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/ext/${DIR}"
      RESULT_VARIABLE "git_return"
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE
      OUTPUT_VARIABLE "git_hash")
    if (NOT ${git_hash} MATCHES "^${HASH}")
      message (FATAL_ERROR "The ${CMAKE_CURRENT_SOURCE_DIR}/src/ext/${DIR} "
        "submodule isn't up to date (${git_hash} vs ${HASH}). Please run:\n"
        "  \"git submodule update --recursive\"")
    else ()
      message (STATUS "${NAME} at commit: ${git_hash}")
    endif()
  else (GIT_FOUND)
    message (STATUS "git not found: unable to verify revisions in submodules")
  endif (GIT_FOUND)
endfunction ()

add_compile_definitions ("$<$<CONFIG:DEBUG>:RT_DEBUG_BUILD>")

enable_testing ()

find_package (Threads)

find_package(OpenGL REQUIRED)

set_property (GLOBAL PROPERTY USE_FOLDERS ON)

if (RT_DBG_LOGGING)
  list (APPEND RT_DEFINITIONS "RT_DBG_LOGGING")
endif ()

########################################
# os/compiler-specific stuff

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  list (APPEND RT_DEFINITIONS "RT_IS_WINDOWS" "NOMINMAX")
elseif (CMAKE_SYSTEM_NAME STREQUAL Darwin)
  list (APPEND RT_DEFINITIONS "RT_IS_OSX")
elseif (CMAKE_SYSTEM_NAME STREQUAL Linux)
  list (APPEND RT_DEFINITIONS "RT_IS_LINUX")
  # -rdynamic so we can get backtrace symbols...
  # --no-as-needed so libprofiler sticks around
  string (APPEND CMAKE_EXE_LINKER_FLAGS " -rdynamic -Wl,--no-as-needed")
else ()
  message (SEND_ERROR "Unknown system name: " + CMAKE_SYSTEM_NAME)
endif()
if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
  add_definitions(-Wno-deprecated-declarations) # silent deprecated 'snprintf' message under MacOS arm64
endif()
# libgoogle-perftools-dev
find_library (PROFILE_LIB profiler)
if (NOT PROFILE_LIB)
  message (STATUS "Unable to find -lprofiler")
else ()
  message (STATUS "Found -lprofiler: ${PROFILE_LIB}")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION GREATER_EQUAL 14)
  message (STATUS "Disabling -ffp-contract (thanks, clang 14!)")
  list (APPEND RT_CXX_FLAGS "-ffp-contract=off")
endif ()

add_library (rt_warnings INTERFACE)
target_compile_options (
    rt_warnings
    INTERFACE
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd4244>" # int -> float conversion
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd4267>" # size_t -> int conversion
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd4305>" # double constant assigned to float
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd4552>" # result of expression not used
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd4838>" # double -> int conversion
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd4843>" # double -> float conversion
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd26451>" # arithmetic on 4-byte value, then cast to 8-byte
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd26495>" # uninitialized member variable
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd4334>" # 32 to 64 bit displacement
        "$<$<CXX_COMPILER_ID:MSVC>:$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >/wd4146>" # NanoVDB: unary minus operator applied to unsigned type, result still unsigned
)

add_library (rt_opt INTERFACE)

###########################################################################
# Annoying compiler-specific details

include (CheckCXXCompilerFlag)

check_cxx_compiler_flag ("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if (COMPILER_SUPPORTS_MARCH_NATIVE)
    target_compile_options (rt_opt INTERFACE
          "$<$<COMPILE_LANGUAGE:CUDA>:SHELL:-Xcompiler >-march=native")
endif ()

###########################################################################
# Check for various C++ features and set preprocessor variables or
# define workarounds.

include (CheckCXXSourceCompiles)

check_cxx_source_compiles ("
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
int main() {
   int fd = open(\"foo\", O_RDONLY);
   struct stat s;
   fstat(fd, &s);
   size_t len = s.st_size;
   void *ptr = mmap(0, len, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);
   munmap(ptr, len);   
}
" HAVE_MMAP)

if (HAVE_MMAP)
  list (APPEND RT_DEFINITIONS "RT_HAVE_MMAP")
ENDIF ()

include (CheckIncludeFiles)

check_cxx_source_compiles ("
#include <intrin.h>
int main() {
    unsigned long lz = 0, v = 1234;
    if (_BitScanReverse(&lz, v)) return lz;
    return 0;
} " HAS_INTRIN_H)

if (HAS_INTRIN_H)
  list (APPEND RT_DEFINITIONS "RT_HAS_INTRIN_H")
endif ()

########################################
# noinline

check_cxx_source_compiles (
"__declspec(noinline) void foo() { }
int main() { }"
HAVE_DECLSPEC_NOINLINE)

check_cxx_source_compiles (
"__attribute__((noinline)) void foo() { }
int main() { }"
HAVE_ATTRIBUTE_NOINLINE)

if (HAVE_ATTRIBUTE_NOINLINE)
  list (APPEND RT_DEFINITIONS "RT_NOINLINE=__attribute__((noinline))")
elseif (HAVE_DECLSPEC_NOINLINE)
  list (APPEND RT_DEFINITIONS "RT_NOINLINE=__declspec(noinline)")
else ()
  list (APPEND RT_DEFINITIONS "RT_NOINLINE")
endif ()

########################################
# restrict

check_cxx_source_compiles (
"int * __restrict__ ptr;
int main() { }"
HAVE_PRE_POST_BAR_RESTRICT)

check_cxx_source_compiles (
"int * __restrict ptr;
int main() { }"
HAVE_PRE_BAR_RESTRICT)

if (HAVE_PRE_POST_BAR_RESTRICT)
  list (APPEND RT_DEFINITIONS "RT_RESTRICT=__restrict__")
elseif (HAVE_PRE_BAR_RESTRICT)
  list (APPEND RT_DEFINITIONS "RT_RESTRICT=__restrict")
else ()
  list (APPEND RT_DEFINITIONS "RT_RESTRICT")
endif ()

########################################
# Aligned memory allocation

check_cxx_source_compiles ( "
#include <malloc.h>
int main() { void * ptr = _aligned_malloc(1024, 32); }
" HAVE__ALIGNED_MALLOC )

check_cxx_source_compiles ( "
#include <stdlib.h>
int main() {
  void *ptr;
  posix_memalign(&ptr, 32, 1024);
} " HAVE_POSIX_MEMALIGN )

if (HAVE__ALIGNED_MALLOC)
  list (APPEND RT_DEFINITIONS "RT_HAVE__ALIGNED_MALLOC")
elseif (HAVE_POSIX_MEMALIGN)
  list (APPEND RT_DEFINITIONS "RT_HAVE_POSIX_MEMALIGN")
else ()
  message (SEND_ERROR "Unable to find a way to allocate aligned memory")
endif ()

########################################
# are long and int64_t the same

check_cxx_source_compiles ("
#include <cstdint>
#include <type_traits>
static_assert(!std::is_same<long, int64_t>::value && !std::is_same<long long, int64_t>::value);
int main() { }
" INT64_IS_OWN_TYPE)

if (INT64_IS_OWN_TYPE)
  list (APPEND RT_DEFINITIONS "RT_INT64_IS_OWN_TYPE")
endif ()

if (PBRT_NVTX)
  add_definitions (-D NVTX)
endif()

######################
# Main renderer

add_executable (rt_exe src/rt/cmd/rt.cpp)
add_executable (rt::rt_exe ALIAS rt_exe)

target_compile_definitions (rt_exe PRIVATE ${RT_DEFINITIONS})
target_compile_options (rt_exe PRIVATE ${RT_CXX_FLAGS})
target_include_directories (rt_exe PRIVATE src src/ext)
target_link_libraries (rt_exe PRIVATE ${ALL_RT_LIBS} rt_opt rt_warnings)

set_target_properties (rt_exe PROPERTIES OUTPUT_NAME rt)

set_property (TARGET rt_exe PROPERTY FOLDER "cmd")

###############################
# Installation

install (TARGETS
  rt_exe
  DESTINATION
  bin
  )
