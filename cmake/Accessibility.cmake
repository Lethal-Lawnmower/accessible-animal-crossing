# Accessibility layer: Prism speech library plus the project's access/ sources.
#
# Prism (https://github.com/ethindp/prism, MPL-2.0) lives in the extern/prism
# submodule. It is built as its own ExternalProject because Prism's CMake sets
# global options (BUILD_SHARED_LIBS, C/C++ standard, MSVC runtime) that must
# not leak into the game build. It is built as a shared library; the DLL or
# shared object is copied next to the game executable.
#
# Included from the top-level CMakeLists.txt after the acgc target exists.

include(ExternalProject)

set(ACCESS_PRISM_SOURCE "${CMAKE_SOURCE_DIR}/extern/prism")
set(ACCESS_PRISM_INSTALL "${CMAKE_BINARY_DIR}/prism-install")

if (NOT EXISTS "${ACCESS_PRISM_SOURCE}/CMakeLists.txt")
    message(FATAL_ERROR "extern/prism is missing. Run: git submodule update --init extern/prism")
endif ()

if (WIN32)
    set(ACCESS_PRISM_IMPORT_LIB "${ACCESS_PRISM_INSTALL}/lib/prism.lib")
    set(ACCESS_PRISM_RUNTIME "${ACCESS_PRISM_INSTALL}/bin/prism.dll")
elseif (APPLE)
    set(ACCESS_PRISM_IMPORT_LIB "${ACCESS_PRISM_INSTALL}/lib/libprism.dylib")
    set(ACCESS_PRISM_RUNTIME "${ACCESS_PRISM_IMPORT_LIB}")
else ()
    set(ACCESS_PRISM_IMPORT_LIB "${ACCESS_PRISM_INSTALL}/lib/libprism.so")
    set(ACCESS_PRISM_RUNTIME "${ACCESS_PRISM_IMPORT_LIB}")
endif ()

# Prism requires C23, which CMake older than 4.1 cannot enable for MSVC. The
# game itself builds with older CMake, so the Prism sub-build may use a
# different CMake executable (tools/build-windows-msvc.ps1 finds one).
set(ACCESS_PRISM_CMAKE_COMMAND "${CMAKE_COMMAND}" CACHE FILEPATH
    "CMake executable (4.1 or newer) used to build Prism")
execute_process(COMMAND "${ACCESS_PRISM_CMAKE_COMMAND}" --version
    OUTPUT_VARIABLE _access_prism_cmake_version_text ERROR_QUIET)
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" _access_prism_cmake_version "${_access_prism_cmake_version_text}")
if (NOT _access_prism_cmake_version OR _access_prism_cmake_version VERSION_LESS 4.1)
    message(FATAL_ERROR
        "Prism needs CMake 4.1 or newer; ACCESS_PRISM_CMAKE_COMMAND is "
        "'${ACCESS_PRISM_CMAKE_COMMAND}' (version '${_access_prism_cmake_version}'). "
        "Pass -DACCESS_PRISM_CMAKE_COMMAND=<path to CMake 4> or use tools/build-windows-msvc.bat.")
endif ()
message(STATUS "Prism will build with CMake ${_access_prism_cmake_version}: ${ACCESS_PRISM_CMAKE_COMMAND}")

ExternalProject_Add(prism_external
    SOURCE_DIR "${ACCESS_PRISM_SOURCE}"
    BINARY_DIR "${CMAKE_BINARY_DIR}/prism-build"
    INSTALL_DIR "${ACCESS_PRISM_INSTALL}"
    CMAKE_COMMAND "${ACCESS_PRISM_CMAKE_COMMAND}"
    CMAKE_GENERATOR Ninja
    BUILD_COMMAND "${ACCESS_PRISM_CMAKE_COMMAND}" --build <BINARY_DIR>
    INSTALL_COMMAND "${ACCESS_PRISM_CMAKE_COMMAND}" --install <BINARY_DIR>
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DBUILD_SHARED_LIBS=ON
        -DPRISM_ENABLE_TESTS=OFF
        -DPRISM_ENABLE_DEMOS=OFF
        -DPRISM_ENABLE_GDEXTENSION=OFF
        -DPRISM_ENABLE_SHIMS=OFF
        -DPRISM_ENABLE_LINTING=OFF
    BUILD_BYPRODUCTS "${ACCESS_PRISM_IMPORT_LIB}"
    UPDATE_COMMAND ""
)

add_library(access_prism SHARED IMPORTED GLOBAL)
file(MAKE_DIRECTORY "${ACCESS_PRISM_INSTALL}/include")
set_target_properties(access_prism PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ACCESS_PRISM_INSTALL}/include"
)
if (WIN32)
    set_target_properties(access_prism PROPERTIES
        IMPORTED_IMPLIB "${ACCESS_PRISM_IMPORT_LIB}"
        IMPORTED_LOCATION "${ACCESS_PRISM_RUNTIME}"
    )
else ()
    set_target_properties(access_prism PROPERTIES
        IMPORTED_LOCATION "${ACCESS_PRISM_RUNTIME}"
    )
endif ()
add_dependencies(access_prism prism_external)

set(ACCESS_ROOT "${PC_ROOT}/src/access")
set(ACCESS_SOURCES
    ${ACCESS_ROOT}/game_text.c
    ${ACCESS_ROOT}/speech.cpp
)
target_sources(acgc PRIVATE ${ACCESS_SOURCES})
set_source_files_properties(${ACCESS_SOURCES} PROPERTIES COMPILE_OPTIONS "${PC_C_WARN_FLAGS}")
target_link_libraries(acgc PRIVATE access_prism)
add_dependencies(acgc prism_external)

add_custom_command(TARGET acgc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ACCESS_PRISM_RUNTIME}" "$<TARGET_FILE_DIR:acgc>"
    COMMENT "Copying Prism runtime next to the game"
)
