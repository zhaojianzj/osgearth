IF   (DYNAMIC_OSGEARTH)
    ADD_DEFINITIONS(-DOSGEARTHUTIL_LIBRARY)
ELSE (DYNAMIC_OSGEARTH)
    ADD_DEFINITIONS(-DOSGEARTHUTIL_LIBRARY_STATIC)
ENDIF(DYNAMIC_OSGEARTH)

SET(LIB_NAME osgEarthUtil)

SET(HEADER_PATH ${OSGEARTH_SOURCE_DIR}/include/${LIB_NAME})


IF(NOT (${OPENSCENEGRAPH_VERSION} VERSION_LESS "2.9.10"))

    SET( POST_2_9_10_HEADERS
        ShadowDecorator
    )
    
    SET( POST_2_9_10_SOURCES
        ShadowDecorator.cpp
    )
    
ENDIF(NOT (${OPENSCENEGRAPH_VERSION} VERSION_LESS "2.9.10"))



SET(LIB_PUBLIC_HEADERS  
    ${POST_2_9_10_HEADERS}  
    AutoClipPlaneHandler
    Common
    Controls
    ClampCallback
    EarthManipulator
    ElevationManager
    Export
    Graticule
	ImageOverlay
	ImageOverlayEditor
    ObjectPlacer
    ObjectLocator
    OceanSurfaceNode
    SkyNode    
    SpatialData
    StarData
    Viewpoint
	WFS
    WMS
)

ADD_LIBRARY(${LIB_NAME} SHARED
    ${LIB_PUBLIC_HEADERS}  
    ${POST_2_9_10_SOURCES}
    AutoClipPlaneHandler.cpp
    ClampCallback.cpp
    Controls.cpp
    EarthManipulator.cpp	
    ElevationManager.cpp
    Graticule.cpp
	ImageOverlay.cpp
	ImageOverlayEditor.cpp
    ObjectPlacer.cpp
    ObjectLocator.cpp
    OceanSurfaceNode.cpp
    SkyNode.cpp
    SpatialData.cpp
    Viewpoint.cpp
	WFS.cpp
    WMS.cpp
)

INCLUDE_DIRECTORIES(${OSG_INCLUDE_DIR} ${OSGEARTH_SOURCE_DIR})

IF (WIN32)
  LINK_EXTERNAL(${LIB_NAME} ${TARGET_EXTERNAL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIBRARY})
ELSE(WIN32)
  LINK_EXTERNAL(${LIB_NAME} ${TARGET_EXTERNAL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIBRARY})
ENDIF(WIN32)

LINK_INTERNAL(${LIB_NAME}
osgEarth
osgEarthFeatures
osgEarthSymbology
)

LINK_WITH_VARIABLES(${LIB_NAME} OSG_LIBRARY OSGUTIL_LIBRARY OSGSIM_LIBRARY OSGTERRAIN_LIBRARY OSGDB_LIBRARY OSGFX_LIBRARY OSGVIEWER_LIBRARY OSGTEXT_LIBRARY OSGGA_LIBRARY OSGSHADOW_LIBRARY OPENTHREADS_LIBRARY)
LINK_CORELIB_DEFAULT(${LIB_NAME} ${CMAKE_THREAD_LIBS_INIT} ${MATH_LIBRARY})

INCLUDE(ModuleInstall OPTIONAL)
