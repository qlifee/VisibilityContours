cmake_minimum_required(VERSION 3.16)

project(VisibilityContours VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(STELLARIUM_SOURCE_DIR "" CACHE PATH
    "Path to the Stellarium 26.2 source tree")
set(STELLARIUM_BUILD_DIR "" CACHE PATH
    "Path to the configured Stellarium 26.2 build tree")

if(NOT EXISTS "${STELLARIUM_SOURCE_DIR}/src/core/StelPluginInterface.hpp")
    message(FATAL_ERROR
        "STELLARIUM_SOURCE_DIR must point to a Stellarium 26.2 source tree")
endif()
if(NOT EXISTS "${STELLARIUM_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR
        "STELLARIUM_BUILD_DIR must point to a configured Stellarium 26.2 build tree")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL)

add_library(VisibilityContours MODULE
    src/VisibilityContours.cpp
    src/VisibilityContours.hpp
)

target_compile_definitions(VisibilityContours PRIVATE
    VISIBILITYCONTOURS_PLUGIN_VERSION="${PROJECT_VERSION}"
    VISIBILITYCONTOURS_PLUGIN_LICENSE="GNU GPLv2 or later"
)

target_include_directories(VisibilityContours PRIVATE
    "${STELLARIUM_BUILD_DIR}"
    "${STELLARIUM_BUILD_DIR}/src"
    "${STELLARIUM_SOURCE_DIR}/src"
    "${STELLARIUM_SOURCE_DIR}/src/core"
    "${STELLARIUM_SOURCE_DIR}/src/core/modules"
    "${STELLARIUM_SOURCE_DIR}/src/core/planetsephems"
    "${STELLARIUM_SOURCE_DIR}/src/gui"
    "${STELLARIUM_SOURCE_DIR}/src/scripting"
)

target_link_libraries(VisibilityContours PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::OpenGL
)

if(APPLE)
    target_link_options(VisibilityContours PRIVATE "-undefined" "dynamic_lookup")
    set_target_properties(VisibilityContours PROPERTIES SUFFIX ".dylib")
elseif(WIN32)
    message(FATAL_ERROR
        "A Stellarium 26.2 import library is required for Windows builds")
endif()

install(TARGETS VisibilityContours
    LIBRARY DESTINATION "modules/VisibilityContours"
    RUNTIME DESTINATION "modules/VisibilityContours"
)
