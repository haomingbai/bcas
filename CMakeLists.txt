cmake_minimum_required(VERSION 3.25)
project(bcas VERSION 0.1.0 LANGUAGES CXX)

if(POLICY CMP0167)
  if(APPLE)
    cmake_policy(SET CMP0167 OLD)
  else()
    cmake_policy(SET CMP0167 NEW)
  endif()
endif()

if(NOT DEFINED BUILD_SHARED_LIBS)
  if(WIN32)
    option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
  else()
    option(BUILD_SHARED_LIBS "Build shared libraries" ON)
  endif()
endif()

if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(bcas_apply_project_warnings target_name)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /W4)
  else()
    target_compile_options(${target_name} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Werror
    )
  endif()
endfunction()

option(BCAS_BUILD_EXAMPLES "Build bcas examples. Default: ON" ON)
option(BCAS_BUILD_TESTS "Build bcas tests. Default: OFF" OFF)
option(BCAS_FETCH_BSRVCORE
  "Fetch bsrvcore when no existing target, package, or source tree is provided."
  ON
)
option(BCAS_FETCH_PUGIXML
  "Fetch pugixml when no existing target or source tree is provided."
  ON
)

set(BCAS_BSRVCORE_CMAKE_DIR "" CACHE PATH
  "Directory containing bsrvcoreConfig.cmake for a local bsrvcore build or install."
)
set(BCAS_BSRVCORE_SOURCE_DIR "" CACHE PATH
  "Path to a local bsrvcore source tree to add as a subdirectory."
)
set(BCAS_BSRVCORE_GIT_REPOSITORY "https://github.com/haomingbai/bsrvcore.git"
  CACHE STRING "Git repository used when bcas fetches bsrvcore."
)
set(BCAS_BSRVCORE_GIT_TAG "v0.14.0"
  CACHE STRING "Git ref used when bcas fetches bsrvcore."
)

set(BCAS_PUGIXML_SOURCE_DIR "" CACHE PATH
  "Path to a local pugixml source tree to add as a subdirectory."
)
set(BCAS_PUGIXML_GIT_REPOSITORY "https://github.com/zeux/pugixml.git"
  CACHE STRING "Git repository used when bcas fetches pugixml."
)
set(BCAS_PUGIXML_GIT_TAG "v1.15"
  CACHE STRING "Git ref used when bcas fetches pugixml."
)

set(BCAS_BSRVCORE_RESOLUTION_MODE "preexisting")
set(BCAS_PUGIXML_RESOLUTION_MODE "preexisting")

if(NOT TARGET bsrvcore::bsrvcore)
  if(BCAS_BSRVCORE_CMAKE_DIR)
    find_package(bsrvcore CONFIG QUIET
      PATHS "${BCAS_BSRVCORE_CMAKE_DIR}"
      NO_DEFAULT_PATH
    )
    if(TARGET bsrvcore::bsrvcore)
      set(BCAS_BSRVCORE_RESOLUTION_MODE
          "config:${BCAS_BSRVCORE_CMAKE_DIR}")
    endif()
  endif()

  if(NOT TARGET bsrvcore::bsrvcore)
    find_package(bsrvcore CONFIG QUIET)
    if(TARGET bsrvcore::bsrvcore)
      set(BCAS_BSRVCORE_RESOLUTION_MODE "config:find_package")
    endif()
  endif()

  if(NOT TARGET bsrvcore::bsrvcore AND BCAS_BSRVCORE_SOURCE_DIR)
    if(NOT EXISTS "${BCAS_BSRVCORE_SOURCE_DIR}/CMakeLists.txt")
      message(FATAL_ERROR
        "BCAS_BSRVCORE_SOURCE_DIR does not point to a bsrvcore source tree: "
        "${BCAS_BSRVCORE_SOURCE_DIR}"
      )
    endif()

    set(BSRVCORE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BSRVCORE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(BSRVCORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BSRVCORE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

    add_subdirectory(
      "${BCAS_BSRVCORE_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/_deps/bsrvcore-build"
    )
    if(TARGET bsrvcore::bsrvcore)
      set(BCAS_BSRVCORE_RESOLUTION_MODE
          "source:${BCAS_BSRVCORE_SOURCE_DIR}")
    endif()
  endif()

  if(NOT TARGET bsrvcore::bsrvcore AND BCAS_FETCH_BSRVCORE)
    include(FetchContent)

    set(BSRVCORE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BSRVCORE_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(BSRVCORE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BSRVCORE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
      bsrvcore
      GIT_REPOSITORY "${BCAS_BSRVCORE_GIT_REPOSITORY}"
      GIT_TAG "${BCAS_BSRVCORE_GIT_TAG}"
    )
    FetchContent_MakeAvailable(bsrvcore)
    if(TARGET bsrvcore::bsrvcore)
      set(BCAS_BSRVCORE_RESOLUTION_MODE
          "fetch:${BCAS_BSRVCORE_GIT_REPOSITORY}@${BCAS_BSRVCORE_GIT_TAG}")
    endif()
  endif()
endif()

if(NOT TARGET bsrvcore::bsrvcore)
  message(FATAL_ERROR
    "Unable to resolve bsrvcore. Tried existing targets, local packages, "
    "BCAS_BSRVCORE_SOURCE_DIR, and optional FetchContent."
  )
endif()

if(NOT TARGET pugixml::static AND NOT TARGET pugixml::pugixml)
  if(BCAS_PUGIXML_SOURCE_DIR)
    if(NOT EXISTS "${BCAS_PUGIXML_SOURCE_DIR}/CMakeLists.txt")
      message(FATAL_ERROR
        "BCAS_PUGIXML_SOURCE_DIR does not point to a pugixml source tree: "
        "${BCAS_PUGIXML_SOURCE_DIR}"
      )
    endif()

    set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(PUGIXML_INSTALL OFF CACHE BOOL "" FORCE)
    set(PUGIXML_BUILD_SHARED_AND_STATIC_LIBS OFF CACHE BOOL "" FORCE)

    set(BCAS_SAVED_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    add_subdirectory(
      "${BCAS_PUGIXML_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}/_deps/pugixml-build"
    )
    set(BUILD_SHARED_LIBS "${BCAS_SAVED_BUILD_SHARED_LIBS}" CACHE BOOL "" FORCE)
    set(BCAS_PUGIXML_SOURCE_ROOT "${BCAS_PUGIXML_SOURCE_DIR}")
    if(TARGET pugixml::static OR TARGET pugixml::pugixml)
      set(BCAS_PUGIXML_RESOLUTION_MODE
          "source:${BCAS_PUGIXML_SOURCE_DIR}")
    endif()
  elseif(BCAS_FETCH_PUGIXML)
    include(FetchContent)

    set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(PUGIXML_INSTALL OFF CACHE BOOL "" FORCE)
    set(PUGIXML_BUILD_SHARED_AND_STATIC_LIBS OFF CACHE BOOL "" FORCE)

    set(BCAS_SAVED_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
      pugixml
      GIT_REPOSITORY "${BCAS_PUGIXML_GIT_REPOSITORY}"
      GIT_TAG "${BCAS_PUGIXML_GIT_TAG}"
    )
    FetchContent_MakeAvailable(pugixml)
    set(BUILD_SHARED_LIBS "${BCAS_SAVED_BUILD_SHARED_LIBS}" CACHE BOOL "" FORCE)
    set(BCAS_PUGIXML_SOURCE_ROOT "${pugixml_SOURCE_DIR}")
    if(TARGET pugixml::static OR TARGET pugixml::pugixml)
      set(BCAS_PUGIXML_RESOLUTION_MODE
          "fetch:${BCAS_PUGIXML_GIT_REPOSITORY}@${BCAS_PUGIXML_GIT_TAG}")
    endif()
  endif()
endif()

if(TARGET pugixml::static)
  set(BCAS_PUGIXML_TARGET pugixml::static)
elseif(TARGET pugixml::pugixml)
  set(BCAS_PUGIXML_TARGET pugixml::pugixml)
else()
  message(FATAL_ERROR
    "Unable to resolve pugixml. Tried existing targets, "
    "BCAS_PUGIXML_SOURCE_DIR, and optional FetchContent."
  )
endif()

message(STATUS "bcas resolved bsrvcore via ${BCAS_BSRVCORE_RESOLUTION_MODE}")
message(STATUS "bcas resolved pugixml via ${BCAS_PUGIXML_RESOLUTION_MODE}")

set(BCAS_INSTALL_CONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/bcas")

add_subdirectory(src)

if(BCAS_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples")
  add_subdirectory(examples)
endif()

if(BCAS_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
  enable_testing()
  add_subdirectory(tests)
endif()

configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/cmake/bcasConfig.cmake.in
  ${PROJECT_BINARY_DIR}/bcasConfig.cmake
  INSTALL_DESTINATION ${BCAS_INSTALL_CONFIGDIR}
)

write_basic_package_version_file(
  ${PROJECT_BINARY_DIR}/bcasConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h*"
)

install(FILES
  ${PROJECT_BINARY_DIR}/bcasConfig.cmake
  ${PROJECT_BINARY_DIR}/bcasConfigVersion.cmake
  DESTINATION ${BCAS_INSTALL_CONFIGDIR}
)

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
  set(DOXYGEN_OUTPUT_DIR "${PROJECT_BINARY_DIR}/docs")
  set(DOXYGEN_INPUT_DIRS "${PROJECT_SOURCE_DIR}/include")

  configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/Doxyfile.in
                 ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)

  add_custom_target(bcas-doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generate API docs with Doxygen"
    VERBATIM
  )
else()
  message(STATUS "Doxygen not found; skipping doc target")
endif()
