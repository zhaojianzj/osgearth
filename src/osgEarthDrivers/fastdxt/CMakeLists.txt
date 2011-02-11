SET(TARGET_COMMON_LIBRARIES ${TARGET_COMMON_LIBRARIES})

SET(TARGET_SRC
    ReaderWriterFastDXT.cpp
	dxt.cpp
	util.cpp
	libdxt.cpp
	intrinsic.cpp
)
SET(TARGET_H
    dxt.h
	libdxt.h
	util.h
)

SETUP_PLUGIN(osgearth_fastdxt)

# to install public driver includes:
SET(LIB_NAME fastdxt)

