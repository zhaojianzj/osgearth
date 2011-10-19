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
#include <osgEarthUtil/AutoClipPlaneHandler>
#include <osgEarth/FindNode>

using namespace osgEarth::Util;
using namespace osgEarth;

AutoClipPlaneHandler::AutoClipPlaneHandler( const Map* map ) :
_geocentric(false),
_frame(0),
_nfrAtRadius( 0.00001 ),
_nfrAtDoubleRadius( 0.0049 ),
_rp( -1 ),
_autoFarPlaneClipping(true)
{
    //NOP
    if ( map )
    {
        _geocentric = map->isGeocentric();
        if ( _geocentric )
            _rp = map->getProfile()->getSRS()->getEllipsoid()->getRadiusPolar();
    }
}

bool 
AutoClipPlaneHandler::handle( const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa )
{
    if ( ea.getEventType() == osgGA::GUIEventAdapter::FRAME && _frame++ > 1 )
    {
        frame( aa );
    }
    return false;
}

void
AutoClipPlaneHandler::frame( osgGA::GUIActionAdapter& aa )
{
    osg::Camera* cam = aa.asView()->getCamera();

    if ( _rp < 0 )
    {
        osg::ref_ptr<MapNode> tempNode = osgEarth::findTopMostNodeOfType<MapNode>( cam );
        if ( tempNode.valid() && tempNode->getMap()->getProfile() )
        {
            _geocentric = tempNode->getMap()->isGeocentric();
            if ( _geocentric )
                _rp = tempNode->getMap()->getProfile()->getSRS()->getEllipsoid()->getRadiusPolar();
            else
                OE_INFO << "[AutoClipPlaneHandler] disabled for non-geocentric map" << std::endl;
        }
    }

    if ( _rp > 0 && _geocentric )
    {
        cam->setComputeNearFarMode( osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR );

        osg::Vec3d eye, center, up;
        cam->getViewMatrixAsLookAt( eye, center, up );

        double d = eye.length();

        if ( d < _rp )
            d = _rp;

        if ( d > _rp )
        {
            double fovy, ar, znear, zfar, finalZfar;
            cam->getProjectionMatrixAsPerspective( fovy, ar, znear, finalZfar );

            // far clip at the horizon:
            zfar = sqrt( d*d - _rp*_rp );

            if (_autoFarPlaneClipping)
            {
                finalZfar = zfar;
            }

            double nfr = _nfrAtRadius + _nfrAtDoubleRadius * ((d-_rp)/d);
            znear = osg::clampAbove( zfar * nfr, 1.0 );

            cam->setProjectionMatrixAsPerspective( fovy, ar, znear, finalZfar );
        }
    }
}




namespace
{
    struct CustomProjClamper : public osg::CullSettings::ClampProjectionMatrixCallback
    {
        double _minNear, _maxFar, _nearFarRatio;

        template<class matrix_type, class value_type>
        bool _clampProjectionMatrix(matrix_type& projection, double& znear, double& zfar, value_type nearFarRatio) const
        {
            //OE_INFO << "clamp to [" << znear << " <=> " << zfar << "]" << std::endl;

            double epsilon = 1e-6;
            if (zfar<znear-epsilon)
            {
                OSG_INFO<<"_clampProjectionMatrix not applied, invalid depth range, znear = "<<znear<<"  zfar = "<<zfar<<std::endl;
                return false;
            }
            
            if (zfar<znear+epsilon)
            {
                // znear and zfar are too close together and could cause divide by zero problems
                // late on in the clamping code, so move the znear and zfar apart.
                double average = (znear+zfar)*0.5;
                znear = average-epsilon;
                zfar = average+epsilon;
                // OSG_INFO << "_clampProjectionMatrix widening znear and zfar to "<<znear<<" "<<zfar<<std::endl;
            }

            if (fabs(projection(0,3))<epsilon  && fabs(projection(1,3))<epsilon  && fabs(projection(2,3))<epsilon )
            {
                // OSG_INFO << "Orthographic matrix before clamping"<<projection<<std::endl;

                value_type delta_span = (zfar-znear)*0.02;
                if (delta_span<1.0) delta_span = 1.0;
                value_type desired_znear = znear - delta_span;
                value_type desired_zfar = zfar + delta_span;

                // assign the clamped values back to the computed values.
                znear = desired_znear;
                zfar = desired_zfar;

                projection(2,2)=-2.0f/(desired_zfar-desired_znear);
                projection(3,2)=-(desired_zfar+desired_znear)/(desired_zfar-desired_znear);

                // OSG_INFO << "Orthographic matrix after clamping "<<projection<<std::endl;
            }
            else
            {

                // OSG_INFO << "Persepective matrix before clamping"<<projection<<std::endl;

                //std::cout << "_computed_znear"<<_computed_znear<<std::endl;
                //std::cout << "_computed_zfar"<<_computed_zfar<<std::endl;

                value_type zfarPushRatio = 1.02;
                value_type znearPullRatio = 0.98;

                //znearPullRatio = 0.99; 

                value_type desired_znear = znear * znearPullRatio;
                value_type desired_zfar = zfar * zfarPushRatio;

                // near plane clamping.
                double min_near_plane = zfar*nearFarRatio;
                if (desired_znear<min_near_plane) desired_znear=min_near_plane;

                // assign the clamped values back to the computed values.
                znear = desired_znear;
                zfar = desired_zfar;

                value_type trans_near_plane = (-desired_znear*projection(2,2)+projection(3,2))/(-desired_znear*projection(2,3)+projection(3,3));
                value_type trans_far_plane = (-desired_zfar*projection(2,2)+projection(3,2))/(-desired_zfar*projection(2,3)+projection(3,3));

                value_type ratio = fabs(2.0/(trans_near_plane-trans_far_plane));
                value_type center = -(trans_near_plane+trans_far_plane)/2.0;

                projection.postMult(osg::Matrix(1.0f,0.0f,0.0f,0.0f,
                                                0.0f,1.0f,0.0f,0.0f,
                                                0.0f,0.0f,ratio,0.0f,
                                                0.0f,0.0f,center*ratio,1.0f));

                // OSG_INFO << "Persepective matrix after clamping"<<projection<<std::endl;
            }
            return true;
        }


        bool clampProjectionMatrixImplementation(osg::Matrixf& projection, double& znear, double& zfar) const
        {
            double n = std::max( znear, _minNear );
            double f = std::min( zfar, _maxFar );
            bool r = _clampProjectionMatrix( projection, n, f, _nearFarRatio );
            znear = n;
            zfar = f;
            return r;
        }

        bool clampProjectionMatrixImplementation(osg::Matrixd& projection, double& znear, double& zfar) const
        {
            double n = std::max( znear, _minNear );
            double f = std::min( zfar, _maxFar );
            bool r = _clampProjectionMatrix( projection, n, f, _nearFarRatio );
            znear = n;
            zfar = f;
            return r;
        }
    };
}







AutoClipPlaneCallback2::AutoClipPlaneCallback2( Map* map ) :
_map                 ( map ),
_minNearFarRatio     ( 0.00001 ),
_maxNearFarRatio     ( 0.0005 ),
_rp                  ( -1 ),
_autoFarPlaneClipping( true ),
_eq                  ( map )
{
    if ( map )
    {
        _rp = map->getProfile()->getSRS()->getEllipsoid()->getRadiusPolar();
    }
    else
    {
        _rp = 6356752.3142;
    }
}

void
AutoClipPlaneCallback2::operator()( osg::Node* node, osg::NodeVisitor* nv )
{
    osgUtil::CullVisitor* cv = dynamic_cast<osgUtil::CullVisitor*>( nv );
    if ( cv )
    {
        osg::Camera* cam = cv->getCurrentCamera();
        osg::ref_ptr<osg::CullSettings::ClampProjectionMatrixCallback>& clamper = _clampers.get(cam);
        if ( !clamper.valid() )
        {
            clamper = new CustomProjClamper();
            cam->setClampProjectionMatrixCallback( clamper.get() );
        }
        else
        {
            osg::Vec3d eye, center, up;
            cam->getViewMatrixAsLookAt( eye, center, up );

            osg::Vec3d loc;
            _map->worldPointToMapPoint( eye, loc );

            //double elevation;
            //_eq.getElevation( loc, 0L, elevation, 10.0 );

            //double hat = loc.z() - elevation;

            double hat = loc.z();

            CustomProjClamper* c = static_cast<CustomProjClamper*>(clamper.get());
            if ( hat > 250.0 )
            {
                c->_minNear = -DBL_MAX;
                c->_maxFar  =  DBL_MAX;
                c->_nearFarRatio = 0.0005;
            }
            else
            {
                c->_minNear = -DBL_MAX;
                c->_maxFar  =  DBL_MAX;
                c->_nearFarRatio = 0.00001;
            }
#if 0
            double d = eye.length();

            if ( d < _rp )
            {
                d = _rp;
            }
            else if ( d > _rp )
            {
                CustomProjClamper* c = static_cast<CustomProjClamper*>(clamper.get());

                c->_minNear = -99999999.0;

                double horizon = sqrt( d*d - _rp*_rp );
                c->_maxFar = horizon;

                double delta = _maxNearFarRatio - _minNearFarRatio;
                c->_nearFarRatio = _minNearFarRatio + delta*((d-_rp)/d);
            
            }
#endif
        }

                {
                    double n, f, a, v;
                    cv->getProjectionMatrix()->getPerspective(v, a, n, f);
                    OE_INFO << std::fixed << "near = " << n << ", far = " << f << ", ratio = " << n/f << std::endl;
                }
        
    }
    traverse( node, nv );
}

