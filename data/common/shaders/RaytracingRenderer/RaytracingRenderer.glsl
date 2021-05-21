struct RayHit
{
	vec3  position;
	float hitDistance;
	vec3  normal;
	uint  triangleIndex;
	vec2  barycentrics;
	uint  isHit;
	uint  rayID;
	uvec2 instance; // CPU ptr, 64 bits.
	uvec2 meshData; // "
};
layout(std430) restrict readonly buffer bfRayHits
{
	RayHit uRayHits[];
};

struct VertexData
{
	vec2 materialUV;
	vec2 lightmapUV;
	vec3 normal;
};
layout(std430) restrict readonly buffer bfVertexData
{
	VertexData uVertexData[];
};

layout(std430) restrict readonly buffer bfIndexData
{
	uint uTriangles[]; // \todo Can't alias array of uint as uvec3 due to layout requirements.
};

uvec3 RaytracingRenderer_GetTriangle(in uint _triangleIndex)
{
	return uvec3(
		uTriangles[_triangleIndex * 3 + 0], 
		uTriangles[_triangleIndex * 3 + 1], 
		uTriangles[_triangleIndex * 3 + 2]
		);
}

vec3 RaytracingRenderer_GetBarycentrics(in RayHit _rayHit)
{
	return vec3(1.0 - _rayHit.barycentrics.x - _rayHit.barycentrics.y, _rayHit.barycentrics.x, _rayHit.barycentrics.y);
}

vec3 RaytracingRenderer_InterpolateNormal(in RayHit _rayHit)
{
	const uvec3 tri = RaytracingRenderer_GetTriangle(_rayHit.triangleIndex);
	const vec3 bc = RaytracingRenderer_GetBarycentrics(_rayHit);
	return uVertexData[tri.x].normal * bc.x + uVertexData[tri.y].normal * bc.y + uVertexData[tri.z].normal * bc.z;
}

vec2 RaytracingRenderer_InterpolateMaterialUV(in RayHit _rayHit)
{
	const uvec3 tri = RaytracingRenderer_GetTriangle(_rayHit.triangleIndex);
	const vec3 bc = RaytracingRenderer_GetBarycentrics(_rayHit);
	return uVertexData[tri.x].materialUV * bc.x + uVertexData[tri.y].materialUV * bc.y + uVertexData[tri.z].materialUV * bc.z;
}

vec2 RaytracingRenderer_InterpolateLightmapUV(in RayHit _rayHit)
{
	const uvec3 tri = RaytracingRenderer_GetTriangle(_rayHit.triangleIndex);
	const vec3 bc = RaytracingRenderer_GetBarycentrics(_rayHit);
	return uVertexData[tri.x].lightmapUV * bc.x + uVertexData[tri.y].lightmapUV * bc.y + uVertexData[tri.z].lightmapUV * bc.z;
}