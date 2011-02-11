/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
* Copyright 2008-2010 Pelican Mapping
* http://osgearth.org
*
* osgEarth is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>
*/

#include <osgEarth/ImageUtils>
#include <osgEarth/ImageCompressor>
#include <osg/Timer>
#include <osg/Texture>
#include <osgDB/ReaderWriter>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>
#include "libdxt.h"

using namespace osgEarth;

class FastDXTImageCompressor : public osgEarth::ImageCompressor
{
public:
    FastDXTImageCompressor()
    {
    }

    virtual osg::Object* cloneType() const { return 0; } // cloneType() not appropriate
    virtual osg::Object* clone(const osg::CopyOp&) const { return 0; } // clone() not appropriate
    virtual bool isSameKindAs(const osg::Object* obj) const { return dynamic_cast<const FastDXTImageCompressor*>(obj)!=NULL; }
    virtual const char* className() const { return "FastDXTImageCompressor"; }
    virtual const char* libraryName() const { return "osgEarth"; }

    virtual osg::Image* compress(osg::Image* image)
    {
        if (image->getPixelFormat() != GL_RGBA && image->getPixelFormat() != GL_RGB)
    {
        OE_WARN << "ImageUtils::compress only supports GL_RGB or GL_RGBA images" << std::endl;
        return 0;
    }
    osg::Timer_t start = osg::Timer::instance()->tick();

    //Clone the image
    osg::ref_ptr< osg::Image > result;

    //Resize the image to the nearest power of two 
    if (!osgEarth::ImageUtils::isPowerOfTwo( image ))
    {            
        OE_DEBUG << "Resizing" << std::endl;
        unsigned int s = osg::Image::computeNearestPowerOfTwo( image->s() );
        unsigned int t = osg::Image::computeNearestPowerOfTwo( image->t() );
        ImageUtils::resizeImage(image, s, t, result);
    }
    else
    {
        //Just clone the image, no need to resize
        result = image;
    }

    //Allocate memory for the output
    unsigned char* out = (unsigned char*)memalign(16, result->s()*result->t()*4);
    memset(out, 0, result->s()*result->t()*4);

    //FastDXT only works on RGBA imagery so we must convert it
    if (result->getPixelFormat() != GL_RGBA)
    {        
        osg::Timer_t start = osg::Timer::instance()->tick();
        result = osgEarth::ImageUtils::convertToRGBA8( result );
        osg::Timer_t end = osg::Timer::instance()->tick();
        OE_DEBUG << "conversion to rgba took" << osg::Timer::instance()->delta_m(start, end) << std::endl;
    }

    //Copy over the source data to an array
    unsigned char *in = 0;
    in = (unsigned char*)memalign(16, result->getTotalSizeInBytes());
    memcpy(in, result->data(0,0), result->getTotalSizeInBytes());

    int format;
    GLint pixelFormat;
    if (image->getPixelFormat() == GL_RGB)
    {
        format = FORMAT_DXT1;
        pixelFormat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    }
    else
    {
        format = FORMAT_DXT5;
        pixelFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    }

    int outputBytes = CompressDXT(in, out, result->s(), result->t(), format);

    //Allocate and copy over the output data to the correct size array.
    unsigned char* data = (unsigned char*)malloc(outputBytes);
    memcpy(data, out, outputBytes);
    memfree(out);
    memfree(in);

    result->setImage(result->s(), result->t(), result->r(), pixelFormat, pixelFormat, GL_UNSIGNED_BYTE, data, osg::Image::USE_MALLOC_FREE);
    osg::Timer_t end = osg::Timer::instance()->tick();
    OE_DEBUG << "ImageUtils compress took " << osg::Timer::instance()->delta_m(start, end) << "ms" << std::endl;
    return result.release();

    }
};

class ReaderWriterFastDXT : public osgDB::ReaderWriter
{
public:
    ReaderWriterFastDXT()
    {
        supportsExtension( "osgearth_fastdxt", "FastDXT ImageCompressor" );
    }

    virtual const char* className()
    {
        return "ReaderWriterFastDXT";
    }

    virtual ReadResult readObject(const std::string& file_name, const Options* options) const
    {
        if ( !acceptsExtension(osgDB::getLowerCaseFileExtension( file_name )))
            return ReadResult::FILE_NOT_HANDLED;

        //return new OSGTileSource( getTileSourceOptions(options) );
        return new FastDXTImageCompressor();
    }
};



REGISTER_OSGPLUGIN(osgearth_fastdxt, ReaderWriterFastDXT)
