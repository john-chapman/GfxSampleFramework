// Adapted from https://github.com/stegu/webgl-noise/

// Copyright (C) 2011 by Ashima Arts (Simplex noise)
// Copyright (C) 2011-2016 by Stefan Gustavson (Classic noise and others)
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// Utility functions

float _webglnoise_mod289(float x)           { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec2  _webglnoise_mod289(vec2 x)            { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3  _webglnoise_mod289(vec3 x)            { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4  _webglnoise_mod289(vec4 x)            { return x - floor(x * (1.0 / 289.0)) * 289.0; }

float _webglnoise_permute(float x)          { return _webglnoise_mod289(((x * 34.0) + 1.0) * x); }
vec2  _webglnoise_permute(vec2 x)           { return _webglnoise_mod289(((x * 34.0) + 1.0) * x); }
vec3  _webglnoise_permute(vec3 x)           { return _webglnoise_mod289(((x * 34.0) + 1.0) * x); }
vec4  _webglnoise_permute(vec4 x)           { return _webglnoise_mod289(((x * 34.0) + 1.0) * x); }

float _webglnoise_taylorInvSqrt(float r)    { return 1.79284291400159 - 0.85373472095314 * r; }
vec2  _webglnoise_taylorInvSqrt(vec2 r)     { return 1.79284291400159 - 0.85373472095314 * r; }
vec3  _webglnoise_taylorInvSqrt(vec3 r)     { return 1.79284291400159 - 0.85373472095314 * r; }
vec4  _webglnoise_taylorInvSqrt(vec4 r)     { return 1.79284291400159 - 0.85373472095314 * r; }

vec4 _webglnoise_grad4(float j, vec4 ip)
{
	const vec4 ones = vec4(1.0, 1.0, 1.0, -1.0);
	vec4 p,s;

	p.xyz = floor( fract (vec3(j) * ip.xyz) * 7.0) * ip.z - 1.0;
	p.w = 1.5 - dot(abs(p.xyz), ones.xyz);
	s = vec4(lessThan(p, vec4(0.0)));
	p.xyz = p.xyz + (s.xyz*2.0 - 1.0) * s.www; 

	return p;
}

// Hashed 2-D gradients with an extra rotation (the constant 0.0243902439 is 1/41)
vec2 _webglnoise_rgrad2(vec2 p, float rot) {
#if 0
// Map from a line to a diamond such that a shift maps to a rotation.
	float u = _webglnoise_permute(_webglnoise_permute(p.x) + p.y) * 0.0243902439 + rot; // Rotate by shift
	u = 4.0 * fract(u) - 2.0;
	// (This vector could be normalized, exactly or approximately.)
	return vec2(abs(u)-1.0, abs(abs(u+1.0)-2.0)-1.0);
#else
// For more isotropic gradients, sin/cos can be used instead.
	float u = _webglnoise_permute(_webglnoise_permute(p.x) + p.y) * 0.0243902439 + rot; // Rotate by shift
	u = fract(u) * 6.28318530718; // 2*pi
	return vec2(cos(u), sin(u));
#endif
}


float _webglnoise_snoise(vec2 v)
{
	const vec4 C = vec4(
		0.211324865405187,   // (3.0-sqrt(3.0))/6.0
		0.366025403784439,   // 0.5*(sqrt(3.0)-1.0)
		-0.577350269189626,  // -1.0 + 2.0 * C.x
		0.024390243902439    // 1.0 / 41.0
		);

// First corner
	vec2 i  = floor(v + dot(v, C.yy) );
	vec2 x0 = v -   i + dot(i, C.xx);

// Other corners
	vec2 i1;
	//i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0
	//i1.y = 1.0 - i1.x;
	i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
	// x0 = x0 - 0.0 + 0.0 * C.xx ;
	// x1 = x0 - i1 + 1.0 * C.xx ;
	// x2 = x0 - 1.0 + 2.0 * C.xx ;
	vec4 x12 = x0.xyxy + C.xxzz;
	x12.xy -= i1;

// Permutations
	i = _webglnoise_mod289(i); // Avoid truncation effects in permutation
	vec3 p = _webglnoise_permute( _webglnoise_permute( i.y + vec3(0.0, i1.y, 1.0 )) + i.x + vec3(0.0, i1.x, 1.0 ));

	vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
	m = m*m ;
	m = m*m ;

// Gradients: 41 points uniformly over a line, mapped onto a diamond.
// The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)
	vec3 x = 2.0 * fract(p * C.www) - 1.0;
	vec3 h = abs(x) - 0.5;
	vec3 ox = floor(x + 0.5);
	vec3 a0 = x - ox;

// Normalise gradients implicitly by scaling m
// Approximation of: m *= inversesqrt( a0*a0 + h*h );
	m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );

// Compute final noise value at P
	vec3 g;
	g.x  = a0.x  * x0.x  + h.x  * x0.y;
	g.yz = a0.yz * x12.xz + h.yz * x12.yw;
	return 130.0 * dot(m, g);
}


float _webglnoise_snoise(vec3 v)
{
	const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
	const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

// First corner
	vec3 i  = floor(v + dot(v, C.yyy) );
	vec3 x0 =   v - i + dot(i, C.xxx) ;

// Other corners
	vec3 g = step(x0.yzx, x0.xyz);
	vec3 l = 1.0 - g;
	vec3 i1 = min( g.xyz, l.zxy );
	vec3 i2 = max( g.xyz, l.zxy );

	//   x0 = x0 - 0.0 + 0.0 * C.xxx;
	//   x1 = x0 - i1  + 1.0 * C.xxx;
	//   x2 = x0 - i2  + 2.0 * C.xxx;
	//   x3 = x0 - 1.0 + 3.0 * C.xxx;
	vec3 x1 = x0 - i1 + C.xxx;
	vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
	vec3 x3 = x0 - D.yyy;      // -1.0+3.0*C.x = -0.5 = -D.y

// Permutations
	i = _webglnoise_mod289(i); 
	vec4 p = _webglnoise_permute( _webglnoise_permute( _webglnoise_permute( 
		i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
		+ i.y + vec4(0.0, i1.y, i2.y, 1.0 )) 
		+ i.x + vec4(0.0, i1.x, i2.x, 1.0 ));

// Gradients: 7x7 points over a square, mapped onto an octahedron.
// The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
	float n_ = 0.142857142857; // 1.0/7.0
	vec3  ns = n_ * D.wyz - D.xzx;

	vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  //  mod(p,7*7)

	vec4 x_ = floor(j * ns.z);
	vec4 y_ = floor(j - 7.0 * x_ );    // mod(j,N)

	vec4 x = x_ *ns.x + ns.yyyy;
	vec4 y = y_ *ns.x + ns.yyyy;
	vec4 h = 1.0 - abs(x) - abs(y);

	vec4 b0 = vec4( x.xy, y.xy );
	vec4 b1 = vec4( x.zw, y.zw );

	//vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
	//vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
	vec4 s0 = floor(b0)*2.0 + 1.0;
	vec4 s1 = floor(b1)*2.0 + 1.0;
	vec4 sh = -step(h, vec4(0.0));

	vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
	vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

	vec3 p0 = vec3(a0.xy,h.x);
	vec3 p1 = vec3(a0.zw,h.y);
	vec3 p2 = vec3(a1.xy,h.z);
	vec3 p3 = vec3(a1.zw,h.w);

//Normalise gradients
	vec4 norm = _webglnoise_taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;

// Mix final noise value
	vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
	m = m * m;
	return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1), dot(p2,x2), dot(p3,x3) ) );
}


float _webglnoise_snoise(vec4 v)
{
	// (sqrt(5) - 1)/4 = F4, used once below
	const float F4 = 0.309016994374947451;

	const vec4  C = vec4( 0.138196601125011,  // (5 - sqrt(5))/20  G4
		0.276393202250021,  // 2 * G4
		0.414589803375032,  // 3 * G4
		-0.447213595499958 // -1 + 4 * G4
		);

// First corner
	vec4 i  = floor(v + dot(v, vec4(F4)) );
	vec4 x0 = v -   i + dot(i, C.xxxx);

// Other corners

// Rank sorting originally contributed by Bill Licea-Kane, AMD (formerly ATI)
	vec4 i0;
	vec3 isX = step( x0.yzw, x0.xxx );
	vec3 isYZ = step( x0.zww, x0.yyz );
//	i0.x = dot( isX, vec3( 1.0 ) );
	i0.x = isX.x + isX.y + isX.z;
	i0.yzw = 1.0 - isX;
//	i0.y += dot( isYZ.xy, vec2( 1.0 ) );
	i0.y += isYZ.x + isYZ.y;
	i0.zw += 1.0 - isYZ.xy;
	i0.z += isYZ.z;
	i0.w += 1.0 - isYZ.z;

// i0 now contains the unique values 0,1,2,3 in each channel
	vec4 i3 = clamp( i0, 0.0, 1.0 );
	vec4 i2 = clamp( i0-1.0, 0.0, 1.0 );
	vec4 i1 = clamp( i0-2.0, 0.0, 1.0 );

	//  x0 = x0 - 0.0 + 0.0 * C.xxxx
	//  x1 = x0 - i1  + 1.0 * C.xxxx
	//  x2 = x0 - i2  + 2.0 * C.xxxx
	//  x3 = x0 - i3  + 3.0 * C.xxxx
	//  x4 = x0 - 1.0 + 4.0 * C.xxxx
	vec4 x1 = x0 - i1 + C.xxxx;
	vec4 x2 = x0 - i2 + C.yyyy;
	vec4 x3 = x0 - i3 + C.zzzz;
	vec4 x4 = x0 + C.wwww;

// Permutations
	i = _webglnoise_mod289(i); 
	float j0 = _webglnoise_permute( _webglnoise_permute( _webglnoise_permute( _webglnoise_permute(i.w) + i.z) + i.y) + i.x);
	vec4 j1 = _webglnoise_permute( _webglnoise_permute( _webglnoise_permute( _webglnoise_permute (
		i.w + vec4(i1.w, i2.w, i3.w, 1.0 ))
		+ i.z + vec4(i1.z, i2.z, i3.z, 1.0 ))
		+ i.y + vec4(i1.y, i2.y, i3.y, 1.0 ))
		+ i.x + vec4(i1.x, i2.x, i3.x, 1.0 ));

// Gradients: 7x7x6 points over a cube, mapped onto a 4-cross polytope
// 7*7*6 = 294, which is close to the ring size 17*17 = 289.
	vec4 ip = vec4(1.0/294.0, 1.0/49.0, 1.0/7.0, 0.0) ;

	vec4 p0 = _webglnoise_grad4(j0,   ip);
	vec4 p1 = _webglnoise_grad4(j1.x, ip);
	vec4 p2 = _webglnoise_grad4(j1.y, ip);
	vec4 p3 = _webglnoise_grad4(j1.z, ip);
	vec4 p4 = _webglnoise_grad4(j1.w, ip);

// Normalise gradients
	vec4 norm = _webglnoise_taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
	p0 *= norm.x;
	p1 *= norm.y;
	p2 *= norm.z;
	p3 *= norm.w;
	p4 *= _webglnoise_taylorInvSqrt(dot(p4,p4));

// Mix contributions from the five corners
	vec3 m0 = max(0.6 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.0);
	vec2 m1 = max(0.6 - vec2(dot(x3,x3), dot(x4,x4)            ), 0.0);
	m0 = m0 * m0;
	m1 = m1 * m1;
	return 49.0 * ( dot(m0*m0, vec3( dot( p0, x0 ), dot( p1, x1 ), dot( p2, x2 ))) + dot(m1*m1, vec2( dot( p3, x3 ), dot( p4, x4 ) ) ) ) ;
}


//
// 2-D tiling simplex noise with rotating gradients and analytical derivative.
// The first component of the 3-element return vector is the noise value,
// and the second and third components are the x and y partial derivatives.
//
vec3 _webglnoise_psrdnoise(vec2 pos, vec2 per, float rot) 
{
 // Hack: offset y slightly to hide some rare artifacts
	pos.y += 0.01;
 // Skew to hexagonal grid
 	vec2 uv = vec2(pos.x + pos.y*0.5, pos.y);
  
	vec2 i0 = floor(uv);
	vec2 f0 = fract(uv);
 // Traversal order
	vec2 i1 = (f0.x > f0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);

 // Unskewed grid points in (x,y) space
	vec2 p0 = vec2(i0.x - i0.y * 0.5, i0.y);
	vec2 p1 = vec2(p0.x + i1.x - i1.y * 0.5, p0.y + i1.y);
	vec2 p2 = vec2(p0.x + 0.5, p0.y + 1.0);

 // Integer grid point indices in (u,v) space
	i1 = i0 + i1;
	vec2 i2 = i0 + vec2(1.0, 1.0);

 // Vectors in unskewed (x,y) coordinates from each of the simplex corners to the evaluation point
	vec2 d0 = pos - p0;
	vec2 d1 = pos - p1;
	vec2 d2 = pos - p2;

 // Wrap i0, i1 and i2 to the desired period before gradient hashing: wrap points in (x,y), map to (u,v)
	vec3 xw = mod(vec3(p0.x, p1.x, p2.x), per.x);
	vec3 yw = mod(vec3(p0.y, p1.y, p2.y), per.y);
	vec3 iuw = xw + 0.5 * yw;
	vec3 ivw = yw;
  
 // Create gradients from indices
	vec2 g0 = _webglnoise_rgrad2(vec2(iuw.x, ivw.x), rot);
	vec2 g1 = _webglnoise_rgrad2(vec2(iuw.y, ivw.y), rot);
	vec2 g2 = _webglnoise_rgrad2(vec2(iuw.z, ivw.z), rot);

  // Gradients dot vectors to corresponding corners (the derivatives of this are simply the gradients)
	vec3 w = vec3(dot(g0, d0), dot(g1, d1), dot(g2, d2));
  
 // Radial weights from corners
 // 0.8 is the square of 2/sqrt(5), the distance from a grid point to the nearest simplex boundary
	vec3 t = 0.8 - vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2));

 // Partial derivatives for analytical gradient computation
	vec3 dtdx = -2.0 * vec3(d0.x, d1.x, d2.x);
	vec3 dtdy = -2.0 * vec3(d0.y, d1.y, d2.y);

 // Set influence of each surflet to zero outside radius sqrt(0.8)
	if (t.x < 0.0) 
	{
		dtdx.x = 0.0;
		dtdy.x = 0.0;
		t.x = 0.0;
  	}
	if (t.y < 0.0) 
	{
		dtdx.y = 0.0;
		dtdy.y = 0.0;
		t.y = 0.0;
	}
	if (t.z < 0.0) 
	{
		dtdx.z = 0.0;
		dtdy.z = 0.0;
		t.z = 0.0;
	}

 // Fourth power of t (and third power for derivative)
	vec3 t2 = t * t;
	vec3 t4 = t2 * t2;
	vec3 t3 = t2 * t;
  
 // Final noise value is: sum of ((radial weights) times (gradient dot vector from corner))
	float n = dot(t4, w);
  
 // Final analytical derivative (gradient of a sum of scalar products)
	vec2 dt0 = vec2(dtdx.x, dtdy.x) * 4.0 * t3.x;
	vec2 dn0 = t4.x * g0 + dt0 * w.x;
	vec2 dt1 = vec2(dtdx.y, dtdy.y) * 4.0 * t3.y;
	vec2 dn1 = t4.y * g1 + dt1 * w.y;
	vec2 dt2 = vec2(dtdx.z, dtdy.z) * 4.0 * t3.z;
	vec2 dn2 = t4.z * g2 + dt2 * w.z;

	return 11.0 * vec3(n, dn0 + dn1 + dn2);
}

//
// 2-D tiling simplex noise with rotating gradients,
// but without the analytical derivative.
//
float _webglnoise_psrnoise(vec2 pos, vec2 per, float rot) 
{
 // Offset y slightly to hide some rare artifacts
	pos.y += 0.001;
 // Skew to hexagonal grid
	vec2 uv = vec2(pos.x + pos.y*0.5, pos.y);
  
	vec2 i0 = floor(uv);
	vec2 f0 = fract(uv);
 // Traversal order
	vec2 i1 = (f0.x > f0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);

 // Unskewed grid points in (x,y) space
	vec2 p0 = vec2(i0.x - i0.y * 0.5, i0.y);
	vec2 p1 = vec2(p0.x + i1.x - i1.y * 0.5, p0.y + i1.y);
	vec2 p2 = vec2(p0.x + 0.5, p0.y + 1.0);

 // Integer grid point indices in (u,v) space
	i1 = i0 + i1;
	vec2 i2 = i0 + vec2(1.0, 1.0);

 // Vectors in unskewed (x,y) coordinates from each of the simplex corners to the evaluation point
	vec2 d0 = pos - p0;
	vec2 d1 = pos - p1;
	vec2 d2 = pos - p2;

 // Wrap i0, i1 and i2 to the desired period before gradient hashing: wrap points in (x,y), map to (u,v)
	vec3 xw = mod(vec3(p0.x, p1.x, p2.x), per.x);
	vec3 yw = mod(vec3(p0.y, p1.y, p2.y), per.y);
	vec3 iuw = xw + 0.5 * yw;
	vec3 ivw = yw;
  
 // Create gradients from indices
	vec2 g0 = _webglnoise_rgrad2(vec2(iuw.x, ivw.x), rot);
	vec2 g1 = _webglnoise_rgrad2(vec2(iuw.y, ivw.y), rot);
	vec2 g2 = _webglnoise_rgrad2(vec2(iuw.z, ivw.z), rot);

 // Gradients dot vectors to corresponding corners (the derivatives of this are simply the gradients)
	vec3 w = vec3(dot(g0, d0), dot(g1, d1), dot(g2, d2));
  
 // Radial weights from corners
 // 0.8 is the square of 2/sqrt(5), the distance from a grid point to the nearest simplex boundary
	vec3 t = 0.8 - vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2));

 // Set influence of each surflet to zero outside radius sqrt(0.8)
	t = max(t, 0.0);

 // Fourth power of t
	vec3 t2 = t * t;
	vec3 t4 = t2 * t2;
  
 // Final noise value is: sum of ((radial weights) times (gradient dot vector from corner))
	float n = dot(t4, w);
  
 // Rescale to cover the range [-1,1] reasonably well
	return 11.0 * n;
}


//
// 2-D non-tiling simplex noise with rotating gradients and analytical derivative.
// The first component of the 3-element return vector is the noise value,
// and the second and third components are the x and y partial derivatives.
//
vec3 _webglnoise_srdnoise(vec2 pos, float rot) 
{
 // Offset y slightly to hide some rare artifacts
	pos.y += 0.001;
 // Skew to hexagonal grid
	vec2 uv = vec2(pos.x + pos.y*0.5, pos.y);
  
	vec2 i0 = floor(uv);
	vec2 f0 = fract(uv);
 // Traversal order
	vec2 i1 = (f0.x > f0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);

 // Unskewed grid points in (x,y) space
	vec2 p0 = vec2(i0.x - i0.y * 0.5, i0.y);
	vec2 p1 = vec2(p0.x + i1.x - i1.y * 0.5, p0.y + i1.y);
	vec2 p2 = vec2(p0.x + 0.5, p0.y + 1.0);

 // Integer grid point indices in (u,v) space
	i1 = i0 + i1;
	vec2 i2 = i0 + vec2(1.0, 1.0);

 // Vectors in unskewed (x,y) coordinates from each of the simplex corners to the evaluation point
	vec2 d0 = pos - p0;
	vec2 d1 = pos - p1;
	vec2 d2 = pos - p2;

	vec3 x = vec3(p0.x, p1.x, p2.x);
	vec3 y = vec3(p0.y, p1.y, p2.y);
	vec3 iuw = x + 0.5 * y;
	vec3 ivw = y;
  
 // Avoid precision issues in permutation
	iuw = _webglnoise_mod289(iuw);
	ivw = _webglnoise_mod289(ivw);

 // Create gradients from indices
	vec2 g0 = _webglnoise_rgrad2(vec2(iuw.x, ivw.x), rot);
	vec2 g1 = _webglnoise_rgrad2(vec2(iuw.y, ivw.y), rot);
	vec2 g2 = _webglnoise_rgrad2(vec2(iuw.z, ivw.z), rot);

  // Gradients dot vectors to corresponding corners (the derivatives of this are simply the gradients)
	vec3 w = vec3(dot(g0, d0), dot(g1, d1), dot(g2, d2));
  
 // Radial weights from corners
 // 0.8 is the square of 2/sqrt(5), the distance from a grid point to the nearest simplex boundary
	vec3 t = 0.8 - vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2));

 // Partial derivatives for analytical gradient computation
	vec3 dtdx = -2.0 * vec3(d0.x, d1.x, d2.x);
	vec3 dtdy = -2.0 * vec3(d0.y, d1.y, d2.y);

 // Set influence of each surflet to zero outside radius sqrt(0.8)
	if (t.x < 0.0) 
	{
		dtdx.x = 0.0;
		dtdy.x = 0.0;
		t.x = 0.0;
	}
	if (t.y < 0.0) 
	{
		dtdx.y = 0.0;
		dtdy.y = 0.0;
		t.y = 0.0;
	}
	if (t.z < 0.0) 
	{
		dtdx.z = 0.0;
		dtdy.z = 0.0;
		t.z = 0.0;
	}

 // Fourth power of t (and third power for derivative)
	vec3 t2 = t * t;
	vec3 t4 = t2 * t2;
	vec3 t3 = t2 * t;
  
 // Final noise value is: sum of ((radial weights) times (gradient dot vector from corner))
	float n = dot(t4, w);
  
 // Final analytical derivative (gradient of a sum of scalar products)
	vec2 dt0 = vec2(dtdx.x, dtdy.x) * 4.0 * t3.x;
	vec2 dn0 = t4.x * g0 + dt0 * w.x;
	vec2 dt1 = vec2(dtdx.y, dtdy.y) * 4.0 * t3.y;
	vec2 dn1 = t4.y * g1 + dt1 * w.y;
	vec2 dt2 = vec2(dtdx.z, dtdy.z) * 4.0 * t3.z;
	vec2 dn2 = t4.z * g2 + dt2 * w.z;

	return 11.0 * vec3(n, dn0 + dn1 + dn2);
}


//
// 2-D non-tiling simplex noise with rotating gradients,
// without the analytical derivative.
//
float _webglnoise_srnoise(vec2 pos, float rot)
{
 // Offset y slightly to hide some rare artifacts
	pos.y += 0.001;
 // Skew to hexagonal grid
	vec2 uv = vec2(pos.x + pos.y*0.5, pos.y);
  
	vec2 i0 = floor(uv);
	vec2 f0 = fract(uv);
 // Traversal order
	vec2 i1 = (f0.x > f0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);

 // Unskewed grid points in (x,y) space
	vec2 p0 = vec2(i0.x - i0.y * 0.5, i0.y);
	vec2 p1 = vec2(p0.x + i1.x - i1.y * 0.5, p0.y + i1.y);
	vec2 p2 = vec2(p0.x + 0.5, p0.y + 1.0);

 // Integer grid point indices in (u,v) space
	i1 = i0 + i1;
	vec2 i2 = i0 + vec2(1.0, 1.0);

 // Vectors in unskewed (x,y) coordinates from each of the simplex corners to the evaluation point
	vec2 d0 = pos - p0;
	vec2 d1 = pos - p1;
	vec2 d2 = pos - p2;

 // Wrap i0, i1 and i2 to the desired period before gradient hashing: wrap points in (x,y), map to (u,v)
	vec3 x = vec3(p0.x, p1.x, p2.x);
	vec3 y = vec3(p0.y, p1.y, p2.y);
	vec3 iuw = x + 0.5 * y;
	vec3 ivw = y;
  
 // Avoid precision issues in permutation
	iuw = _webglnoise_mod289(iuw);
	ivw = _webglnoise_mod289(ivw);

 // Create gradients from indices
	vec2 g0 = _webglnoise_rgrad2(vec2(iuw.x, ivw.x), rot);
	vec2 g1 = _webglnoise_rgrad2(vec2(iuw.y, ivw.y), rot);
	vec2 g2 = _webglnoise_rgrad2(vec2(iuw.z, ivw.z), rot);

  // Gradients dot vectors to corresponding corners (the derivatives of this are simply the gradients)
	vec3 w = vec3(dot(g0, d0), dot(g1, d1), dot(g2, d2));
  
 // Radial weights from corners
 // 0.8 is the square of 2/sqrt(5), the distance from a grid point to the nearest simplex boundary
	vec3 t = 0.8 - vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2));

 // Set influence of each surflet to zero outside radius sqrt(0.8)
	t = max(t, 0.0);

 // Fourth power of t
	vec3 t2 = t * t;
	vec3 t4 = t2 * t2;
  
 // Final noise value is: sum of ((radial weights) times (gradient dot vector from corner))
	float n = dot(t4, w);
  
 // Rescale to cover the range [-1,1] reasonably well
	return 11.0 * n;
}
