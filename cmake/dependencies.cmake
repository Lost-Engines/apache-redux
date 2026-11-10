find_package(OpenGL REQUIRED)
if(NOT TARGET glfw)
  include(FetchContent)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation")
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples")
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests")
  set(GLFW_INSTALL OFF CACHE BOOL "Install GLFW")
  FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG master
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(glfw)
  if(APPLE)
    # GLFW's C files include Cocoa headers with Apple's blocks extension too.
    # Match Darker: compile these sources with Apple's Objective-C compiler.
    get_target_property(glfw_sources glfw SOURCES)
    foreach(source IN LISTS glfw_sources)
      if(source MATCHES "\\.c$")
        get_filename_component(source_path "${source}" ABSOLUTE BASE_DIR "${glfw_SOURCE_DIR}/src")
        set_source_files_properties("${source_path}" DIRECTORY "${glfw_SOURCE_DIR}/src" PROPERTIES LANGUAGE OBJC)
      endif()
    endforeach()
  endif()
endif()


include(FetchContent)
find_package(Threads REQUIRED)
FetchContent_Declare(miniaudio
  URL https://codeload.github.com/mackron/miniaudio/tar.gz/f40cf03f80cdb7e741d43e53b7e706e8c1394bcf
  URL_HASH SHA256=412326cf55133404cbfb81ec8974b10149dc68732ce5bdeee8ba9cfc2695d646
  SOURCE_SUBDIR unused_upstream_build
)
FetchContent_MakeAvailable(miniaudio)
