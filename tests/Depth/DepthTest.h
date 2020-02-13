#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class DepthTest: public AppBase
{
public:
	DepthTest();
	virtual ~DepthTest();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:
		
	enum DepthFormat_
	{
		DepthFormat_16,
		DepthFormat_24,
		DepthFormat_32,
		DepthFormat_32F,

		DepthFormat_Count,
		DepthFormat_Default = DepthFormat_32F
	};
	typedef int DepthFormat;

	DepthFormat       m_depthFormat         = DepthFormat_Default;
	frm::Texture*     m_txDepth             = nullptr;
	frm::Texture*     m_txColor             = nullptr;
	frm::Framebuffer* m_fbDepth             = nullptr;
	frm::Framebuffer* m_fbDepthColor        = nullptr;
	frm::Texture*     m_txRadar             = nullptr;
	frm::Shader*      m_shDepthOnly         = nullptr;
	frm::Shader*      m_shDepthError        = nullptr;
	frm::Buffer*      m_bfInstances         = nullptr;
	frm::Mesh*        m_mesh                = nullptr;
	int               m_instanceCount       = 64;
	float             m_maxError            = 1e-3f;
	bool              m_reconstructPosition = false;

	bool initShaders();
	void shutdownShaders();

	bool initTextures();
	void shutdownTextures();

};
