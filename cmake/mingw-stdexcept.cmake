# GCC PR libstdc++/125151: C++26 exception copy constructors are emitted as
# strong symbols on MinGW, colliding with the static libstdc++ definitions.
# Probe the actual toolchain so the workaround disappears with a fixed GCC.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=125151
include_guard(GLOBAL)

function(apache_check_mingw_stdexcept)
  if(NOT MINGW OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR NOT APACHE_STATIC_RUNTIME)
    return()
  endif()

  include(CheckCXXSourceCompiles)
  include(CMakePushCheckState)
  cmake_push_check_state(RESET)
  # Release builds can inline away the faulty definitions. Always probe an
  # unoptimised executable, even when configuring Release or a multi-config IDE.
  set(CMAKE_TRY_COMPILE_CONFIGURATION Debug)
  set(CMAKE_CXX_FLAGS_DEBUG "-O0")
  set(CMAKE_REQUIRED_LINK_OPTIONS -static)
  file(READ "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/stdexcept-copy.cpp" probe)
  check_cxx_source_compiles("${probe}" APACHE_MINGW_STDEXCEPT_LINKS)
  if(NOT APACHE_MINGW_STDEXCEPT_LINKS)
    set(CMAKE_REQUIRED_DEFINITIONS -D__cpp_lib_constexpr_exceptions=0)
    check_cxx_source_compiles("${probe}" APACHE_MINGW_STDEXCEPT_COMPAT_LINKS)
    if(NOT APACHE_MINGW_STDEXCEPT_COMPAT_LINKS)
      message(FATAL_ERROR "MinGW cannot link the standard exception copy probe, including the GCC PR125151 workaround. See CMakeConfigureLog.yaml.")
    endif()
    # This disables only libstdc++'s constexpr exception classes. Runtime
    # exceptions and the C++26 language mode remain enabled in every target.
    add_compile_definitions("$<$<COMPILE_LANGUAGE:CXX>:__cpp_lib_constexpr_exceptions=0>")
    message(STATUS "Working around GCC PR125151: disable libstdc++ constexpr exception classes")
  endif()
  cmake_pop_check_state()
endfunction()

apache_check_mingw_stdexcept()
