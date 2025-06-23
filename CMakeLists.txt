cmake_minimum_required(VERSION 3.5)

set(TARGET_NAME quickjs)

# DuckDB's extension distribution supports vcpkg. As such, dependencies can be added in ./vcpkg.json and then
# used in cmake with find_package. Feel free to remove or replace with other dependencies.
# Note that it should also be removed from vcpkg.json to prevent needlessly installing it..

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)

set(EXTENSION_SOURCES src/quickjs_extension.cpp)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# Add custom target to build the quickjs library first
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/build/libqjs.a
    COMMAND make
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/quickjs
    COMMENT "Building quickjs library..."
)

add_custom_target(
    quickjs_lib ALL
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/build/libqjs.a
)

add_dependencies(${EXTENSION_NAME} quickjs_lib)
add_dependencies(${LOADABLE_EXTENSION_NAME} quickjs_lib)

# Add quickjs include directory
target_include_directories(${EXTENSION_NAME} PUBLIC quickjs)
target_include_directories(${LOADABLE_EXTENSION_NAME} PUBLIC quickjs)

# Link against quickjs
target_link_libraries(${EXTENSION_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/build/libqjs.a)
target_link_libraries(${LOADABLE_EXTENSION_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/quickjs/build/libqjs.a)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")

install(
  EXPORT "${DUCKDB_EXPORT_SET}"
  NAMESPACE "duckdb::"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/DuckDB"
  FILE "DuckDB-extension-targets.cmake"
)
