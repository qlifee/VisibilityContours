cmake_minimum_required(VERSION 3.16)

project(VisibilityContours VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(STELLARIUM_SOURCE_DIR "" CACHE PATH
    "Path to the Stellarium 26.2 source tree")
set(STELLARIUM_BUILD_DIR "" CACHE PATH
    "Path to the configured Stellarium 26.2 build tree")
set(STELLARIUM_IMPORT_LIBRARY "" CACHE FILEPATH
    "Windows import library for the official Stellarium 26.2 executable or stelMain DLL")

if(NOT EXISTS "${STELLARIUM_SOURCE_DIR}/src/core/StelPluginInterface.hpp")
    message(FATAL_ERROR
        "STELLARIUM_SOURCE_DIR must point to a Stellarium 26.2 source tree")
endif()
if(NOT EXISTS "${STELLARIUM_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR
        "STELLARIUM_BUILD_DIR must point to a configured Stellarium 26.2 build tree")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL LinguistTools)

add_library(VisibilityContours MODULE
    src/CrescentNavigatorDialog.cpp
    src/CrescentNavigatorDialog.hpp
    src/CrescentNavigatorDialog.ui
    src/VisibilityContours.cpp
    src/VisibilityContours.hpp
    src/VisibilityContoursDialog.cpp
    src/VisibilityContoursDialog.hpp
    src/VisibilityContoursDialog.ui
    src/VisibilityMath.cpp
    src/VisibilityMath.hpp
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
    Qt6::Widgets
    Qt6::OpenGL
)

qt_add_translations(VisibilityContours
    IMMEDIATE_CALL
    TS_FILES translations/VisibilityContours_ar.ts
    RESOURCE_PREFIX "/i18n"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(VisibilityMathTests
        tests/VisibilityMathTests.cpp
        src/VisibilityMath.cpp
    )
    target_include_directories(VisibilityMathTests PRIVATE src)
    target_link_libraries(VisibilityMathTests PRIVATE Qt6::Core)
    if(MSVC)
        target_compile_options(VisibilityMathTests PRIVATE /UNDEBUG)
    else()
        target_compile_options(VisibilityMathTests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME VisibilityMathTests COMMAND VisibilityMathTests)
endif()

if(APPLE)
    target_link_options(VisibilityContours PRIVATE "-undefined" "dynamic_lookup")
    set_target_properties(VisibilityContours PROPERTIES SUFFIX ".dylib")
elseif(WIN32)
    if(NOT EXISTS "${STELLARIUM_IMPORT_LIBRARY}")
        message(FATAL_ERROR
            "Windows builds require STELLARIUM_IMPORT_LIBRARY from the exact "
            "Stellarium 26.2 Qt6/MSVC package being targeted")
    endif()
    target_link_libraries(VisibilityContours PRIVATE
        "${STELLARIUM_IMPORT_LIBRARY}"
    )
    set_target_properties(VisibilityContours PROPERTIES PREFIX "lib")
endif()

install(TARGETS VisibilityContours
    LIBRARY DESTINATION "modules/VisibilityContours"
    RUNTIME DESTINATION "modules/VisibilityContours"
)
