cmake_minimum_required(VERSION 3.21)
project(InstallValla)

include(ExternalProject)

message(STATUS "Dependency \"valla\"...")

# Attempt to find the installed valla library
find_package(valla QUIET PATHS ${CMAKE_INSTALL_PREFIX} NO_DEFAULT_PATH)

if (NOT valla_FOUND)
    list(APPEND CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
            -DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DTBB_TEST=OFF
    )

    message(STATUS "Preparing external project \"valla\" with args:")
    foreach(CMAKE_ARG ${CMAKE_ARGS})
        message(STATUS "-- ${CMAKE_ARG}")
    endforeach()
    ExternalProject_Add(
        valla
        GIT_REPOSITORY https://github.com/drexlerd/tree-compression.git
        GIT_TAG 6a5ae8e2ce59edf9ce1fe015099e3cb24f249bdd
        PREFIX ${CMAKE_BINARY_DIR}/valla
        DEPENDS fmt
        CMAKE_ARGS ${CMAKE_ARGS}
    )
else()
    message(STATUS "valla is already installed.")
endif()