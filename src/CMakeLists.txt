INCLUDE_DIRECTORIES(.)
LINK_DIRECTORIES(${BUILD_DIR}/src)

SET(VisibilityContours_SRCS
    VisibilityContours.hpp
    VisibilityContours.cpp
)

SET(VisibilityContours_Qt_Libraries Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::OpenGL)

IF(BUILD_DYNAMIC_PLUGIN)
    ADD_LIBRARY(VisibilityContours MODULE ${VisibilityContours_SRCS})
    IF(APPLE)
        FIND_LIBRARY(OPENGL_LIBRARY OpenGL)
        MARK_AS_ADVANCED(OPENGL_LIBRARY)
        SET_TARGET_PROPERTIES(VisibilityContours PROPERTIES LINK_FLAGS "-undefined dynamic_lookup" SUFFIX ".dylib")
    ENDIF()
    IF(WIN32)
        SET_TARGET_PROPERTIES(VisibilityContours PROPERTIES LINK_FLAGS "-enable-runtime-pseudo-reloc -Wl,--allow-multiple-definition")
        SET(StelMain stelMain)
    ELSE(WIN32)
        SET(StelMain)
    ENDIF(WIN32)
    TARGET_LINK_LIBRARIES(VisibilityContours ${StelMain} ${VisibilityContours_Qt_Libraries})
    INSTALL(TARGETS VisibilityContours DESTINATION "modules/VisibilityContours")
ELSE()
    ADD_LIBRARY(VisibilityContours-static STATIC ${VisibilityContours_SRCS})
    SET_TARGET_PROPERTIES(VisibilityContours-static PROPERTIES OUTPUT_NAME "VisibilityContours")
    TARGET_LINK_LIBRARIES(VisibilityContours-static ${StelMain} ${VisibilityContours_Qt_Libraries})
    SET_TARGET_PROPERTIES(VisibilityContours-static PROPERTIES COMPILE_FLAGS "-DQT_STATICPLUGIN")
    ADD_DEPENDENCIES(AllStaticPlugins VisibilityContours-static)
    SET_TARGET_PROPERTIES(VisibilityContours-static PROPERTIES FOLDER "plugins/VisibilityContours")
ENDIF()
