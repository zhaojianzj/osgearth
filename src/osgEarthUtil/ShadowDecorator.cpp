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
#include <osgEarthUtil/ShadowDecorator>
#include <osgShadow/ShadowedScene>
#include <osgShadow/ShadowMap>
#include <osgShadow/SoftShadowMap>
#include <osgShadow/MinimalShadowMap>
#include <osgShadow/LightSpacePerspectiveShadowMap>

#define LC "[ShadowDecorator] "

using namespace osgEarth::Util;

ShadowDecorator::ShadowDecorator()
{
    //nop
}

void
ShadowDecorator::setLight( osg::Light* light )
{
    _light = light;
    if ( _scene.valid() )
    {
        osgShadow::MinimalShadowMap* sm = dynamic_cast<osgShadow::MinimalShadowMap*>( _scene->getShadowTechnique() );
        if ( sm )
            sm->setLight( light );
    }
}

void 
ShadowDecorator::onInstall( TerrainEngineNode* engine )
{
    _scene = new osgShadow::ShadowedScene();

    osgShadow::MinimalShadowMap* msm = new osgShadow::LightSpacePerspectiveShadowMapDB();   

    float minLightMargin = 10.f;
    float maxFarPlane = 0;
    unsigned int texSize = 1024;
    unsigned int baseTexUnit = 0;
    unsigned int shadowTexUnit = 7;

    msm->setMinLightMargin( minLightMargin );
    msm->setMaxFarPlane( maxFarPlane );
    msm->setTextureSize( osg::Vec2s( texSize, texSize ) );
    msm->setShadowTextureCoordIndex( shadowTexUnit );
    msm->setShadowTextureUnit( shadowTexUnit );
    msm->setBaseTextureCoordIndex( baseTexUnit );
    msm->setBaseTextureUnit( baseTexUnit );

    if ( _light.valid() )
        msm->setLight( _light.get() );

    _scene->setShadowTechnique( msm );

    // add this decorator's children to the scene.
    for( unsigned i=0; i<getNumChildren(); ++i )
    {
        _scene->addChild( getChild(i) );
    }
}

void
ShadowDecorator::onUninstall( TerrainEngineNode* engine )
{
    _scene = 0L;
}

void
ShadowDecorator::traverse( osg::NodeVisitor& nv )
{
    if ( _scene.valid() )
    {
        _scene->accept( nv );
    }
    else
    {
        TerrainDecorator::traverse( nv );
    }
}
