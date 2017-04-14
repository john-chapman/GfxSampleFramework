#include <frm/MeshData.h>

#include <apt/TextParser.h>

#include <extern/md5mesh.h>

#include <EASTL/sort.h>
#include <EASTL/vector.h>

using namespace frm;
using namespace apt;

bool MeshData::ReadMd5(MeshData& mesh_, const char* _srcData, uint _srcDataSize)
{
	TextParser tp(_srcData);

	long int numJoints = -1;
	long int numMeshes = -1;
	md5_call(md5::ParseMeshHeader(tp, &numJoints, &numMeshes));
	
	eastl::vector<md5::MeshJoint> joints;
	joints.resize(numJoints);
	eastl::vector<md5::Mesh> meshes;
	meshes.resize(numMeshes);

	MeshBuilder tmpMesh;
	Skeleton tmpSkeleton;
	bool ret = true;
	
	long int meshCount = 0;
	while (!tp.isNull()) {
		if (tp.compareNext("joints")) {
			md5_call(md5::ParseMeshJointList(tp, numJoints, joints.data()));
			continue;
		}
		
		if (tp.compareNext("mesh")) {
			if (meshCount > numMeshes) {
				md5_err("too many meshes, expected %d", numMeshes);
			}
			md5_call(md5::ParseMesh(tp, meshes[meshCount++]));
			continue;
		}
	}

	for (auto& src : joints) {
		int i = tmpSkeleton.addBone(src.m_name, src.m_parentIndex);
		Skeleton::Bone& bone = tmpSkeleton.getBone(i);
		bone.m_scale = vec3(1.0f);
		bone.m_position = src.m_position;
		bone.m_orientation = src.m_orientation;
		bone.m_parentIndex = -1;// src.m_parentIndex; // set -1 to prevent resolve applying parent transforms
	}
	tmpSkeleton.resolve();
	for (int i = 0; i < tmpSkeleton.getBoneCount(); ++i) {
		tmpSkeleton.getPose()[i] = affineInverse(tmpSkeleton.getPose()[i]);
	}

	for (auto& mesh : meshes) {
		tmpMesh.beginSubmesh(0); // \todo material id?

		uint32 vertOffset = tmpMesh.getVertexCount();
		tmpMesh.setVertexCount(vertOffset + (uint32)mesh.m_verts.size());
		MeshBuilder::Vertex* vdst = &tmpMesh.getVertex(vertOffset);
		for (auto& vsrc : mesh.m_verts) {
			memset(vdst, 0, sizeof(MeshBuilder::Vertex));
			vdst->m_texcoord = vsrc.m_texcoord;

		 // construct a sorted list of weights
			eastl::vector<md5::Weight> weights;
			for (long int i = 0; i < vsrc.m_weightCount; ++i) {
				weights.push_back(mesh.m_weights[vsrc.m_weightStart + i]);
			}
			eastl::sort(weights.begin(), weights.end(), [](const md5::Weight& _a, const md5::Weight& _b) { return _b.m_bias < _a.m_bias; });
		 
		 // copy the first 4 weights/indices into the vertex and normalize
			float weightSum = 0.0f;
			for (int i = 0, n = APT_MIN(4, (int)weights.size()); i < n; ++i) {
				vdst->m_boneIndices[i] = weights[i].m_jointIndex;
				vdst->m_boneWeights[i] = weights[i].m_bias;
				weightSum += weights[i].m_bias;
			}
			vdst->m_boneWeights /= weightSum;

		 // construct object space vertex position
			vdst->m_position = vec3(0.0f);
			for (auto& weight : weights) {
				auto& joint = joints[weight.m_jointIndex];
				vec3 posJ = joint.m_orientation * weight.m_position;
				vdst->m_position += (joint.m_position + posJ) * weight.m_bias;
			}

			++vdst;
		}

		uint32 triOffset = tmpMesh.getTriangleCount();
		tmpMesh.setTriangleCount(tmpMesh.getTriangleCount() + (uint32)mesh.m_tris.size());
		MeshBuilder::Triangle* tdst = &tmpMesh.getTriangle(triOffset);
		for (auto& tsrc : mesh.m_tris) {
			//for (int i = 0; i < 3; ++i) {
			//	(*tdst)[i] = tsrc.m_verts[i] + vertOffset;
			//}
		 // \todo winding inverted?
			tdst->a = tsrc.m_verts[0] + vertOffset;
			tdst->b = tsrc.m_verts[2] + vertOffset;
			tdst->c = tsrc.m_verts[1] + vertOffset;
			
			++tdst;
		}

		tmpMesh.endSubmesh();
	}

	tmpMesh.generateNormals();
	tmpMesh.generateTangents();
	tmpMesh.updateBounds();

 // \todo use _mesh desc as a conversion target
	if (ret) {
		MeshDesc retDesc(MeshDesc::Primitive_Triangles);
		retDesc.addVertexAttr(VertexAttr::Semantic_Positions,   DataType::Float32, 3);
		retDesc.addVertexAttr(VertexAttr::Semantic_Normals,     DataType::Sint8N,  3);
		retDesc.addVertexAttr(VertexAttr::Semantic_Tangents,    DataType::Sint8N,  3);
		retDesc.addVertexAttr(VertexAttr::Semantic_Texcoords,   DataType::Uint16N, 2);
		retDesc.addVertexAttr(VertexAttr::Semantic_BoneWeights, DataType::Uint16N, 4);
		retDesc.addVertexAttr(VertexAttr::Semantic_BoneIndices, DataType::Uint8,   4);
		MeshData retMesh(retDesc, tmpMesh);
		retMesh.setBindPose(tmpSkeleton);
		swap(mesh_, retMesh);
	}

	return true;
}
