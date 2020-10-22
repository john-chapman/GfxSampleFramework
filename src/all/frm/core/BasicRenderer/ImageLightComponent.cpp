#include "ImageLightComponent.h"

#include <frm/core/types.h>
#include <frm/core/Image.h>
#include <frm/core/GlContext.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

#include <imgui/imgui.h>

namespace frm {

FRM_COMPONENT_DEFINE(ImageLightComponent, 0);

// PUBLIC

void ImageLightComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("ImageLightComponent::Update");

	if (_phase != World::UpdatePhase::PostPhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		ImageLightComponent* component = (ImageLightComponent*)*_from;
	}
}

eastl::span<ImageLightComponent*> ImageLightComponent::GetActiveComponents()
{
	static ComponentList& activeList = (ComponentList&)Component::GetActiveComponents(StringHash("ImageLightComponent"));
	return eastl::span<ImageLightComponent*>(*((eastl::vector<ImageLightComponent*>*)&activeList));
}

// PROTECTED

bool ImageLightComponent::initImpl()
{
	bool ret = true;

	if (!m_texturePath.isEmpty())
	{
		ret = loadAndFilter();
	}

	return ret;
}

void ImageLightComponent::shutdownImpl()
{
	Texture::Release(m_texture);
}

bool ImageLightComponent::editImpl()
{
	bool ret = false;

	if (ImGui::Button("Source"))
	{
		if (FileSystem::PlatformSelect(m_texturePath, { "*.exr", "*.hdr", "*.dds", "*.psd", "*.tga", "*.png" }))
		{
			m_texturePath = FileSystem::MakeRelative(m_texturePath.c_str());
			ret |= loadAndFilter();
		}
	}
	ImGui::SameLine();
	ImGui::Text("'%s'", m_texturePath.c_str());

	ret |= ImGui::DragFloat("Brightness", &m_brightness, 0.1f);
	ret |= ImGui::Checkbox("Is Background", &m_isBackground);

	if (ImGui::Button("Refilter"))
	{
		ret |= loadAndFilter();
	}

	return ret;
}

bool ImageLightComponent::serializeImpl(Serializer& _serializer_)
{
	if (!SerializeAndValidateClass(_serializer_))
	{
		return false;
	}

	Serialize(_serializer_, m_brightness,   "m_brightness");
	Serialize(_serializer_, m_texturePath,  "m_texturePath");
	Serialize(_serializer_, m_isBackground, "m_isBackground");
	return _serializer_.getError() == nullptr;
}

bool ImageLightComponent::loadAndFilter()
{	
	FRM_AUTOTIMER("ImageLightComponent::loadAndFilter");

	if (m_texturePath.isEmpty())
	{
		return false;
	}

	File srcFile;
	if (!FileSystem::Read(srcFile, m_texturePath.c_str()))
	{
		return false;
	}

	Image srcImage;
	if (!Image::Read(srcImage, srcFile))
	{
		return false;
	}

	Texture* srcTexture = Texture::Create(srcImage);
	if (srcImage.getType() != Image::Type_Cubemap)
	{
		// Convert to cubemap, assume rectilinear (sphere) projection.
		srcTexture->setWrapV(GL_CLAMP_TO_EDGE);
		if (!Texture::ConvertSphereToCube(*srcTexture, srcTexture->getHeight()))
		{
			return false;
		}
	}
	Texture* dstTexture = Texture::CreateCubemap(srcTexture->getWidth(), GL_RGBA16F, 99);
	const bool srcIsGamma = !DataTypeIsFloat(srcImage.getImageDataType());

	{	FRM_AUTOTIMER("Filter");
		srcTexture->generateMipmap();

		GlContext* ctx = GlContext::GetCurrent();
		Shader* shFilter = Shader::CreateCs("shaders/BasicRenderer/FilterImageLight.glsl", 8, 8);
		FRM_ASSERT(shFilter && shFilter->getState() == Shader::State_Loaded);
		
		for (int i = 0; i < dstTexture->getMipCount(); ++i)
		{
			ctx->setShader(shFilter);
			ctx->setUniform("uLevel", i);
			ctx->setUniform("uMaxLevel", (int)dstTexture->getMipCount());
			ctx->setUniform("uSrcIsGamma", (int)srcIsGamma);
			ctx->bindTexture("txSrc", srcTexture);
			ctx->bindImage("txDst", dstTexture, GL_WRITE_ONLY, i);
			ctx->dispatch(dstTexture, 6);
		}
		glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
		glAssert(glFinish());

		Shader::Release(shFilter);
		Texture::Release(srcTexture);
	}

	Texture::Release(m_texture);
	m_texture = dstTexture;

	return true;
}


} // namespace frm