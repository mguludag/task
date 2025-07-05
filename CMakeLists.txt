cmake_minimum_required(VERSION 3.14)

project(
  task
  VERSION 1.0
  LANGUAGES CXX)

option(TASK_NO_INSTALL "Skip installation of task" OFF)
option(TASK_BUILD_EXAMPLES "Build examples" OFF)

# Define the library
add_library(task INTERFACE)
add_library(mgutility::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Specify the include directories for the library
target_include_directories(
  task INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                      $<INSTALL_INTERFACE:include>)

# Set the C++ standard
target_compile_features(task INTERFACE cxx_std_11)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(STATUS "Compiler is Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()


if(CMAKE_SYSTEM_NAME STREQUAL Linux)
  include(GNUInstallDirs)
  set(include_install_dir ${CMAKE_INSTALL_INCLUDEDIR})
  set(config_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
else()
  set(include_install_dir "include")
  set(config_install_dir "lib/cmake/${PROJECT_NAME}")
endif()

# Create the package configuration files
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/taskConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/cmake/taskConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/taskConfig.cmake"
  INSTALL_DESTINATION lib/cmake/task)

if(TASK_BUILD_EXAMPLES)
  message(STATUS "Building examples")
  add_subdirectory(examples)
else()
  message(STATUS "Skipping examples build")
endif(TASK_BUILD_EXAMPLES)


if(NOT ${task_NO_INSTALL})
  # Install the library
  install(
    TARGETS task
    EXPORT taskTargets
    INCLUDES
    DESTINATION include)

  install(
    EXPORT taskTargets
    FILE taskTargets.cmake
    NAMESPACE mgutility::
    DESTINATION lib/cmake/task)

  install(DIRECTORY include/ DESTINATION include)

  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/taskConfig.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/taskConfigVersion.cmake"
          DESTINATION lib/cmake/task)

  export(
    EXPORT taskTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/taskTargets.cmake"
    NAMESPACE mgutility::)
endif()
