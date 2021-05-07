#pragma once

#include <frm/core/AppSample3d.h>
#include <frm/core/String.h>

using namespace frm;

typedef AppSample3d AppBase;

class MeshViewer: public AppBase
{
public:
	MeshViewer();
	virtual ~MeshViewer();

	virtual bool init(const ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	enum _OverlayMode
	{
		Mode_None,
		Mode_Normals,
		Mode_Tangents,
		Mode_Colors,
		Mode_MaterialUVs,
		Mode_LightmapUVs,
		Mode_BoneWeights,
		Mode_BoneIndices,

		Mode_Count
	};
	using OverlayMode = int;
	static const char* kOverlayModeStr[Mode_Count];

	BasicRenderer*            m_basicRenderer    = nullptr;
	PathStr                   m_meshPath         = "models/Box1.gltf";
	PathStr                   m_materialPath     = "materials/BasicMaterial.mat";
	PathStr                   m_environmentPath  = "textures/env_lightgray.exr";
	DrawMesh*                 m_mesh             = nullptr;
	BasicRenderableComponent* m_renderable       = nullptr;
	BasicMaterial*            m_material         = nullptr;
	ImageLightComponent*      m_environment      = nullptr;
	OrbitLookComponent*       m_cameraController = nullptr;
	Camera*                   m_camera           = nullptr;
	Shader*                   m_shOverlay        = nullptr;
	Shader*                   m_shWireframe      = nullptr;

	int                       m_overlayMode      = Mode_None;
	float                     m_overlayAlpha     = 0.75f;
	bool                      m_wireframe        = true;
	int                       m_lodOverride      = -1;
	int                       m_submeshOverride  = -1;


	void resetCamera();
	bool initScene();
	bool initMeshMaterial();
};
