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
#include "MeshManager"
#include <osg/CullFace>
#include <osg/Texture2D>
#include <osgEarth/ImageToHeightFieldConverter>
#include <osgEarth/URI>

#define LC "[MeshManager] "

// --------------------------------------------------------------------------

struct ImageRequest : public osgEarth::TaskRequest
{
    ImageRequest( ImageLayer* layer, const TileKey& key ) : _layer(layer), _key(key) { }

    void operator()( ProgressCallback* progress )
    {
        _myResult = _layer->createImage( _key );
    }

    osg::ref_ptr<ImageLayer> _layer;
    TileKey _key;
    GeoImage _myResult;
};

struct ElevationRequest : public osgEarth::TaskRequest
{
    ElevationRequest( Map* map, const TileKey& key ) : _mapf(map), _key(key) { }

    void operator()( ProgressCallback* progress )
    {
        std::vector<TileKey> newkeys;
        _mapf.getProfile()->getIntersectingTiles( _key, newkeys );

        std::vector<GeoHeightField> fields;
        for( std::vector<TileKey>::const_iterator i = newkeys.begin(); i != newkeys.end(); ++i )
        {
            osg::ref_ptr<osg::HeightField> hf;
            if ( _mapf.getHeightField( *i, true, hf, 0L ) )
            {
                fields.push_back( GeoHeightField( hf.get(), i->getExtent(), i->getProfile()->getVerticalSRS() ) );
            }
        }

        _myResult = GeoHeightField::mosaic( fields, _key );
    }

    MapFrame         _mapf;
    TileKey          _key;
    GeoHeightField   _myResult;
};

struct ElevationLayerRequest : public osgEarth::TaskRequest
{
    ElevationLayerRequest( ElevationLayer* layer, const TileKey& key ) : _layer(layer), _key(key) { }

    void operator()( ProgressCallback* progress )
    {
        _result = _layer->createHeightField( _key, progress );
    }

    osg::ref_ptr<ElevationLayer> _layer;
    TileKey _key;
};

// --------------------------------------------------------------------------

MeshManager::MeshManager(Manifold* manifold, 
                         Map* map, 
                         const OceanSurfaceOptions& options,
                         ImageLayer* maskLayer,
                         ElevationLayer* bathyLayer ) :

_manifold       ( manifold ),
_map            ( map ),
_options        ( options ),
_maskLayer      ( maskLayer ),
_bathyLayer     ( bathyLayer ),
_minGeomLevel   ( 1 ),
_minActiveLevel ( 0 ),
_maxActiveLevel ( MAX_ACTIVE_LEVEL ),
_maxJobsPerFrame( MAX_JOBS_PER_FRAME )
{
    // fire up a task service to load textures.
    unsigned numTasks = 8;
    _taskService = new TaskService( "Ocean Service", numTasks );

    _amrGeom = new AMRGeometry();
    _amrGeom->setDataVariance( osg::Object::DYNAMIC );

    _amrGeode = new osg::Geode();
    _amrGeode->addDrawable( _amrGeom.get() );

    // set up the manifold framework.
    manifold->initialize( this );

    // the surface texture- TODO
    osg::Image* surfaceImage = URI( "../data/oceanalpha.int" ).readImage();
    _surfaceTex = new osg::Texture2D( surfaceImage );   
    _surfaceTex->setFilter( osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR );
    _surfaceTex->setFilter( osg::Texture::MAG_FILTER, osg::Texture::LINEAR );
    _surfaceTex->setWrap( osg::Texture::WRAP_S, osg::Texture::REPEAT );
    _surfaceTex->setWrap( osg::Texture::WRAP_T, osg::Texture::REPEAT ); 
    
    apply( _options );
}

void
MeshManager::apply( const OceanSurfaceOptions& options )
{
    _options = options;
    _amrGeom->_seaLevelUniform->set( *options.seaLevel() );
}

NodeIndex
MeshManager::addNode( const MeshNode& node )
{
    NodeIndex result;

    if ( _freeList.empty() )
    {
        _nodes.push_back( node );
        result = _nodes.size()-1;
    }
    else
    {
        NodeIndex ni = _freeList.front();
        _freeList.pop();
        _nodes[ni] = node;
        result = ni;
    }

    return result;
}

NodeIndex
MeshManager::addNode( const osg::Vec3d& manifoldCoord )
{
    return addNode( _manifold->createNode( manifoldCoord ) );
}

void
MeshManager::removeNode( NodeIndex ni )
{
    _freeList.push( ni );
}

void
MeshManager::queueForRefresh( Diamond* d )
{
    //OE_NOTICE << d->_name << ": queued for refresh." << std::endl;
    _dirtyQueue.push( d );
}

void
MeshManager::queueForSplit( Diamond* d, float priority )
{    
    if ( !d->_queuedForSplit )
    {
        //OE_NOTICE << "q split: " << d->_name << std::endl;        
        _splitQueue.push( DiamondJob( d, priority ) );
        d->_queuedForSplit = true;
    }
}

void
MeshManager::queueForMerge( Diamond* d, float priority )
{
    if ( !d->_queuedForMerge )
    {
        //OE_NOTICE << "q merge: " << d->_name << std::endl;
        _mergeQueue.push( DiamondJob( d, priority ) );
        d->_queuedForMerge = true;
    }
}

void
MeshManager::queueForImage( Diamond* d, float priority )
{
    if ( !d->_queuedForImage && !d->_imageRequest.valid() )
    {
        if ( _maskLayer.valid() )
        {
            d->_imageRequest = new ImageRequest( _maskLayer.get(), d->_key );
            _taskService->add( d->_imageRequest.get() );
            _imageQueue.push_back( DiamondJob( d, priority ) );
            d->_queuedForImage = true;
        }

        else if ( _bathyLayer.valid() )
        {
            d->_imageRequest = new ElevationLayerRequest( _bathyLayer.get(), d->_key );
            _taskService->add( d->_imageRequest.get() );
            _imageQueue.push_back( DiamondJob( d, priority ) );
            d->_queuedForImage = true;
        }

        else if ( _map.valid() )
        {
            d->_imageRequest = new ElevationRequest( _map.get(), d->_key );
            _taskService->add( d->_imageRequest.get() );
            _imageQueue.push_back( DiamondJob( d, priority ) );
            d->_queuedForImage = true;
        }
    }
}

static void
outlineTexture( osg::Image* image )
{
    for( int s=1; s<image->s()-1; ++s )
    {
        *((unsigned int*)image->data( s, 1 )) = 0x00ff00ff;
        *((unsigned int*)image->data( s, image->t()-2 )) = 0x00ff00ff;
    }

    for( int t=1; t<image->t()-1; ++t )
    {
        *((unsigned int*)image->data( 1, t )) = 0x00ff00ff;
        *((unsigned int*)image->data( image->s()-2, t )) = 0x00ff00ff;
    }
}

static
float remap( float a, float a0, float a1, float r0, float r1 )
{
    float ratio = ( osg::clampBetween(a,a0,a1)-a0)/(a1-a0);
    return r0 + ratio*(r1-r0);
}

void
MeshManager::update()
{
    int j;

    // process the split queue. these are diamonds that have requested to be split into
    // all four children.
    for( j=0; j<_maxJobsPerFrame && !_splitQueue.empty(); ++j )
    //if( !_splitQueue.empty() )
    {
        Diamond* d = _splitQueue.top()._d.get();
        if ( d->_status == ACTIVE && d->referenceCount() > 1 )
        {
            if ( d->_queuedForSplit )
            {
                //OE_NOTICE << "split: " << d->_name << "\n";
                d->split();
                d->_queuedForSplit = false;
                d->_queuedForMerge = false;
            }
            else
            {
                //OE_WARN << d->_name << " was in the split Q, but NOT marked for splitting!" << std::endl;
            }
        }
        else
        {
            // the diamond was removed while in the queue. ignore it.
        }
        _splitQueue.pop();
    }

    // process the merge queue. these are diamonds that have requested that all their
    // children be removed.
    // FUTURE: process jobs until we reach some sort of time quota?

    for( j=0; j<_maxJobsPerFrame && !_mergeQueue.empty(); ++j )
    {
        Diamond* d = _mergeQueue.top()._d.get();
        if ( d->_status == ACTIVE && d->referenceCount() > 1 )
        {
            if ( d->_queuedForMerge )
            {
                //OE_NOTICE << "merge: " << d->_name << "\n";

                //TODO: when you merge, children are recursively merged..thus the merge
                //may take some time. rather it might be better to traverse and schedule
                //child merges first.?
                d->merge();
                d->_queuedForMerge = false;
                d->_queuedForSplit = false;
            }
            else
            {
                //this means that the diamond was once queued for a merge, but then
                // passed cull again.
                //OE_WARN << d->_name << " was in the merge Q, but NOT marked for merging!" << std::endl;
            }
        }
        _mergeQueue.pop();
    }

    // process the texture image request queue.
    j=0;
    for( DiamondJobList::iterator i = _imageQueue.begin(); i != _imageQueue.end() && j < _maxJobsPerFrame; ++j )
    //if( _imageQueue.size() > 0 )
    {
        bool increment = true;
        bool remove = true;

        Diamond* d = i->_d.get();

        if ( d->_status == ACTIVE && d->referenceCount() > 1 && d->_imageRequest.valid() )
        {
            if ( d->_imageRequest->isCompleted() )
            {
                //OE_NOTICE << "REQ: " << d->_key->str() << " completed" << std::endl;
                osg::Texture2D* tex = 0L;
                
#ifdef USE_DEBUG_TEXTURES

                tex = new osg::Texture2D();
                tex->setImage( createDebugImage() );

#else

                if ( _maskLayer.valid() )
                {
                    const GeoImage& geoImage =  dynamic_cast<ImageRequest*>(d->_imageRequest.get())->_myResult;
                    if ( geoImage.valid() )
                    {
                        tex = new osg::Texture2D();
                        tex->setImage( geoImage.getImage() );
                    }
                }

                else if ( _bathyLayer.valid() )
                {
                    const osg::HeightField* hf = dynamic_cast<osg::HeightField*>( d->_imageRequest->getResult() );
                    if ( hf )
                    {
                        osg::Image* image = new osg::Image();
                        image->allocateImage(hf->getNumColumns(), hf->getNumRows(), 1, GL_LUMINANCE, GL_UNSIGNED_SHORT);
                        image->setInternalTextureFormat( GL_LUMINANCE16 );
                        const osg::FloatArray* floats = hf->getFloatArray();
                        for( unsigned int i = 0; i < floats->size(); ++i  ) {
                            int col = i % hf->getNumColumns();
                            int row = i / hf->getNumColumns();
                            *(unsigned short*)image->data( col, row ) = (unsigned short)(32768 + (short)floats->at(i));
                        }

                        tex = new osg::Texture2D();
                        tex->setImage( image );
                        tex->setUnRefImageDataAfterApply( true );
                    }
                }

                else if ( _map.valid() )
                {
                    GeoHeightField& ghf = dynamic_cast<ElevationRequest*>(d->_imageRequest.get())->_myResult;
                    if ( ghf.getHeightField() )
                    {
                        osg::HeightField* hf = ghf.getHeightField();
                        osg::Image* image = new osg::Image();
                        image->allocateImage(hf->getNumColumns(), hf->getNumRows(), 1, GL_LUMINANCE, GL_UNSIGNED_SHORT);
                        image->setInternalTextureFormat( GL_LUMINANCE16 );
                        const osg::FloatArray* floats = hf->getFloatArray();
                        for( unsigned int i = 0; i < floats->size(); ++i  ) {
                            int col = i % hf->getNumColumns();
                            int row = i / hf->getNumColumns();
                            *(unsigned short*)image->data( col, row ) = (unsigned short)(32768 + (short)floats->at(i));
                        }

                        tex = new osg::Texture2D();
                        tex->setImage( image );
                        tex->setUnRefImageDataAfterApply( true );
                    }
                }

#endif // USE_DEBUG_TEXTURES

                if ( tex )
                {
                    tex->setFilter( osg::Texture::MIN_FILTER, osg::Texture::NEAREST );
                    tex->setFilter( osg::Texture::MAG_FILTER, osg::Texture::LINEAR );

                    tex->setWrap( osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE );
                    tex->setWrap( osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE );
                    d->_stateSet->setTextureAttributeAndModes( 0, tex, osg::StateAttribute::ON );
                    d->_stateSet->dirty(); // bump revision number so that users of this stateset can detect the change
                    d->_hasFinalImage = true;

                    // temp:
                    d->_stateSet->setTextureAttributeAndModes( 1, _surfaceTex.get(), osg::StateAttribute::ON ); 
                    osg::Uniform* stUni = new osg::Uniform( osg::Uniform::SAMPLER_2D, "tex1" );
                    stUni->set( 1 );
                    d->_stateSet->addUniform( stUni );

#ifdef OUTLINE_TEXTURES

                    outlineTexture( tex->getImage() );
#endif
                }

                remove = true;
            }

            else
            {
                remove = false;
            }
        }


        if ( remove )
        {
            d->_imageRequest = 0L;
            d->_queuedForImage = false;
            i = _imageQueue.erase( i );
        }
        else
        {
            ++i;
        }
    }

#ifdef USE_DIRTY_QUEUE

    // process the dirty diamond queue. these are diamonds that have been changed and
    // need a new primitive set.
    // NOTE: we need to process the entire dirty queue each frame.
    while( _dirtyQueue.size() > 0 )
    {
        Diamond* d = _dirtyQueue.front().get();

        if ( d->_status == ACTIVE && d->referenceCount() > 1 )
        {
            // first, check to see whether the diamond's target stateset is ready. if so,
            // install it and mark it up to date.
            if ( d->_targetStateSetOwner->_stateSet->outOfSyncWith( d->_targetStateSetRevision ) )
            {            
                d->_amrDrawable->_stateSet = d->_targetStateSetOwner->_stateSet.get();

                d->_currentStateSetOwner = d->_targetStateSetOwner;
                d->_targetStateSetOwner->_stateSet->sync( d->_targetStateSetRevision );
            }

            // rebuild the primitives now.
            d->refreshDrawable();
        }
        _dirtyQueue.pop();
    }

#endif

    //OE_NOTICE << "dq size = " << _dirtyQueue.size() << "; splits = " << _splitQueue.size() << "; merges = " << _mergeQueue.size() << std::endl;
}
