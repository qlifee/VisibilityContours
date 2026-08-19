INCLUDE_DIRECTORIES(.)
LINK_DIRECTORIES(${BUILD_DIR}/src)

SET(OdehContours_SRCS
    OdehContours.hpp
    OdehContours.cpp
)

SET(OdehContours_Qt_Libraries Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui Qt${QT_VERSION_MAJOR}::OpenGL)

IF(BUILD_DYNAMIC_PLUGIN)
    ADD_LIBRARY(OdehContours MODULE ${OdehContours_SRCS})
    IF(APPLE)
        FIND_LIBRARY(OPENGL_LIBRARY OpenGL)
        MARK_AS_ADVANCED(OPENGL_LIBRARY)
        SET_TARGET_PROPERTIES(OdehContours PROPERTIES LINK_FLAGS "-undefined dynamic_lookup" SUFFIX ".dylib")
    ENDIF()
    IF(WIN32)
        SET_TARGET_PROPERTIES(OdehContours PROPERTIES LINK_FLAGS "-enable-runtime-pseudo-reloc -Wl,--allow-multiple-definition")
        SET(StelMain stelMain)
    ELSE(WIN32)
        SET(StelMain)
    ENDIF(WIN32)
    TARGET_LINK_LIBRARIES(OdehContours ${StelMain} ${OdehContours_Qt_Libraries})
    INSTALL(TARGETS OdehContours DESTINATION "modules/OdehContours")
ELSE()
    ADD_LIBRARY(OdehContours-static STATIC ${OdehContours_SRCS})
    SET_TARGET_PROPERTIES(OdehContours-static PROPERTIES OUTPUT_NAME "OdehContours")
    TARGET_LINK_LIBRARIES(OdehContours-static ${StelMain} ${OdehContours_Qt_Libraries})
    SET_TARGET_PROPERTIES(OdehContours-static PROPERTIES COMPILE_FLAGS "-DQT_STATICPLUGIN")
    ADD_DEPENDENCIES(AllStaticPlugins OdehContours-static)
    SET_TARGET_PROPERTIES(OdehContours-static PROPERTIES FOLDER "plugins/OdehContours")
ENDIF()
