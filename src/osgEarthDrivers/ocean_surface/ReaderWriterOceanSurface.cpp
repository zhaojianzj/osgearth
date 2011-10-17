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
#include <osgDB/ReaderWriter>
#include <osgDB/FileNameUtils>
#include <osgDB/Registry>
#include <osgDB/FileUtils>
#include <osgEarth/Registry>
#include <osgEarth/ThreadingUtils>
#include <osgEarthDrivers/tms/TMSOptions>

#include "OceanSurface"
#include "DRoamNode"

#undef  LC
#define LC "[ReaderWriterOceanSurface] "

using namespace osgEarth;
using namespace osgEarth::Drivers;

//---------------------------------------------------------------------------

struct ReaderWriterOceanSurface : public osgDB::ReaderWriter
{
    ReaderWriterOceanSurface()
    {
        supportsExtension( "ocean_surface", "Ocean Surface" );
    }

    ReadResult readObject(const std::string& url, const Options* options) const
    {
        return readNode( url, options );
    }

    ReadResult readNode(const std::string& url, const Options* options) const
    {
        std::string ext = osgDB::getLowerCaseFileExtension(url);
        if ( !acceptsExtension(ext) )
            return ReadResult::FILE_NOT_HANDLED;

        Map*        map       = 0L;
        ImageLayer* maskLayer = 0L;

        if ( options )
        {
            map       = static_cast<Map*>( const_cast<void*>(options->getPluginData("osgEarth::Map")) );
            //maskLayer = static_cast<ImageLayer*>( const_cast<void*>(options->getPluginData("osgEarth::ImageLayer")) );
        }

        TMSOptions mask_o;
        mask_o.url() = "http://readymap.org/readymap/tiles/1.0.0/2/";
        maskLayer = new ImageLayer( "ocean mask", mask_o );

        return new DRoamNode( map, maskLayer );
    }
};

REGISTER_OSGPLUGIN( ocean_surface, ReaderWriterOceanSurface )
