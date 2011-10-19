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
#include "DRoamNode"
#include "CubeManifold"
#include "GeodeticManifold"
#include <osgEarth/Cube>
#include <osgEarthDrivers/tms/TMSOptions>
#include <osg/Depth>
#include <osg/PolygonOffset>

using namespace osgEarth;
using namespace osgEarth::Drivers;

DRoamNode::DRoamNode( Map* map, const OceanSurfaceOptions& options ) :
_map       ( map ),
_options   ( options )
{
    this->setNumChildrenRequiringUpdateTraversal( 1 );

    _manifold = new CubeManifold();

    _mesh = new MeshManager( _manifold.get(), _map.get(), _options, _maskLayer.get(), _bathyLayer.get() );

    _mesh->_maxActiveLevel = MAX_ACTIVE_LEVEL;

    this->setInitialBound( _manifold->initialBound() );

    this->addChild( _mesh->_amrGeode.get() );

    osg::StateSet* sset = this->getOrCreateStateSet();
    sset->setMode( GL_LIGHTING, 0 );
    sset->setMode( GL_BLEND, 1 );
    sset->setMode( GL_CULL_FACE, 0 );

    // trick to prevent z-fighting..
    sset->setAttributeAndModes( new osg::Depth(osg::Depth::LEQUAL, 0.0, 1.0, false) );
    sset->setRenderBinDetails( 15, "RenderBin" ); //, "DepthSortedBin" );
}

void
DRoamNode::apply( const OceanSurfaceOptions& options )
{
    _mesh->apply( options );
}

void
DRoamNode::traverse( osg::NodeVisitor& nv )
{
    if ( nv.getVisitorType() == osg::NodeVisitor::CULL_VISITOR )
    {
        osgUtil::CullVisitor* cv = static_cast<osgUtil::CullVisitor*>( &nv );

        _mesh->_amrDrawList.clear();

        _manifold->cull( cv );

        // I know is not strictly kosher to modify the scene graph from the CULL traversal. But
        // we need frame-coherence, and our one Drawable is marked DYNAMIC so it should be safe.
        _mesh->_amrGeom->setDrawList( _mesh->_amrDrawList );
        _mesh->_amrGeode->dirtyBound();

#ifdef DISABLE_NEAR_FAR
        osg::CullSettings::ComputeNearFarMode saveMode = cv->getComputeNearFarMode();
        cv->setComputeNearFarMode( osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR );
#endif

        osg::Group::traverse( nv );

#ifdef DISABLE_NEAR_FAR
        cv->setComputeNearFarMode( saveMode );
#endif
    }
    else
    {
        if ( nv.getVisitorType() == osg::NodeVisitor::UPDATE_VISITOR )
        {
            _mesh->update();
        }
        osg::Group::traverse( nv );
    }
}
