#include "VRContext.h"

#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/Buffer.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Mesh.h>
#include <frm/core/Shader.h>
#include <frm/core/String.h>
#include <frm/core/Texture.h>

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>

#include <EASTL/fixed_vector.h>

#include <cstring> // memset

#define ENABLE_OVR_LOG 1

namespace {

ovrErrorInfo s_errInfo;

const char* OVRErrorString()
{
	ovr_GetLastErrorInfo(&s_errInfo);
	return s_errInfo.ErrorString;
}

ovrResult OVRErrorResult()
{
	ovr_GetLastErrorInfo(&s_errInfo);
	return s_errInfo.Result;
}

#define ovrAssert(_call) \
	if (OVR_FAILURE(_call)) \
	{ \
		FRM_LOG_ERR("OVR_ASSERT (%s, line %d)\n\t'%s' %s", frm::internal::StripPath(__FILE__), __LINE__, #_call, OVRErrorString()); \
		FRM_BREAK(); \
	}


inline frm::vec3 OVRVec3ToVec3(const ovrVector3f& _v)
{
	return frm::vec3(_v.x, _v.y, _v.z);
}

inline frm::quat OVRQuatToQuat(const ovrQuatf& _q)
{
	return frm::quat(_q.x, _q.y, _q.z, _q.w);
}

inline frm::mat4 OVRMatrixToMat4(const ovrMatrix4f& _mat)
{
	return frm::mat4(
		_mat.M[0][0], _mat.M[1][0], _mat.M[2][0], _mat.M[3][0],
		_mat.M[0][1], _mat.M[1][1], _mat.M[2][1], _mat.M[3][1],
		_mat.M[0][2], _mat.M[1][2], _mat.M[2][2], _mat.M[3][2],
		_mat.M[0][3], _mat.M[1][3], _mat.M[2][3], _mat.M[3][3]
		);
}

inline frm::mat4 OVRPoseToMat4(const ovrPosef& _pose)
{
	const ovrVector3f& p = _pose.Position;
	const ovrQuatf&    q = _pose.Orientation;
	return frm::TransformationMatrix(OVRVec3ToVec3(p), OVRQuatToQuat(q)); 
}

void OVRPoseStateToPoseData(frm::VRContext::PoseData& out_, const ovrPoseStatef& _state, const frm::mat4& _userTransform)
{
	out_.pose                = _userTransform * OVRPoseToMat4(_state.ThePose);
	out_.linearVelocity      = frm::TransformDirection(_userTransform, OVRVec3ToVec3(_state.LinearVelocity));
	out_.linearAcceleration  = frm::TransformDirection(_userTransform, OVRVec3ToVec3(_state.LinearAcceleration));

	// Angular data is reported by the Oculus SDK in pose-local space, hence we convert to world space.
	out_.angularVelocity     = frm::TransformDirection(out_.pose, OVRVec3ToVec3(_state.AngularVelocity));
	out_.angularAcceleration = frm::TransformDirection(out_.pose, OVRVec3ToVec3(_state.AngularAcceleration));
}

void OVR_CDECL OVRLogCallback(uintptr_t _userData, int _level, const char* _message)
{
	#if ENABLE_OVR_LOG
		switch (_level)
		{
			default:
			case ovrLogLevel_Info:
				FRM_LOG("OVR: %s", _message);
				break;
			case ovrLogLevel_Debug:
				FRM_LOG_DBG("OVR: %s", _message);
				break;
			case ovrLogLevel_Error:
				FRM_LOG_ERR("OVR: %s", _message);
				break;
		};
	#endif
}

struct OVRLayer
{
	ovrLayerEyeFov                            m_ovrLayer;
	ovrTextureSwapChain                       m_ovrSwapchain;
	int                                       m_swapchainLength;
	int                                       m_currentSwapchainIndex;
	eastl::fixed_vector<frm::Texture*, 3>     m_txSwapchain;
	eastl::fixed_vector<frm::Framebuffer*, 3> m_fbSwapchain;

	frm::Viewport getViewport(int _eye) const
	{
		const ovrRecti& v = m_ovrLayer.Viewport[_eye];
		return { v.Pos.x, v.Pos.y, v.Size.w, v.Size.h };
	}

};

} // namespace

namespace frm {

/*******************************************************************************

                              VRContext::Impl

*******************************************************************************/

static_assert((int)ovrEye_Left   == VRContext::Eye_Left,   "ovrEye_Left != VRContext::Eye_Left");
static_assert((int)ovrEye_Right  == VRContext::Eye_Right,  "ovrEye_Right != VRContext::Eye_Right");
static_assert((int)ovrHand_Left  == VRContext::Hand_Left,  "ovrHand_Left != VRContext::Hand_Left");
static_assert((int)ovrHand_Right == VRContext::Hand_Right, "ovrHand_Right != VRContext::Hand_Right");

struct VRContext::Impl
{
	ovrSession          m_ovrSession;
	ovrSessionStatus    m_ovrSessionStatus;
	ovrGraphicsLuid     m_ovrGraphicsLUID;
	ovrHmdDesc          m_ovrHmdDesc;
	ovrTrackerDesc      m_ovrTrackerDesc[4];
	ovrEyeRenderDesc    m_ovrEyeDesc[ovrEye_Count];

	OVRLayer            m_layers[Layer_Count];
	ovrMirrorTexture    m_ovrMirrorTexture;

 	ovrPosef            m_ovrEyePoses[ovrEye_Count];
	ovrTrackingState    m_ovrTrackingState;

	vec2                m_stencilRect[ovrEye_Count][4]; // Viewport-relative stencil rect, 4 verts in [0,1].
	Mesh*               m_msNonVisible[ovrEye_Count];   // Non-visible area stencil mesh.

	     Impl();
	     ~Impl();

	bool isInit();

	bool initSwapChain();
	bool initStencilMeshes();

	void shutdownSwapChain();
	void shutdownStencilMesh();

};

VRContext::Impl::Impl()
{
	memset(this, 0, sizeof(Impl));
}

VRContext::Impl::~Impl()
{
	FRM_ASSERT_MSG(m_ovrSession == nullptr, "VRContext::Impl: shtudown() not called");
}

bool VRContext::Impl::isInit()
{
	return m_ovrSession != nullptr;
}

bool VRContext::Impl::initSwapChain()
{
	FRM_ASSERT(Eye_Count == 2); // \todo swapchain init makes assumptions based on 2 eye buffers

	bool ret = true;

	for (int layerIndex = 0; layerIndex < Layer_Count; ++layerIndex)
	{
		OVRLayer& layer = m_layers[layerIndex];

		FRM_ASSERT_MSG(layer.m_txSwapchain.empty() && layer.m_fbSwapchain.empty(), "VRContext::Impl: initSwapchain() already called");

		layer.m_ovrLayer.Header.Type  = ovrLayerType_EyeFov;
		layer.m_ovrLayer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft; // OpenGL
		switch ((Layer)layerIndex)
		{
			default:
			case Layer_Main:
				break;
			case Layer_Text:
				layer.m_ovrLayer.Header.Flags |= ovrLayerFlag_HighQuality;
				break;
		};
		
	// Compute swapchain size.
		ovrSizei renderSize = { 0, 0 };
		for (int eyeIndex = 0; eyeIndex < Eye_Count; ++eyeIndex)
		{
			ovrSizei eyeRenderSize = ovr_GetFovTextureSize(m_ovrSession, (ovrEyeType)eyeIndex, m_ovrHmdDesc.DefaultEyeFov[eyeIndex], 1.f); // \todo configurable pixel density per layer?
			renderSize.w = Max(renderSize.w, eyeRenderSize.w);
			renderSize.h = Max(renderSize.h, eyeRenderSize.h);
		}
		renderSize.w *= Eye_Count;
		const int maxMipCount = Min(Texture::GetMaxMipCount(renderSize.w, renderSize.h), 8);

	// Allocate swapchian textures.
		ovrTextureSwapChainDesc desc = {};
		desc.Format                  = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.Type                    = ovrTexture_2D;
		desc.Width                   = renderSize.w;
		desc.Height                  = renderSize.h;
		desc.ArraySize               = 1;
		desc.SampleCount             = 1;
		desc.MipLevels               = (layerIndex == Layer_Text) ? maxMipCount : 1;
		desc.StaticImage             = ovrFalse;

		if (OVR_FAILURE(ovr_CreateTextureSwapChainGL(m_ovrSession, &desc, &layer.m_ovrSwapchain)))
		{
			FRM_LOG_ERR("ovr_CreateTextureSwapChainGL: %s", OVRErrorString());
			ret = false;
		}
		ovrAssert(ovr_GetTextureSwapChainLength(m_ovrSession, layer.m_ovrSwapchain, &layer.m_swapchainLength));

	// Alias swapchain textures with framework types.
		layer.m_txSwapchain.resize(layer.m_swapchainLength);
		layer.m_fbSwapchain.resize(layer.m_swapchainLength);
		for (int chainIndex = 0; chainIndex < layer.m_swapchainLength; ++chainIndex)
		{
			GLuint txH;
			ovrAssert(ovr_GetTextureSwapChainBufferGL(m_ovrSession, layer.m_ovrSwapchain, chainIndex, &txH));
			layer.m_txSwapchain[chainIndex] = Texture::CreateProxy(txH, String<32>("#VR_SWAPCHAIN_Layer[%d][%d]", layerIndex, chainIndex).c_str());
			layer.m_fbSwapchain[chainIndex] = Framebuffer::Create(1, layer.m_txSwapchain[chainIndex]);
		}
		layer.m_currentSwapchainIndex = 0;

	// Set viewports per eye.
		const int halfRenderSizeW = renderSize.w / 2;
		layer.m_ovrLayer.ColorTexture[ovrEye_Left]   = layer.m_ovrSwapchain;
		layer.m_ovrLayer.Viewport[ovrEye_Left].Pos   = { 0, 0 };
		layer.m_ovrLayer.Viewport[ovrEye_Left].Size  = { halfRenderSizeW, renderSize.h };
		layer.m_ovrLayer.ColorTexture[ovrEye_Right]  = nullptr;
		layer.m_ovrLayer.Viewport[ovrEye_Right].Pos  = { halfRenderSizeW, 0 };
		layer.m_ovrLayer.Viewport[ovrEye_Right].Size = { halfRenderSizeW, renderSize.h };
	}

	// \todo init mirror texture

	return ret;
}

bool VRContext::Impl::initStencilMeshes()
{
	bool ret = true;

	MeshDesc meshDesc;
	meshDesc.addVertexAttr(VertexAttr::Semantic_Positions, DataType_Float, 2);

	for (int eyeIndex = 0; eyeIndex < ovrEye_Count; ++eyeIndex)
	{
		ovrFovStencilMeshBuffer ovrMeshData = {};
		ovrFovStencilDesc ovrMeshDesc = {};
		ovrMeshDesc.Eye               = (ovrEyeType)eyeIndex;
		ovrMeshDesc.FovPort           = ovr_GetRenderDesc(m_ovrSession, (ovrEyeType)eyeIndex, m_ovrHmdDesc.DefaultEyeFov[eyeIndex]).Fov;
		ovrMeshDesc.StencilFlags      = ovrFovStencilFlag_MeshOriginAtBottomLeft; // OpenGL

		// Get visible rect.
		uint16_t unused[6];
		ovrMeshDesc.StencilType = ovrFovStencil_VisibleRectangle;
		ovrMeshData.VertexBuffer = (ovrVector2f*)m_stencilRect[eyeIndex];
		ovrMeshData.AllocVertexCount = 4;
		ovrMeshData.IndexBuffer = unused;
		ovrMeshData.AllocIndexCount = 6;
		ovrAssert(ovr_GetFovStencil(m_ovrSession, &ovrMeshDesc, &ovrMeshData));

		// Get non-visible mesh.
		ovrMeshData = {};
		ovrMeshDesc.StencilType = ovrFovStencil_HiddenArea;		
		ovrAssert(ovr_GetFovStencil(m_ovrSession, &ovrMeshDesc, &ovrMeshData));
		ovrMeshData.VertexBuffer     = FRM_NEW_ARRAY(ovrVector2f, ovrMeshData.UsedVertexCount);
		ovrMeshData.AllocVertexCount = ovrMeshData.UsedVertexCount;
		ovrMeshData.IndexBuffer      = FRM_NEW_ARRAY(uint16_t, ovrMeshData.UsedIndexCount);
		ovrMeshData.AllocIndexCount  = ovrMeshData.UsedIndexCount;
		ovrAssert(ovr_GetFovStencil(m_ovrSession, &ovrMeshDesc, &ovrMeshData));

		// Convert mesh data from [0,1] -> [-1,1] range.
		for (int i = 0; i < ovrMeshData.AllocVertexCount; ++i)
		{
			ovrMeshData.VertexBuffer[i].x = ovrMeshData.VertexBuffer[i].x * 2.f - 1.f;
			ovrMeshData.VertexBuffer[i].y = ovrMeshData.VertexBuffer[i].y * 2.f - 1.f;
		}

		m_msNonVisible[eyeIndex] = Mesh::Create(meshDesc);
		m_msNonVisible[eyeIndex]->setVertexData(ovrMeshData.VertexBuffer, ovrMeshData.AllocVertexCount);
		m_msNonVisible[eyeIndex]->setIndexData(DataType_Uint16, ovrMeshData.IndexBuffer, ovrMeshData.AllocIndexCount);

		FRM_DELETE_ARRAY(ovrMeshData.VertexBuffer);	
		FRM_DELETE_ARRAY(ovrMeshData.IndexBuffer);
	}

	return ret;
}

void VRContext::Impl::shutdownSwapChain()
{
	for (int layerIndex = 0; layerIndex < Layer_Count; ++layerIndex)
	{
		OVRLayer& layer = m_layers[layerIndex];

		if (!layer.m_ovrSwapchain)
		{
			continue;
		}

		for (int chainIndex = 0; chainIndex < layer.m_swapchainLength; ++chainIndex)
		{
			Framebuffer::Destroy(layer.m_fbSwapchain[chainIndex]);
			Texture::Release(layer.m_txSwapchain[chainIndex]);
		}
		layer.m_fbSwapchain.clear();
		layer.m_txSwapchain.clear();

		ovr_DestroyTextureSwapChain(m_ovrSession, layer.m_ovrSwapchain);
	}
}

void VRContext::Impl::shutdownStencilMesh()
{
	for (int eyeIndex = 0; eyeIndex < ovrEye_Count; ++eyeIndex)
	{
		Mesh::Release(m_msNonVisible[eyeIndex]);
	}
}

/*******************************************************************************

                                 VRContext

*******************************************************************************/

static VRContext* s_current;

// PUBLIC

VRContext* VRContext::Create()
{
	VRContext* ret = FRM_NEW(VRContext);
	if (!ret->init())
	{
		FRM_DELETE(ret);
		return nullptr;
	}

	s_current = ret;

	return ret;
}

void VRContext::Destroy(VRContext*& _ctx_)
{
	if (_ctx_ == s_current)
	{
		s_current = nullptr;
	}

	_ctx_->shutdown();
	FRM_DELETE(_ctx_);
	_ctx_ = nullptr;
}

VRContext* VRContext::GetCurrent()
{
	return s_current;
}

bool VRContext::MakeCurrent(VRContext* _ctx)
{
	s_current = _ctx;
	return true;
}

bool VRContext::update(float _dt, const vec3& _userPosition, const quat& _userOrientation)
{
	ovrAssert(ovr_GetSessionStatus(m_impl->m_ovrSession, &m_impl->m_ovrSessionStatus));

	if (m_impl->m_ovrSessionStatus.ShouldQuit)
	{
		return false;
	}

	m_prevUserTransform = m_userTransform;
	m_userTransform.position = _userPosition;
	m_userTransform.orientation = _userOrientation;
	
	pollHmd(_dt); // \todo Better to poll HMD position as late as possible?
	pollInput(_dt);

	return true;
}

void VRContext::beginDraw()
{
	for (int layerIndex = 0; layerIndex < Layer_Count; ++layerIndex)
	{
		OVRLayer& layer = m_impl->m_layers[layerIndex];
		ovrAssert(ovr_GetTextureSwapChainCurrentIndex(m_impl->m_ovrSession, layer.m_ovrSwapchain, &layer.m_currentSwapchainIndex));

		#ifdef FRM_DEBUG
		{
			// Validate that the texture proxies are still valid.
			int swapchainLength;
			ovrAssert(ovr_GetTextureSwapChainLength(m_impl->m_ovrSession, layer.m_ovrSwapchain, &swapchainLength));
			FRM_ASSERT(layer.m_swapchainLength == swapchainLength);
			for (int swapchainIndex = 0; swapchainIndex < swapchainLength; ++swapchainIndex)
			{
				GLuint txH;
				ovrAssert(ovr_GetTextureSwapChainBufferGL(m_impl->m_ovrSession, layer.m_ovrSwapchain, swapchainIndex, &txH));
				FRM_ASSERT(layer.m_txSwapchain[swapchainIndex]->getHandle() == txH);
			}
		}	
		#endif
	}

	bool displayLost = false;

	ovrResult ret = ovr_WaitToBeginFrame(m_impl->m_ovrSession, m_frameIndex);
	displayLost |= ret == ovrError_DisplayLost;
	FRM_ASSERT(ret == ovrSuccess || ret == ovrError_DisplayLost);

	ret = ovr_BeginFrame(m_impl->m_ovrSession, m_frameIndex);
	displayLost |= ret == ovrError_DisplayLost;
	FRM_ASSERT(ret == ovrSuccess || ret == ovrError_DisplayLost);

	if (displayLost)
	{
		FRM_LOG("VRContext: Display lost, recreating.");
		shutdown();
		if (!init())
		{
			FRM_LOG("VRContext: Failed to re-init after display loss.");
			return;
		}
	}
}

void VRContext::endDraw()
{
	ovrLayerHeader* layerHeaders[Layer_Count];
	for (int layerIndex = 0; layerIndex < Layer_Count; ++layerIndex)
	{
		ovrAssert(ovr_CommitTextureSwapChain(m_impl->m_ovrSession, m_impl->m_layers[layerIndex].m_ovrSwapchain));

		layerHeaders[layerIndex] = &m_impl->m_layers[layerIndex].m_ovrLayer.Header;
	}
	ovrAssert(ovr_EndFrame(m_impl->m_ovrSession, m_frameIndex, nullptr, layerHeaders, (unsigned int)Layer_Count)); // \todo can lose device here
	++m_frameIndex;
}

Framebuffer* VRContext::getFramebuffer(Eye _eye, Layer _layer) const
{
	int i = m_impl->m_layers[(int)_layer].m_currentSwapchainIndex;
	return m_impl->m_layers[(int)_layer].m_fbSwapchain[i];
}

Viewport VRContext::getViewport(Eye _eye, Layer _layer) const
{
	return m_impl->m_layers[(int)_layer].getViewport((int)_eye);
}

Viewport VRContext::getStencilRect(Eye _eye, Layer _layer) const
{
	Viewport viewport = m_impl->m_layers[(int)_layer].getViewport((int)_eye);
	const vec2* stencilRect = m_impl->m_stencilRect[_eye];
	viewport.x += (int)(stencilRect[0].x * viewport.w);
	viewport.y += (int)(stencilRect[0].y * viewport.h);
	viewport.w  = (int)((stencilRect[1].x - stencilRect[0].x) * viewport.w);
	viewport.h  = (int)((stencilRect[2].y - stencilRect[0].y) * viewport.h);
	return viewport;
}

void VRContext::primeDepthBuffer(Eye _eye, float _depthValue)
{
	GlContext* ctx = GlContext::GetCurrent();
	
	glScopedEnable(GL_DEPTH_TEST, GL_TRUE);
	glAssert(glDepthFunc(GL_ALWAYS));
	glAssert(glClear(GL_DEPTH_BUFFER_BIT)); // \todo draw visible mesh at far depth to avoid clearing the whole buffer
	ctx->setShader(m_shPrimeDepth);
	ctx->setMesh(m_impl->m_msNonVisible[_eye]);
	ctx->setUniform("uClearDepth", _depthValue); 
	ctx->draw();

	/*glScopedEnable(GL_CULL_FACE, GL_FALSE);
	glScopedEnable(GL_DEPTH_TEST, GL_FALSE);
	glAssert(glDepthMask(GL_FALSE));
	ctx->setShader(m_shPrimeDepth);
	ctx->setMesh(m_impl->m_msNonVisible[_eye]);
	ctx->draw();
	glAssert(glDepthMask(GL_TRUE));*/
}

float VRContext::getHMDRefreshRate() const
{
	return m_impl->m_ovrHmdDesc.DisplayRefreshRate;
}

bool VRContext::isActive() const
{
	return m_impl->m_ovrSessionStatus.IsVisible && m_impl->m_ovrSessionStatus.HmdMounted;
}

bool VRContext::isFocused() const
{
	return m_impl->m_ovrSessionStatus.HasInputFocus;
}

// PRIVATE

bool VRContext::init()
{
	FRM_ASSERT(GlContext::GetCurrent());
	if (!GlContext::GetCurrent())
	{
		FRM_LOG_ERR("VRContext::init() no GlContext.");
		return false;
	}

	if (m_impl && m_impl->isInit())
	{
		shutdown();
	}
	else
	{
		m_impl = FRM_NEW(Impl);
	}

	ovrInitParams initParams = {};
	initParams.Flags = ovrInit_FocusAware;
	initParams.LogCallback = OVRLogCallback;
	if (OVR_FAILURE(ovr_Initialize(&initParams)))
	{
		FRM_LOG_ERR("ovr_Initialize: %s", OVRErrorString());
		return false;
	}

	if (OVR_FAILURE(ovr_Create(&m_impl->m_ovrSession, &m_impl->m_ovrGraphicsLUID)))
	{
		// \todo
		//if (OVRErrorResult() == ovrError_NoHmd)
		//{
		//	return true;
		//}

		FRM_LOG_ERR("ovr_Create: %s", OVRErrorString());

		return false;
	}

	bool ret = true;

	m_impl->m_ovrHmdDesc = ovr_GetHmdDesc(m_impl->m_ovrSession);
	switch (m_impl->m_ovrHmdDesc.Type)
	{
		case ovrHmd_RiftS:
		case ovrHmd_CV1:
			break;
		default:
			FRM_LOG_ERR("Invalid HMD (%u)", (unsigned)m_impl->m_ovrHmdDesc.Type);
			ret = false;
			break;
	};
	
	
	ovrAssert(ovr_SetTrackingOriginType(m_impl->m_ovrSession, ovrTrackingOrigin_FloorLevel)); // \todo config

	m_impl->m_ovrEyeDesc[ovrEye_Left]  = ovr_GetRenderDesc(m_impl->m_ovrSession, ovrEye_Left,  m_impl->m_ovrHmdDesc.DefaultEyeFov[ovrEye_Left]);
	m_impl->m_ovrEyeDesc[ovrEye_Right] = ovr_GetRenderDesc(m_impl->m_ovrSession, ovrEye_Right, m_impl->m_ovrHmdDesc.DefaultEyeFov[ovrEye_Right]);

	String<256> descStr;
	descStr.appendf("VR subsystem version: '%s'",  ovr_GetVersionString());
	descStr.appendf("\nHMD Info:");
	descStr.appendf("\n\tProduct Name:  '%s'",     m_impl->m_ovrHmdDesc.ProductName);
	descStr.appendf("\n\tManufacturer:  '%s'",     m_impl->m_ovrHmdDesc.Manufacturer);
	descStr.appendf("\n\tSerial Number:  %s",      m_impl->m_ovrHmdDesc.SerialNumber);
	descStr.appendf("\n\tFirmware:       %d.%d",   m_impl->m_ovrHmdDesc.FirmwareMajor, m_impl->m_ovrHmdDesc.FirmwareMinor);
	descStr.appendf("\n\tResolution:     %dx%d",   m_impl->m_ovrHmdDesc.Resolution.w, m_impl->m_ovrHmdDesc.Resolution.h);
	descStr.appendf("\n\tRefresh Rate:   %1.3fHz", m_impl->m_ovrHmdDesc.DisplayRefreshRate);
	FRM_LOG((const char*)descStr);

	if (ret)
	{
		ret &= m_impl->initSwapChain();
		ret &= m_impl->initStencilMeshes();
	}

	if (!ret)
	{
		shutdown();
		return false;
	}

	m_frameIndex = 0;

	for (int eyeIndex = 0; eyeIndex < Eye_Count; ++eyeIndex)
	{
		const ovrEyeRenderDesc& eyeDesc = m_impl->m_ovrEyeDesc[eyeIndex];

		m_eyeCameras[eyeIndex].m_up          =  eyeDesc.Fov.UpTan;
		m_eyeCameras[eyeIndex].m_down        = -eyeDesc.Fov.DownTan;
		m_eyeCameras[eyeIndex].m_right       =  eyeDesc.Fov.RightTan;
		m_eyeCameras[eyeIndex].m_left        = -eyeDesc.Fov.LeftTan;
		m_eyeCameras[eyeIndex].m_near        = 0.075f;
		m_eyeCameras[eyeIndex].m_far         = 1000.f;
		m_eyeCameras[eyeIndex].m_aspectRatio = fabs(m_eyeCameras[eyeIndex].m_right - m_eyeCameras[eyeIndex].m_left) / Abs(m_eyeCameras[eyeIndex].m_up - m_eyeCameras[eyeIndex].m_down);
		m_eyeCameras[eyeIndex].m_projFlags   = Camera::ProjFlag_Perspective | Camera::ProjFlag_Infinite | Camera::ProjFlag_Asymmetrical;
		m_eyeCameras[eyeIndex].m_projDirty   = true;

		m_eyeCameras[eyeIndex].updateGpuBuffer(); // force-allocate GPU buffer
	}

	m_shPrimeDepth = Shader::CreateVsFs("shaders/BasicRenderer/DepthClear.glsl", "shaders/BasicRenderer/DepthClear.glsl");
	FRM_ASSERT(m_shPrimeDepth && m_shPrimeDepth->getState() == Shader::State_Loaded);

	return true;
}

void VRContext::shutdown()
{
	if (m_impl)
	{
		m_impl->shutdownSwapChain();
		m_impl->shutdownStencilMesh();
		if (m_impl->m_ovrSession)
		{
			ovr_Destroy(m_impl->m_ovrSession);
			m_impl->m_ovrSession = nullptr;
		}

		//ovr_Shutdown(); // \todo this causes a crash later

		FRM_DELETE(m_impl);
		m_impl = nullptr;	
	}

	for (Camera& camera : m_eyeCameras)
	{
		Buffer::Destroy(camera.m_gpuBuffer);
	}
}

void VRContext::pollHmd(float _dt)
{
	FRM_STRICT_ASSERT(m_impl);

	const double predictedDisplayTime = ovr_GetPredictedDisplayTime(m_impl->m_ovrSession, (long long)m_frameIndex);
	ovrTrackingState& trackingState = m_impl->m_ovrTrackingState;
	trackingState = ovr_GetTrackingState(m_impl->m_ovrSession, predictedDisplayTime, ovrTrue);
	const double sampleTime = ovr_GetTimeInSeconds();

	if (m_impl->m_ovrSessionStatus.ShouldRecenter)
	{
		ovr_RecenterTrackingOrigin(m_impl->m_ovrSession);
	}
	
	// \todo check hand status flags separately?
	if (trackingState.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked))
	{
		// Re-acquire eye render descs (IPD can change during runtime).
		for (int eyeIndex = 0; eyeIndex < Eye_Count; ++eyeIndex)
		{
			m_impl->m_ovrEyeDesc[eyeIndex] = ovr_GetRenderDesc(m_impl->m_ovrSession, (ovrEyeType)eyeIndex, m_impl->m_ovrHmdDesc.DefaultEyeFov[eyeIndex]);
		}
		// Calculate eye poses.
		const ovrPosef hmdToEyeOffset[Eye_Count] =
			{
				m_impl->m_ovrEyeDesc[Eye_Left].HmdToEyePose,
				m_impl->m_ovrEyeDesc[Eye_Right].HmdToEyePose
			};
		ovr_CalcEyePoses(trackingState.HeadPose.ThePose, hmdToEyeOffset, m_impl->m_ovrEyePoses);

		// Update layer poses.
		for (int layerIndex = 0; layerIndex < Layer_Count; ++layerIndex)
		{
			OVRLayer& layer = m_impl->m_layers[layerIndex];
			ovrLayerEyeFov& layerEyeFov = layer.m_ovrLayer;
			layerEyeFov.SensorSampleTime = sampleTime;

			for (int eyeIndex = 0; eyeIndex < Eye_Count; ++eyeIndex)
			{
				layerEyeFov.Fov[eyeIndex] = m_impl->m_ovrEyeDesc[eyeIndex].Fov;
				layerEyeFov.RenderPose[eyeIndex] = m_impl->m_ovrEyePoses[eyeIndex];
			}
		}

		// Set tracked data.
		const mat4 userTransform = TransformationMatrix(m_userTransform.position, m_userTransform.orientation);
		m_trackedData.headOffset = OVRVec3ToVec3(trackingState.HeadPose.ThePose.Position);
		OVRPoseStateToPoseData(m_trackedData.headPose, trackingState.HeadPose, userTransform);
		OVRPoseStateToPoseData(m_trackedData.handPoses[Hand_Left],  trackingState.HandPoses[ovrHand_Left], userTransform);
		OVRPoseStateToPoseData(m_trackedData.handPoses[Hand_Right], trackingState.HandPoses[ovrHand_Right], userTransform);

		vec3 userLinearVelocity = (m_userTransform.position - m_prevUserTransform.position) / _dt;
		m_trackedData.headPose.linearVelocity += userLinearVelocity;
		m_trackedData.handPoses[Hand_Left].linearVelocity += userLinearVelocity;
		m_trackedData.handPoses[Hand_Right].linearVelocity += userLinearVelocity;
		// \todo linear acceleration + angular velocity/acceleration

		// Update eye poses + cameras.
		for (int eyeIndex = 0; eyeIndex < Eye_Count; ++eyeIndex)
		{
			vec3 eyePosition = OVRVec3ToVec3(m_impl->m_ovrEyeDesc[eyeIndex].HmdToEyePose.Position);
			quat eyeRotation = OVRQuatToQuat(m_impl->m_ovrEyeDesc[eyeIndex].HmdToEyePose.Orientation);
			mat4 eyePose = m_trackedData.headPose.pose * TransformationMatrix(eyePosition, eyeRotation);
			
			m_trackedData.eyePoses[eyeIndex] = eyePose;

			Camera& eyeCamera = m_eyeCameras[eyeIndex];

			// Motion blur: we want to eliminate blur from head motion while preserving object motion. Naive approach is to just force prevViewProj == viewProj, however this causes false blur
			// to happen on objects which are translating with the camera. Better approach is to eliminate head rotation but preserve translation. Note that this still causes false blur on
			// objects which are rotating with the head (i.e. hands/controllers) but is much less objectionable.
			mat4 prevWorld = m_trackedData.eyePoses[eyeIndex];
			prevWorld[3].xyz() = eyeCamera.m_world[3].xyz();

			eyeCamera.m_world = m_trackedData.eyePoses[eyeIndex];
			eyeCamera.m_viewProj = eyeCamera.m_proj * AffineInverse(prevWorld); // update() will set m_prevViewProj = m_viewProj, hence modify m_viewProj here 
			eyeCamera.update();
		}
	}
}

void VRContext::pollInput(float _dt)
{
	FRM_STRICT_ASSERT(m_impl);

	unsigned int connectedControllers = ovr_GetConnectedControllerTypes(m_impl->m_ovrSession);

	if (!(connectedControllers & ovrControllerType_Touch))
	{
		m_input.m_isConnected = false;
		return;
	}

	m_input.m_isConnected = true;

	if (!m_impl->m_ovrSessionStatus.HasInputFocus)
	{
		m_input.reset();
		return;
	}

	ovrInputState state;
	ovrAssert(ovr_GetInputState(m_impl->m_ovrSession, ovrControllerType_Touch, &state));

	m_input.pollBegin();

	m_input.m_axisStates[VRInput::Axis_LThumbStickX]   = state.ThumbstickRaw[ovrHand_Left].x;
	m_input.m_axisStates[VRInput::Axis_LThumbStickY]   = state.ThumbstickRaw[ovrHand_Left].y;
	m_input.m_axisStates[VRInput::Axis_RThumbStickX]   = state.ThumbstickRaw[ovrHand_Right].x;
	m_input.m_axisStates[VRInput::Axis_RThumbStickY]   = state.ThumbstickRaw[ovrHand_Right].y;
	m_input.m_axisStates[VRInput::Axis_LTrigger]       = state.IndexTriggerRaw[ovrHand_Left];
	m_input.m_axisStates[VRInput::Axis_RTrigger]       = state.IndexTriggerRaw[ovrHand_Right];
	m_input.m_axisStates[VRInput::Axis_LGrip]          = state.HandTriggerRaw[ovrHand_Left];
	m_input.m_axisStates[VRInput::Axis_RGrip]          = state.HandTriggerRaw[ovrHand_Right];

	m_input.setIncButton(VRInput::Button_LMenu,        0 != (state.Buttons & ovrButton_Enter));
	m_input.setIncButton(VRInput::Button_RMenu,        0 != (state.Buttons & ovrButton_Home));
	m_input.setIncButton(VRInput::Button_A,            0 != (state.Buttons & ovrButton_A));
	m_input.setIncButton(VRInput::Touch_A,             0 != (state.Touches & ovrTouch_A));
	m_input.setIncButton(VRInput::Button_B,            0 != (state.Buttons & ovrButton_B));
	m_input.setIncButton(VRInput::Touch_B,             0 != (state.Touches & ovrTouch_B));
	m_input.setIncButton(VRInput::Button_X,            0 != (state.Buttons & ovrButton_X));
	m_input.setIncButton(VRInput::Touch_X,             0 != (state.Touches & ovrTouch_X));
	m_input.setIncButton(VRInput::Button_Y,            0 != (state.Buttons & ovrButton_Y));
	m_input.setIncButton(VRInput::Touch_Y,             0 != (state.Touches & ovrTouch_Y));
	m_input.setIncButton(VRInput::Button_LThumb,       0 != (state.Buttons & ovrButton_LThumb));
	m_input.setIncButton(VRInput::Touch_LThumb,        0 != (state.Touches & ovrTouch_LThumb));
	m_input.setIncButton(VRInput::Button_RThumb,       0 != (state.Buttons & ovrButton_RThumb));
	m_input.setIncButton(VRInput::Touch_RThumb,        0 != (state.Touches & ovrTouch_RThumb));
	m_input.setIncButton(VRInput::Touch_LTrigger,      0 != (state.Touches & ovrTouch_LIndexTrigger));
	m_input.setIncButton(VRInput::Touch_RTrigger,      0 != (state.Touches & ovrTouch_RIndexTrigger));
	m_input.setIncButton(VRInput::Pose_LIndexPointing, 0 != (state.Touches & ovrTouch_LIndexPointing));
	m_input.setIncButton(VRInput::Pose_RIndexPointing, 0 != (state.Touches & ovrTouch_RIndexPointing));
	m_input.setIncButton(VRInput::Pose_LThumbUp,       0 != (state.Touches & ovrTouch_LThumbUp));
	m_input.setIncButton(VRInput::Pose_RThumbUp,       0 != (state.Touches & ovrTouch_RThumbUp));

	constexpr float kTriggerThreshold = 0.5f;
	m_input.setIncButton(VRInput::Button_LTrigger,     state.IndexTrigger[ovrHand_Left] > kTriggerThreshold);
	m_input.setIncButton(VRInput::Button_RTrigger,     state.IndexTrigger[ovrHand_Right] > kTriggerThreshold);
	m_input.setIncButton(VRInput::Button_LGrip,        state.HandTrigger[ovrHand_Left]  > kTriggerThreshold);
	m_input.setIncButton(VRInput::Button_RGrip,        state.HandTrigger[ovrHand_Right]  > kTriggerThreshold);

	m_input.pollEnd();
}

} // namespace frm
