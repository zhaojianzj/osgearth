OPTION(CURL_IS_STATIC "on if curl is a static lib " ON)
MARK_AS_ADVANCED(CURL_IS_STATIC)

IF(WIN32)
    SET(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} /NODEFAULTLIB:MSVCRT")
    IF(CURL_IS_STATIC)
        ADD_DEFINITIONS(-DCURL_STATICLIB)
        SET(TARGET_EXTERNAL_LIBRARIES ws2_32 winmm)
    ENDIF(CURL_IS_STATIC)
ENDIF(WIN32)
    
#SET(TARGET_COMMON_LIBRARIES ${TARGET_COMMON_LIBRARIES} ... )

SET(TARGET_H
    OceanSurface
    
    AMRGeometry
    AMRShaders.h
    Common
    CubeManifold
    Diamond
    DRoamNode
    GeodeticManifold
    Manifold
    MeshManager    
)

SET(TARGET_SRC
    ReaderWriterOceanSurface.cpp
    AMRGeometry.cpp
    CubeManifold.cpp
    Diamond.cpp
    DRoamNode.cpp
    GeodeticManifold.cpp
    Manifold.cpp
    MeshManager.cpp
)
    
SETUP_PLUGIN(ocean_surface)

SET(LIB_NAME ocean_surface)
SET(LIB_PUBLIC_HEADERS OceanSurface)
INCLUDE(ModuleInstallOsgEarthDriverIncludes OPTIONAL)
