    
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
    
SETUP_PLUGIN(osgearth_ocean_surface)

SET(LIB_NAME osgearth_ocean_surface)
SET(LIB_PUBLIC_HEADERS OceanSurface)
INCLUDE(ModuleInstallOsgEarthDriverIncludes OPTIONAL)
