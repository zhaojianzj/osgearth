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

// --------------------------------------------------------------------------

#ifdef USE_IMAGE_MASK

// The mask-based approach:

static char source_vertShaderMain_geocentricMethod[] =

"uniform sampler2D tex0; \n"
"uniform mat4 osg_ViewMatrixInverse; \n"

"varying float v_maskValue; \n"
"varying float v_elevation; \n"
"varying float v_range; \n"

"uniform vec3 v0, v1, v2; \n"
"uniform vec2 t0, t1, t2; \n"
"varying vec2 texCoord0; \n"
"\n"
"void main (void) \n"
"{ \n"
"   // interpolate vert form barycentric coords \n"
"   float u = gl_Vertex.x; \n"
"   float v = gl_Vertex.y; \n"
"   float w = gl_Vertex.z; // 1-u-v  \n"
"   vec3 outVert3 = v0*u + v1*v + v2*w; \n"
"   float h = length(v0)*u + length(v1)*v + length(v2)*w; // interpolate height \n" // ...oops, local space
"   vec4 outVert4 = vec4( normalize(outVert3) * h, gl_Vertex.w ); \n"
"   gl_Position = gl_ModelViewProjectionMatrix * outVert4; \n"
"\n"
"   // set up the tex coords for the frad shader: \n"
"   u = gl_MultiTexCoord0.s; \n"
"   v = gl_MultiTexCoord0.t; \n"
"   w = 1.0 - u - v; \n"
"   texCoord0 = t0*u + t1*v + t2*w; \n"

"   v_maskValue  = texture2D( tex0, texCoord0 ).a; \n"

// scale the texture mapping....
"   texCoord0 *= 100.0; \n"

// elevation...
"   vec4 eye = osg_ViewMatrixInverse * vec4(0,0,0,1); \n"
"   v_elevation = length(eye.xyz) - 6378137.0; \n"

// range...
"   v_range = distance(outVert3, gl_ModelViewMatrixInverse[3]); \n"
"} \n";

char source_fragShaderMain[] = 

"float remap( float val, float vmin, float vmax, float r0, float r1 ) \n"
"{ \n"
"    float vr = (clamp(val, vmin, vmax)-vmin)/(vmax-vmin); \n"
"    return r0 + vr * (r1-r0); \n"
"} \n"

"varying float v_maskValue; \n"
"varying float v_elevation; \n"
"varying float v_range; \n"

"varying vec2 texCoord0; \n"
"uniform sampler2D tex0, tex1; \n"
"\n"
"void main (void) \n"
"{ \n"
"    vec3 baseColor = vec3( 0.1, 0.3, 0.5 ); \n"

"    float eMoney = remap( v_elevation, -10000, 10000, 10.0, 1.0 ); \n"

// alpha based on camera range....
"    v_maskValue = 1.0 - (1.0-v_maskValue)*(1.0-v_maskValue); \n"
"    float rangeEffect = remap( v_range, 75000, 200000 * eMoney, 1.0, 0.0 ); \n"
"    float maskEffect  = remap( v_maskValue, 0.0, 1.0, 0.0, 1.0 ); \n"

#ifdef USE_TEXTURES
"    float texAlpha = texture2D( tex1, texCoord0 ).r; \n"
"    gl_FragColor = vec4( baseColor, texAlpha * maskEffect * rangeEffect); \n"
#else
"    gl_FragColor = vec4(.5, .5, 1.0, 0.5); \n"
#endif
"} \n";

#else // !USE_IMAGE_MASK (use elevation mask)

//------------------------------------------------------------------------
// The elevation-based approach:

static char source_vertShaderMain_geocentricMethod[] =

"uniform mat4 osg_ViewMatrixInverse; \n"
"uniform vec3 v0, v1, v2; \n"                // triangle verts
"uniform vec2 t0, t1, t2; \n"                // triangle tex coords
"uniform sampler2D tex0; \n"                 // heightfield encoded into 16 bit texture

"varying float v_elevation; \n"              // elevation (HAE) of camera
"varying float v_range; \n"                  // distance from camera to current vertex
"varying float v_enorm; \n"                  // normalized terrain height at vertex [0..1]
"varying vec2 texCoord0; \n"
"\n"
"void main (void) \n"
"{ \n"
// interpolate vertex from barycentric coords:
"   float u = gl_Vertex.x; \n"
"   float v = gl_Vertex.y; \n"
"   float w = gl_Vertex.z; // 1-u-v  \n"
"   vec3 outVert3 = v0*u + v1*v + v2*w; \n"

// next interpolate the height along geocentric space (-ish):
"   float h = length(v0)*u + length(v1)*v + length(v2)*w; \n"
"   vec4 outVert4 = vec4( normalize(outVert3) * h, gl_Vertex.w ); \n"
"   gl_Position = gl_ModelViewProjectionMatrix * outVert4; \n"

// set up the tex coords for the frag shader:
"   u = gl_MultiTexCoord0.s; \n"
"   v = gl_MultiTexCoord0.t; \n"
"   w = 1.0 - u - v; \n"
"   texCoord0 = t0*u + t1*v + t2*w; \n"

// read normalized [0..1] elevation data from the height texture:
"   v_enorm = texture2D( tex0, texCoord0 ).r; \n"

// scale the texture mapping to something reasonable:
"   texCoord0 *= 100.0; \n"

// calculate the approximate elevation:
"   vec4 eye = osg_ViewMatrixInverse * vec4(0,0,0,1); \n"
"   v_elevation = length(eye.xyz) - 6378137.0; \n"

// calculate distance from camera to current vertex:
"   v_range = distance(outVert3, gl_ModelViewMatrixInverse[3]); \n"
"} \n";


char source_fragShaderMain[] = 

// clamps a value to the vmin/vmax range, then re-maps it to the r0/r1 range:
"float remap( float val, float vmin, float vmax, float r0, float r1 ) \n"
"{ \n"
"    float vr = (clamp(val, vmin, vmax)-vmin)/(vmax-vmin); \n"
"    return r0 + vr * (r1-r0); \n"
"} \n"

"varying float v_elevation; \n"              // elevation (HAE) of camera
"varying float v_range; \n"                  // distance from camera to current vertex
"varying float v_enorm; \n"                  // normalized terrain height at vertex [0..1]

"varying vec2 texCoord0; \n"
"uniform sampler2D tex1; \n"                 // intensity texture (water surface)

"void main (void) \n"
"{ \n"
// baseline ocean color
"    vec3 baseColor = vec3( 0.25, 0.35, 0.6 ); \n"

// un-normalize the heightfield data
"    float elev = (v_enorm * 65535.0) - 32768.0; \n"                  

// heightfield's effect on alpha [0..1]
"    float elevEffect = remap( elev, -50.0, -10.0, 1.0, 0.0 ); \n" 

// amplify the range's effect on alpha when the camera elevation gets low
"    float rangeFactor = remap( v_elevation, -10000, 10000, 10.0, 1.0 ); \n"
"    float rangeEffect = remap( v_range, 75000, 200000 * rangeFactor, 1.0, 0.0 ); \n"

// balance between texture-based alpha and static alpha
"    float texBalance = remap( v_range, 7500, 15000, 0.0, 1.0 ); \n"
"    float texIntensity = texture2D( tex1, texCoord0 ).r; \n"
"    float texEffect = mix( texIntensity, 0.8, texBalance ); \n"

// color it
"    gl_FragColor = vec4( baseColor, texEffect * elevEffect * rangeEffect); \n"

//"    gl_FragColor = vec4( 1, 0, 0, 1 ); \n" // debugging
"} \n";

#endif // USE_IMAGE_MASK