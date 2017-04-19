#include <frm/Texture.h>

#include <frm/gl.h>
#include <frm/icon_fa.h>
#include <frm/Framebuffer.h>
#include <frm/GlContext.h>
#include <frm/Resource.h>
#include <frm/Shader.h>

#include <apt/File.h>
#include <apt/FileSystem.h>
#include <apt/Image.h>
#include <apt/Time.h>

#include <imgui/imgui.h>

#include <EASTL/utility.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                                 TextureView

*******************************************************************************/
TextureView::TextureView(Texture* _texture)
	: m_texture(_texture)
	, m_offset(0.0f, 0.0f)
	, m_size(0.0f, 0.0f)
	, m_mip(0)
	, m_array(0)
{
	m_rgbaMask[0] = m_rgbaMask[1] = m_rgbaMask[2] = m_rgbaMask[3] = true;
	if (m_texture) {
		m_size = vec2(_texture->getWidth(), _texture->getHeight());
		//Texture::Use(m_texture);
	}
}

TextureView::~TextureView()
{
	//Texture::Release(m_texture);
}

void TextureView::reset()
{
	m_offset = vec2(0.0f);
	m_size = vec2(m_texture->getWidth(), m_texture->getHeight());
	m_mip = 0;
	m_array = 0;
}

vec2 TextureView::getNormalizedOffset() const 
{ 
	return m_offset / vec2(m_texture->getWidth(), m_texture->getHeight()); 
}
vec2 TextureView::getNormalizedSize() const
{ 
	return m_size / vec2(m_texture->getWidth(), m_texture->getHeight()); 
}

/*******************************************************************************

                               TextureViewer

*******************************************************************************/
struct TextureViewer
{
	int  m_selected;
	bool m_showHidden;
	bool m_showTexelGrid;
	bool m_isDragging;
	eastl::vector<TextureView> m_txViews;

	static vec2 ThumbToTxView(const frm::TextureView& _txView)
	{
		ImGuiIO io = ImGui::GetIO();
		vec2 thumbPos;
		thumbPos.x = io.MousePos.x - ImGui::GetItemRectMin().x;
		thumbPos.y = ImGui::GetItemRectMax().y - io.MousePos.y;
		thumbPos /= vec2(ImGui::GetItemRectSize()); // y is inverted in thumbnail space
		return _txView.m_offset + thumbPos * _txView.m_size;
	}

	TextureViewer()
		: m_selected(-1)
		, m_showHidden(false)
		, m_showTexelGrid(true)
		, m_isDragging(false)
	{
	}

	void addTextureView(Texture* _tx)
	{
		m_txViews.push_back(TextureView(_tx));
	}

	void removeTextureView(Texture* _tx)
	{
		int i = 0;
		for (auto it = m_txViews.begin(); it != m_txViews.end(); ++it, ++i) {
			if (it->m_texture == _tx) {
				APT_ASSERT(_tx->getHandle() == it->m_texture->getHandle());
				m_txViews.erase(it);
				if (m_selected == i) {
					m_selected = -1;
				}
				return;
			}
		}
	}

	TextureView* findTextureView(Texture* _tx)
	{
		for (auto& txView : m_txViews) {
			if (txView.m_texture == _tx) {
				return &txView;
			}
		}
		return nullptr;
	}

	void draw(bool* _open_)
	{
		using namespace frm::internal;

		static ImGuiTextFilter filter;
	 	static const ImVec4 kColorTxName = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
		static const ImVec4 kColorTxInfo = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
		static const ImU32  kColorGrid   = ImColor(1.0f, 1.0f, 1.0f, 0.5f);
		static const float  kThumbHeight = 128.0f;
		static const float  kZoomSpeed   = 32.0f;
	
		ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetItemsLineHeightWithSpacing()), ImGuiSetCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2), ImGuiSetCond_FirstUseEver);
		if (!ImGui::Begin("Texture Viewer", _open_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
			ImGui::End();
			return; // window collapsed, early-out
		}
	
		ImGuiIO& io = ImGui::GetIO();
	
		if (m_selected == -1) {
			ImGui::AlignFirstTextHeightToWidgets();
			ImGui::Text("%d texture%s", Texture::GetInstanceCount(), Texture::GetInstanceCount() > 0 ? "s" : "");
			ImGui::SameLine();
			ImGui::Checkbox("Show Hidden", &m_showHidden);
			ImGui::SameLine();
			ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.2f);
				filter.Draw("Filter##TextureName");
			ImGui::PopItemWidth();
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_REFRESH " Reload All")) {
				Texture::ReloadAll();
			}
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_FLOPPY_O " Load")) {
				FileSystem::PathStr pth;
				if (FileSystem::PlatformSelect(pth)) {
					FileSystem::StripRoot(pth, pth);
					Texture::Create(pth);
				}
			}
			
			ImGui::Separator();
			
			bool first = true;
			for (int i = 0; i < (int)m_txViews.size(); ++i) {
				TextureView& txView = m_txViews[i];
				APT_ASSERT(txView.m_texture != nullptr);
				Texture& tx = *txView.m_texture;
				if (!filter.PassFilter(tx.getName())) {
					continue;
				}
				if (tx.getName()[0] == '#' && !m_showHidden) {
					continue;
				}
	
			 // compute thumbnail size
				//float txAspect = (float)tx.getWidth() / (float)tx.getHeight();
				//float thumbWidth = kThumbHeight * (float)tx.getWidth() / (float)tx.getHeight();
				//vec2 thumbSize(APT_MIN(thumbWidth, kThumbHeight * 2.0f), kThumbHeight);
				vec2 thumbSize(kThumbHeight); float thumbWidth = kThumbHeight; // square thumbnails
	
				
			 // move to a new line if the thumbnail width is too big to fit in the content region
				if (!first) { // (except if it's the first one)
					ImGui::SameLine();
					if (ImGui::GetCursorPosX() + thumbWidth > ImGui::GetContentRegionMax().x) {
						ImGui::NewLine();
					};
				}
				first = false;
				
			 // thumbnail button
				if (ImGui::ImageButton((ImTextureID)&txView, thumbSize, ImVec2(0, 1), ImVec2(1, 0), 1, ImColor(0.5f, 0.5f, 0.5f))) {
					m_selected = i;
				}
			 // basic info tooltip
				if (ImGui::IsItemHovered()) {
					ImGui::BeginTooltip();
						ImGui::TextColored(kColorTxName, tx.getName());
						ImGui::TextColored(kColorTxInfo, "%s\n%s\n%dx%dx%d", GlEnumStr(tx.getTarget()), GlEnumStr(tx.getFormat()), tx.getWidth(), tx.getHeight(), APT_MAX(tx.getDepth(), tx.getArrayCount()));
					ImGui::EndTooltip();
				}
			}
		
		} else {
			TextureView& txView = m_txViews[m_selected];
			APT_ASSERT(txView.m_texture != nullptr);
			Texture& tx = *txView.m_texture;
			float txAspect = (float)tx.getWidth() / (float)tx.getHeight();
	
			if (ImGui::Button(ICON_FA_BACKWARD)) {
				m_selected = -1;
			}
			if (*tx.getPath() != '\0') {
				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_REFRESH " Reload")) {
					tx.reload();
				}
				ImGui::SameLine();
				if (ImGui::Button(ICON_FA_FLOPPY_O " Replace")) {
					FileSystem::PathStr pth;
					if (FileSystem::PlatformSelect(pth)) {
						FileSystem::StripRoot(pth, pth);
						tx.setPath(pth);
						tx.reload();
						txView.reset();
					}
				}
			}
			ImGui::SameLine();
			ImGui::Checkbox("Show Texel Grid", &m_showTexelGrid);
			ImGui::Separator();
	
			ImGui::Columns(2);
			//float thumbWidth  = ImGui::GetContentRegionAvailWidth();
			//float thumbHeight = (float)tx.getHeight() / (float)tx.getWidth() * thumbWidth;
			float thumbHeight = ImGui::GetWindowHeight() * 0.75f;
			float thumbWidth = (float)tx.getWidth() / (float)tx.getHeight() * thumbHeight;
			vec2  thumbSize(thumbWidth, APT_MAX(thumbHeight, 16.0f));
		  // need to flip the UVs here to account for the orientation of the quad output by ImGui
			vec2  uv0 = vec2(0.0f, 1.0f);
			vec2  uv1 = vec2(1.0f, 0.0f);
			if (ImGui::ImageButton((ImTextureID)&txView, thumbSize, uv0, uv1, 0)) {
				//m_selected = -1;
			}
			if (m_showTexelGrid) {
				vec2 drawStart = ImGui::GetItemRectMin();
				vec2 drawEnd   = ImGui::GetItemRectMax();
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				drawList->AddRect(drawStart, drawEnd, kColorGrid);
				drawList->PushClipRect(drawStart, min(drawEnd, vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetWindowSize())));
					if ((drawEnd.x - drawStart.x) > (txView.m_size.x * 3.0f)) { // only draw grid if texel density is low enough
						float scale = thumbSize.x / txView.m_size.x;
						float bias  = (1.0f - fract(txView.m_offset.x)) * scale;
						for (int i = 0, n = (int)txView.m_size.x + 1; i <= n; ++i) {
							float x = drawStart.x + (float)i * scale + bias;
							drawList->AddLine(vec2(x, drawStart.y), vec2(x, drawEnd.y), kColorGrid);
						}
						scale = thumbSize.y / txView.m_size.y;
						bias  = (1.0f - fract(txView.m_offset.y)) * scale;
						for (int i = 0, n = (int)txView.m_size.y + 1; i <= n; ++i) {
							float y = drawEnd.y - (float)i * scale - bias;
							drawList->AddLine(vec2(drawStart.x, y), vec2(drawEnd.x, y), kColorGrid);
						}
					}	
				drawList->PopClipRect();
			}
	
			if (m_isDragging || ImGui::IsItemHovered()) {
			 // zoom
				vec2 txViewPos = ThumbToTxView(txView);
				//ImGui::BeginTooltip();
				//	ImGui::Text("%.1f, %.1f", txViewPos.x, txViewPos.y);
				//ImGui::EndTooltip();
				vec2 offsetBeforeZoom = ThumbToTxView(txView);
				vec2 zoomDelta = vec2(txAspect, 1.0f) * vec2(io.MouseWheel * kZoomSpeed);
				txView.m_size = max(txView.m_size - zoomDelta, vec2(txAspect * 4.0f, 4.0f));
				vec2 offsetAfterZoom = ThumbToTxView(txView);
				vec2 offsetDelta = offsetBeforeZoom - offsetAfterZoom;
				txView.m_offset += offsetDelta;
	
			 // pan
				if (io.MouseDown[0]) {
					m_isDragging = true;
				}
			}
			if (m_isDragging) {
				if (!io.MouseDown[0]) {
					m_isDragging = false;
				}
				vec2 offset = vec2(io.MouseDelta.x, -io.MouseDelta.y) * vec2(txView.m_texture->getWidth(), txView.m_texture->getHeight()) / vec2(thumbWidth, thumbHeight) * txView.getNormalizedSize();
				txView.m_offset -= offset;
			}
			ImGui::NextColumn();
			ImGui::SetColumnOffset(-1, thumbWidth + ImGui::GetStyle().ItemSpacing.x);
		
		 // zoom/pan
			if (ImGui::Button("Reset View")) {
				txView.reset();
			}
			ImGui::SameLine();
			ImGui::Text("Zoom: %1.2f%, %1.2f ", txView.m_size.x, txView.m_size.y);
			ImGui::SameLine();
			ImGui::Text("Pan: %1.2f,%1.2f", txView.m_offset.x, txView.m_offset.y);
			ImGui::Spacing();

		 // basic info
			ImGui::AlignFirstTextHeightToWidgets();
			ImGui::TextColored(kColorTxName, tx.getName());
			
			ImGui::TextColored(kColorTxInfo, "Id:     %llu",     tx.getId());
			ImGui::TextColored(kColorTxInfo, "Type:   %s",       GlEnumStr(tx.getTarget()));
			ImGui::TextColored(kColorTxInfo, "Format: %s",       GlEnumStr(tx.getFormat()));
			ImGui::TextColored(kColorTxInfo, "Size:   %dx%dx%d", tx.getWidth(), tx.getHeight(), tx.getDepth());
			ImGui::TextColored(kColorTxInfo, "Array:  %d",       tx.getArrayCount());
			ImGui::TextColored(kColorTxInfo, "Mips:   %d",       tx.getMipCount());	
			
		 // filter mode
			ImGui::Spacing(); ImGui::Spacing();
			int fm = TextureFilterModeToIndex(tx.getMinFilter());
			if (ImGui::Combo("Min Filter", &fm, "NEAREST\0LINEAR\0NEAREST_MIPMAP_NEAREST\0LINEAR_MIPMAP_NEAREST\0NEAREST_MIPMAP_LINEAR\0LINEAR_MIPMAP_LINEAR\0")) { // must match order of internal::kTextureWrapModes (gl.cpp)
				tx.setMinFilter(kTextureFilterModes[fm]);
			}
			fm = TextureFilterModeToIndex(tx.getMagFilter());
			if (ImGui::Combo("Mag Filter", &fm, "NEAREST\0LINEAR\0")) { // must match order of internal::kTextureWrapModes (gl.cpp)
				tx.setMagFilter(kTextureFilterModes[fm]);
			}
	
		 // anisotropy
			float aniso = tx.getAnisotropy();
			if (ImGui::SliderFloat("Anisotropy", &aniso, 1.0f, 16.0f)) {
				tx.setAnisotropy(aniso);
			}
		
		 // wrap mode
			ImGui::Spacing();
			static const char* kWrapItems = "REPEAT\0MIRRORED_REPEAT\0CLAMP_TO_EDGE\0MIRROR_CLAMP_TO_EDGE\0CLAMP_TO_BORDER\0"; // must match order of internal::kTextureWrapModes (gl.cpp)
			int wm = TextureWrapModeToIndex(tx.getWrapU());
			if (ImGui::Combo("Wrap U", &wm, kWrapItems)) {
				tx.setWrapU(kTextureWrapModes[wm]);
			}
			wm = TextureWrapModeToIndex(tx.getWrapV());
			if (ImGui::Combo("Wrap V", &wm, kWrapItems)) {
				tx.setWrapV(kTextureWrapModes[wm]);
			}
			if (tx.getDepth() > 1) {
				wm = TextureWrapModeToIndex(tx.getWrapW());
				if (ImGui::Combo("Wrap W", &wm, kWrapItems)) {
					tx.setWrapW(kTextureWrapModes[wm]);
				}
			}
		
		 // view options
			ImGui::Checkbox("R", &txView.m_rgbaMask[0]);
			ImGui::SameLine();
			ImGui::Checkbox("G", &txView.m_rgbaMask[1]);
			ImGui::SameLine();
			ImGui::Checkbox("B", &txView.m_rgbaMask[2]);
			ImGui::SameLine();
			ImGui::Checkbox("A", &txView.m_rgbaMask[3]);
	
			if (tx.getDepth() > 1) {
				ImGui::SliderInt("Layer", &txView.m_array, 0, tx.getDepth() - 1);
			}
			if (tx.getTarget() == GL_TEXTURE_CUBE_MAP) {
				ImGui::SliderInt("Face", &txView.m_array, 0, 5);
			}
			if (tx.getArrayCount() > 1) {
				ImGui::SliderInt("Array", &txView.m_array, 0, tx.getArrayCount() - 1);
			}
			if (tx.getMipCount() > 1) {
				ImGui::SliderInt("Mip", &txView.m_mip, 0, tx.getMipCount() - 1);
			}
		
			ImGui::Columns(1);
		}
	
		ImGui::End();
	}
};

static TextureViewer g_textureViewer;

void Texture::ShowTextureViewer(bool* _open_)
{
	g_textureViewer.draw(_open_);
}

/*******************************************************************************

                                   Texture

*******************************************************************************/

struct Texture_ScopedPixelStorei
{
	GLenum m_pname;
	GLint  m_param;

	Texture_ScopedPixelStorei(GLenum _pname, GLint _param)
		: m_pname(_pname)
	{
		glAssert(glGetIntegerv(m_pname, &m_param));
		glAssert(glPixelStorei(m_pname, _param));
	}

	~Texture_ScopedPixelStorei()
	{
		glAssert(glPixelStorei(m_pname, m_param));
	}
};
#define SCOPED_PIXELSTOREI(_pname, _param) Texture_ScopedPixelStorei APT_UNIQUE_NAME(_scopedPixelStorei)(_pname, _param)


static bool GlIsTexFormatCompressed(GLenum _format)
{
	switch (_format) {
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
		case GL_COMPRESSED_RED_RGTC1:
		case GL_COMPRESSED_RG_RGTC2:
		case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:
		case GL_COMPRESSED_RGBA_BPTC_UNORM:
			return true;
		default:
			return false;
	};
}

static bool GlIsTexFormatDepth(GLenum _format)
{
	switch (_format) {
		case GL_DEPTH_COMPONENT16:
		case GL_DEPTH_COMPONENT24:
		case GL_DEPTH_COMPONENT32:
		case GL_DEPTH_COMPONENT32F:
		case GL_DEPTH24_STENCIL8:
		case GL_DEPTH32F_STENCIL8:
			return true;
		default:
			return false;
	};
}

// PUBLIC

Texture* Texture::Create(const char* _path)
{
	Id id = GetHashId(_path);
	Texture* ret = Find(id);
	if (!ret) {
		ret = new Texture(id, _path);
		ret->m_path.set(_path);
	}
	Use(ret);
	if (ret->getState() != State_Loaded) {
	 // \todo replace with default
	} else {
		g_textureViewer.addTextureView(ret);
	}
	return ret;
}

Texture* Texture::CreateCubemap2x3(const char* _path)
{
	Id id = GetHashId(_path);
	Texture* ret = Find(id);
	if (!ret) {
		ret = new Texture(id, _path);
		ret->m_target = GL_TEXTURE_CUBE_MAP; // modifies behavior of reload()
		ret->m_path.set(_path);
	}
	Use(ret);
	if (ret->getState() != State_Loaded) {
	 // \todo replace with default
	} else {
		g_textureViewer.addTextureView(ret);
	}
	return ret;
}

Texture* Texture::Create1d(GLsizei _width, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_1D, _width, 1, 1, 1, _mipCount, _format);
}
Texture* Texture::Create1dArray(GLsizei _width, GLsizei _arrayCount, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_1D_ARRAY, _width, 1, 1, _arrayCount, _mipCount, _format);
}
Texture* Texture::Create2d(GLsizei _width, GLsizei _height, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_2D, _width, _height, 1, 1, _mipCount, _format);
}
Texture* Texture::Create2dArray(GLsizei _width, GLsizei _height, GLsizei _arrayCount, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_2D_ARRAY, _width, _height, 1, _arrayCount, _mipCount, _format);
}
Texture* Texture::Create3d(GLsizei _width, GLsizei _height, GLsizei _depth, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_3D, _width, _height, _depth, 1, _mipCount, _format);
}
Texture* Texture::CreateCubemap(GLsizei _width, GLenum _format, GLint _mipCount)
{
	return Create(GL_TEXTURE_CUBE_MAP, _width, _width, _width, 1, _mipCount, _format);
}

Texture* Texture::CreateProxy(GLuint _handle, const char* _name)
{
	Id id = GetUniqueId();

	Texture* ret = Find(id);
	if (ret) {
		return ret;
	}
	
	ret = new Texture(id, _name);
	if (_name[0] == '\0') {
		ret->setNamef("%llu", id);
	}
	ret->m_handle = _handle;
	ret->m_ownsHandle = false;
	ret->m_width = ret->m_height = ret->m_depth = 1;
	ret->m_mipCount = 1;
	ret->m_arrayCount  = 1;
	glAssert(glGetTextureParameteriv(_handle, GL_TEXTURE_TARGET, (GLint*)&ret->m_target));
	glAssert(glGetTextureLevelParameteriv(_handle, 0, GL_TEXTURE_WIDTH, (GLint*)&ret->m_width));
	glAssert(glGetTextureLevelParameteriv(_handle, 0, GL_TEXTURE_HEIGHT, (GLint*)&ret->m_height));
	ret->m_height = APT_MAX(ret->m_height, 1);
	if (ret->m_target == GL_TEXTURE_1D_ARRAY) {
		ret->m_arrayCount = ret->m_height;
		ret->m_height = 1;
	}
	glAssert(glGetTextureLevelParameteriv(_handle, 0, GL_TEXTURE_DEPTH, (GLint*)&ret->m_depth));
	ret->m_depth = APT_MAX(ret->m_depth, 1);
	if (ret->m_target == GL_TEXTURE_2D_ARRAY) {
		ret->m_arrayCount = ret->m_depth;
		ret->m_depth = 1;
	}
	glAssert(glGetTextureLevelParameteriv(_handle, 0, GL_TEXTURE_INTERNAL_FORMAT, (GLint*)&ret->m_format));
	glAssert(glGetTextureParameteriv(_handle, GL_TEXTURE_MAX_LEVEL, (GLint*)&ret->m_mipCount));
	ret->m_mipCount = APT_MAX(ret->m_mipCount, 1);
	ret->setState(State_Loaded);
	
	Use(ret);
	g_textureViewer.addTextureView(ret);
	return ret;
}

void Texture::Destroy(Texture*& _inst_)
{
	g_textureViewer.removeTextureView(_inst_);
	delete _inst_;
}

GLint Texture::GetMaxMipCount(GLsizei _width, GLsizei _height, GLsizei _depth)
{
	const double rlog2 = 1.0 / log(2.0);
	const GLint log2Width  = (GLint)(log((double)_width)  * rlog2);
	const GLint log2Height = (GLint)(log((double)_height) * rlog2);
	const GLint log2Depth  = (GLint)(log((double)_depth)  * rlog2);
	return APT_MAX(log2Width, APT_MAX(log2Height, log2Depth)) + 1; // +1 for level 0
}

bool Texture::ConvertSphereToCube(Texture& _sphere, GLsizei _width)
{
	static Shader* shConvert;
	if_unlikely (!shConvert) {
		shConvert = Shader::CreateCs("shaders/ConvertEnvmap_cs.glsl", 1, 1, 1, "SPHERE_TO_CUBE\0");
		if (!shConvert) {
			return false;
		}
	}

 // \hack can't bind RGB textures as images, so convert
	GLenum format = _sphere.m_format;
	switch (format) {
		case GL_RGB32F: format = GL_RGBA32F; break;
		case GL_RGB16F: format = GL_RGBA16F; break;
		case GL_RGB16:  format = GL_RGBA16; break;
		case GL_RGB8:   format = GL_RGBA8; break;
		default: break;
	};

	Texture* cube = CreateCubemap(_width, format, GetMaxMipCount(_width, _width));
	
	//static vec4 kFaceColors[6] = {
	//	vec4(1.0f, 0.0f, 0.0f, 1.0f),
	//	vec4(0.0f, 1.0f, 0.0f, 1.0f),
	//	vec4(0.0f, 0.0f, 1.0f, 1.0f),
	//	vec4(1.0f, 0.0f, 1.0f, 1.0f),
	//	vec4(1.0f, 1.0f, 0.0f, 1.0f),
	//	vec4(0.0f, 1.0f, 1.0f, 1.0f)
	//};
	//Framebuffer* fb = Framebuffer::Create();
	//for (int i = 0; i < 6; ++i) {
	//	fb->attachLayer(cubemap, GL_COLOR_ATTACHMENT0, i);
	//	GlContext::GetCurrent()->setFramebufferAndViewport(fb);
	//	glAssert(glClearColor(kFaceColors[i].x, kFaceColors[i].y, kFaceColors[i].z, kFaceColors[i].w));
	//	glAssert(glClear(GL_COLOR_BUFFER_BIT));
	//}
	//Framebuffer::Destroy(fb);

	GlContext* ctx = GlContext::GetCurrent();
	ctx->setShader(shConvert);
	ctx->bindTexture("txSphere", &_sphere);
	ctx->bindImage("txCube", cube, GL_WRITE_ONLY);
	ctx->dispatch(_width, _width, 6);
	glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));	

	swap(*cube, _sphere);
	Texture::Release(cube);

	TextureView* txView = g_textureViewer.findTextureView(&_sphere);
	if (txView) {
		txView->reset();
	}

	return true;
}

bool Texture::ConvertCubeToSphere(Texture& _cube, GLsizei _width)
{
	static Shader* shConvert;
	if_unlikely (!shConvert) {
		shConvert = Shader::CreateCs("shaders/ConvertEnvmap_cs.glsl", 1, 1, 1, "CUBE_TO_SPHERE\0");
		if (!shConvert) {
			return false;
		}
	}

 // \hack can't bind RGB textures as images, so convert
	GLenum format = _cube.m_format;
	switch (format) {
		case GL_RGB32F: format = GL_RGBA32F; break;
		case GL_RGB16F: format = GL_RGBA16F; break;
		case GL_RGB16:  format = GL_RGBA16; break;
		case GL_RGB8:   format = GL_RGBA8; break;
		default: break;
	};

	Texture* sphere = Create2d(_width, _width / 2, format, GetMaxMipCount(_width, _width / 2));

	GlContext* ctx = GlContext::GetCurrent();
	ctx->setShader(shConvert);
	ctx->bindTexture("txCube", &_cube);
	ctx->bindImage("txSphere", sphere, GL_WRITE_ONLY);
	ctx->dispatch(_width, _width / 2);
	glAssert(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));	

	swap(*sphere, _cube);
	Texture::Release(sphere);

	TextureView* txView = g_textureViewer.findTextureView(&_cube);
	if (txView) {
		txView->reset();
	}

	return true;
}

bool Texture::reload()
{
	if (m_path.isEmpty()) {
		return true;
	}

	APT_AUTOTIMER("Texture::load(%s)", (const char*)m_path);
	
	File f;
	if (!FileSystem::Read(f, (const char*)m_path)) {
		setState(State_Error);
		return false;
	}

	Image img;
	if (!Image::Read(img, f)) {
		setState(State_Error);
		return false;
	}

	if (!loadImage(img)) {
		setState(State_Error);
		return false;
	}
	setState(State_Loaded);

	return true;
}

void Texture::setData(const void* _data, GLenum _dataFormat, GLenum _dataType, GLint _mip)
{
	setSubData(0, 0, 0, m_width, m_height, m_depth, _data, _dataFormat, _dataType, _mip);
	setState(State_Loaded);
}

void Texture::setSubData(
	GLint       _offsetX, 
	GLint       _offsetY, 
	GLint       _offsetZ, 
	GLsizei     _sizeX, 
	GLsizei     _sizeY, 
	GLsizei     _sizeZ, 
	const void* _data, 
	GLenum      _dataFormat, 
	GLenum      _dataType, 
	GLint       _mip
	)
{
	APT_ASSERT(_mip <= m_mipCount);

	if (GlIsTexFormatCompressed(m_format)) {
		#ifdef APT_DEBUG
			GLsizei mipDiv = (GLsizei)pow(2.0, (double)_mip);
			GLsizei w = m_width / mipDiv;
			GLsizei h = m_height / mipDiv;
			if (w <= 4 || h <= 4) { // mip is 1 block
				bool illegal = 
					_offsetX > 0 ||
					_offsetY > 0 ||
					_sizeX != w  ||
					_sizeY != h
					;
				APT_ASSERT_MSG(!illegal, "Illegal operation, cannot upload sub data within a compressed block");
			}
		#endif
		switch (m_target) {
			case GL_TEXTURE_1D:
				glAssert(glCompressedTextureSubImage1D(m_handle, _mip, _offsetX, _sizeX, m_format, (GLsizei)_dataType, (GLvoid*)_data));
				break;
			case GL_TEXTURE_1D_ARRAY:
			case GL_TEXTURE_2D:
				glAssert(glCompressedTextureSubImage2D(m_handle, _mip, _offsetX, _offsetY, _sizeX, _sizeY, m_format, (GLsizei)_dataType, (GLvoid*)_data));
				break;		
			case GL_TEXTURE_2D_ARRAY:
			case GL_TEXTURE_3D:
				glAssert(glCompressedTextureSubImage3D(m_handle, _mip, _offsetX, _offsetY, _offsetZ, _sizeX, _sizeY, _sizeZ, m_format, (GLsizei)_dataType, (GLvoid*)_data));
				break;
			default:
				APT_ASSERT(false);
				setState(State_Error);
				return;
		};

	} else {
		switch (m_target) {
			case GL_TEXTURE_1D:
				glAssert(glTextureSubImage1D(m_handle, _mip, _offsetX, _sizeX, _dataFormat, _dataType, (GLvoid*)_data));
				break;
			case GL_TEXTURE_1D_ARRAY:
			case GL_TEXTURE_2D:
				glAssert(glTextureSubImage2D(m_handle, _mip, _offsetX, _offsetY, _sizeX, _sizeY, _dataFormat, _dataType, (GLvoid*)_data));
				break;		
			case GL_TEXTURE_2D_ARRAY:
			case GL_TEXTURE_3D:
				glAssert(glTextureSubImage3D(m_handle, _mip, _offsetX, _offsetY, _offsetZ, _sizeX, _sizeY, _sizeZ, _dataFormat, _dataType, (GLvoid*)_data));
				break;
			default:
				APT_ASSERT(false);
				setState(State_Error);
				return;
		};
	}
}

void Texture::generateMipmap()
{
	APT_ASSERT(m_handle);
	m_mipCount = GetMaxMipCount(m_width, m_height, m_depth);
	setMipRange(0, m_mipCount - 1);
	setMinFilter(GL_LINEAR_MIPMAP_LINEAR);
	glAssert(glActiveTexture(GL_TEXTURE0));
	glAssert(glGenerateTextureMipmap(m_handle));
	
}

void Texture::setMipRange(GLint _base, GLint _max)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_BASE_LEVEL, (GLint)_base));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAX_LEVEL, (GLint)_max));
}

Image* Texture::downloadImage()
{
	APT_ASSERT(m_handle);

	Image::Layout layout;
	Image::DataType dataType;
	Image::CompressionType compression = Image::Compression_None;
	GLenum glFormat, glType;

	switch (m_format) {
		case GL_R:
		case GL_R8:   layout = Image::Layout_R; dataType = Image::DataType::Uint8N;  glFormat = GL_RED; glType = GL_UNSIGNED_BYTE;  break;
		case GL_R16:  layout = Image::Layout_R; dataType = Image::DataType::Uint16N; glFormat = GL_RED; glType = GL_UNSIGNED_SHORT; break;
		case GL_R16F: layout = Image::Layout_R; dataType = Image::DataType::Float16; glFormat = GL_RED; glType = GL_HALF_FLOAT;     break;
		case GL_R32F: layout = Image::Layout_R; dataType = Image::DataType::Float32; glFormat = GL_RED; glType = GL_FLOAT;          break;

		case GL_RG:
		case GL_RG8:   layout = Image::Layout_RG; dataType = Image::DataType::Uint8N;  glFormat = GL_RG; glType = GL_UNSIGNED_BYTE;  break;
		case GL_RG16:  layout = Image::Layout_RG; dataType = Image::DataType::Uint16N; glFormat = GL_RG; glType = GL_UNSIGNED_SHORT; break;
		case GL_RG16F: layout = Image::Layout_RG; dataType = Image::DataType::Float16; glFormat = GL_RG; glType = GL_HALF_FLOAT;     break;
		case GL_RG32F: layout = Image::Layout_RG; dataType = Image::DataType::Float32; glFormat = GL_RG; glType = GL_FLOAT;          break;

		case GL_RGB:
		case GL_RGB8:   layout = Image::Layout_RGB; dataType = Image::DataType::Uint8N;  glFormat = GL_RGB; glType = GL_UNSIGNED_BYTE;  break;
		case GL_RGB16:  layout = Image::Layout_RGB; dataType = Image::DataType::Uint16N; glFormat = GL_RGB; glType = GL_UNSIGNED_SHORT; break;
		case GL_RGB16F: layout = Image::Layout_RGB; dataType = Image::DataType::Float16; glFormat = GL_RGB; glType = GL_HALF_FLOAT;     break;
		case GL_RGB32F: layout = Image::Layout_RGB; dataType = Image::DataType::Float32; glFormat = GL_RGB; glType = GL_FLOAT;          break;
			
		case GL_RGBA:
		case GL_RGBA8:   layout = Image::Layout_RGBA; dataType = Image::DataType::Uint8N;  glFormat = GL_RGBA; glType = GL_UNSIGNED_BYTE;  break;
		case GL_RGBA16:  layout = Image::Layout_RGBA; dataType = Image::DataType::Uint16N; glFormat = GL_RGBA; glType = GL_UNSIGNED_SHORT; break;
		case GL_RGBA16F: layout = Image::Layout_RGBA; dataType = Image::DataType::Float16; glFormat = GL_RGBA; glType = GL_HALF_FLOAT;     break;
		case GL_RGBA32F: layout = Image::Layout_RGBA; dataType = Image::DataType::Float32; glFormat = GL_RGBA; glType = GL_FLOAT;          break;

		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:       layout = Image::Layout_RGB;  dataType = Image::DataType::InvalidType; compression = Image::Compression_BC1; break;
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:      layout = Image::Layout_RGBA; dataType = Image::DataType::InvalidType; compression = Image::Compression_BC1; break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:      layout = Image::Layout_RGBA; dataType = Image::DataType::InvalidType; compression = Image::Compression_BC2; break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:      layout = Image::Layout_RGBA; dataType = Image::DataType::InvalidType; compression = Image::Compression_BC3; break;
		case GL_COMPRESSED_RED_RGTC1:               layout = Image::Layout_R;    dataType = Image::DataType::InvalidType; compression = Image::Compression_BC4; break;
		case GL_COMPRESSED_RG_RGTC2:                layout = Image::Layout_R;    dataType = Image::DataType::InvalidType; compression = Image::Compression_BC5; break;
		case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT: layout = Image::Layout_RGB;  dataType = Image::DataType::InvalidType; compression = Image::Compression_BC6; break;
		case GL_COMPRESSED_RGBA_BPTC_UNORM:         layout = Image::Layout_RGBA; dataType = Image::DataType::InvalidType; compression = Image::Compression_BC7; break;
	};

	Image* ret = 0;
	switch (m_target) {
		case GL_TEXTURE_1D: ret = Image::Create1d(m_width, layout, dataType, m_mipCount, compression); break;
		case GL_TEXTURE_2D: ret = Image::Create2d(m_width, m_height, layout, dataType, m_mipCount, compression); break;
		case GL_TEXTURE_3D: ret = Image::Create3d(m_width, m_height, m_depth, layout, dataType, m_mipCount, compression); break;

		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D_ARRAY:
		default: APT_ASSERT_MSG(false, "downloadImage unsupported for '%s'", internal::GlEnumStr(m_target)); break;
	};

	SCOPED_PIXELSTOREI(GL_PACK_ALIGNMENT, 1);

 // \todo array/mip support
	APT_ASSERT(m_arrayCount == 1);
	//for (int i = 0; i < m_arrayCount; ++i) {
		//for (int j = 0; j < m_mipCount; ++j) {
		int i = 0, j = 0;
			char* raw = ret->getRawImage(i, j);
			APT_ASSERT(raw);
			if (ret->isCompressed()) {
				glAssert(glGetCompressedTextureImage(m_handle, j, (GLsizei)ret->getRawImageSize(j), raw));
			} else {
				glAssert(glGetTextureImage(m_handle, j, glFormat, glType, (GLsizei)ret->getRawImageSize(j), raw));
			}
		//}
	//}

	return ret;
}

void Texture::setFilter(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, (GLint)_mode));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, (GLint)_mode));
}
void Texture::setMinFilter(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, (GLint)_mode));
}
void Texture::setMagFilter(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, (GLint)_mode));
}
GLenum Texture::getMinFilter() const 
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_MIN_FILTER, &ret));
	return (GLenum)ret;

}
GLenum Texture::getMagFilter() const
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_MAG_FILTER, &ret));
	return (GLenum)ret;
}

void Texture::setAnisotropy(GLfloat _anisotropy)
{
	APT_ASSERT(m_handle);
	//if (GLEW_EXT_texture_filter_anisotropic) {
		float mx;
		glAssert(glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &mx));
		glAssert(glTextureParameterf(m_handle, GL_TEXTURE_MAX_ANISOTROPY_EXT, APT_CLAMP(_anisotropy, 1.0f, mx)));
	//}
}

GLfloat Texture::getAnisotropy() const
{
	APT_ASSERT(m_handle);
	GLfloat ret = -1.0f;
	//if (GLEW_EXT_texture_filter_anisotropic) {
		glAssert(glGetTextureParameterfv(m_handle, GL_TEXTURE_MAX_ANISOTROPY_EXT, &ret));
	//}
	return ret;
}

void Texture::setWrap(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_S, (GLint)_mode));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_T, (GLint)_mode));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_R, (GLint)_mode));
}
void Texture::setWrapU(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_S, (GLint)_mode));
}
void Texture::setWrapV(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_T, (GLint)_mode));
}
void Texture::setWrapW(GLenum _mode)
{
	APT_ASSERT(m_handle);
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_WRAP_R, (GLint)_mode));
}
GLenum Texture::getWrapU() const
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_WRAP_S, &ret));
	return (GLenum)ret;

}
GLenum Texture::getWrapV() const
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_WRAP_T, &ret));
	return (GLenum)ret;
}
GLenum Texture::getWrapW() const
{
	APT_ASSERT(m_handle);
	GLint ret;
	glAssert(glGetTextureParameteriv(m_handle, GL_TEXTURE_WRAP_R, &ret));
	return (GLenum)ret;
}


// PROTECTED

Texture::Texture(uint64 _id, const char* _name)
	: Resource(_id, _name)
	, m_handle(0)
	, m_ownsHandle(true)
	, m_target(GL_NONE)
	, m_format((GLint)GL_NONE)
	, m_width(0), m_height(0), m_depth(0)
	, m_mipCount(0)
{
	APT_ASSERT(GlContext::GetCurrent());
}

Texture::Texture(
	uint64      _id, 
	const char* _name,
	GLenum      _target,
	GLsizei     _width,
	GLsizei     _height,
	GLsizei     _depth, 
	GLsizei     _arrayCount,
	GLsizei     _mipCount,
	GLenum      _format
	)
	: Resource(_id, _name)
{
	m_target     = _target;
	m_format     = _format;
	m_width      = _width;
	m_height     = _height;
	m_height     = _depth;
	m_arrayCount = _arrayCount;
	m_mipCount   = APT_MIN(_mipCount, GetMaxMipCount(_width, _height));
	glAssert(glCreateTextures(m_target, 1, &m_handle));

	switch (_target) {
		case GL_TEXTURE_1D:       glAssert(glTextureStorage1D(m_handle, _mipCount, _format, _width)); break;
		case GL_TEXTURE_1D_ARRAY: glAssert(glTextureStorage2D(m_handle, _mipCount, _format, _width, _arrayCount)); break;
		case GL_TEXTURE_2D:       glAssert(glTextureStorage2D(m_handle, _mipCount, _format, _width, _height)); break;
		case GL_TEXTURE_2D_ARRAY: glAssert(glTextureStorage3D(m_handle, _mipCount, _format, _width, _height, _arrayCount)); break;
		case GL_TEXTURE_3D:       glAssert(glTextureStorage3D(m_handle, _mipCount, _format, _width, _height, _depth)); break;
		case GL_TEXTURE_CUBE_MAP: glAssert(glTextureStorage2D(m_handle, _mipCount, _format, _width, _height)); break;
		default:                  APT_ASSERT(false); setState(State_Error); return;
	};
	
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, (_mipCount > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_BASE_LEVEL, 0));
	glAssert(glTextureParameteri(m_handle, GL_TEXTURE_MAX_LEVEL, _mipCount - 1));
	updateParams();
	
	setState(State_Loaded);
}

Texture::~Texture()
{
	if (m_ownsHandle && m_handle) {
		glAssert(glDeleteTextures(1, &m_handle));
		m_handle = 0;
	}
	setState(State_Unloaded);
}


bool Texture::isCompressed() const
{
	return GlIsTexFormatCompressed(m_format);
}
bool Texture::isDepth() const
{
	return GlIsTexFormatDepth(m_format);
}

void frm::swap(Texture& _a, Texture& _b)
{
	using eastl::swap;
	swap(_a.m_path, _b.m_path);

	swap(_a.m_handle,     _b.m_handle);
	swap(_a.m_ownsHandle, _b.m_ownsHandle);
	swap(_a.m_target,     _b.m_target);
	swap(_a.m_format,     _b.m_format);
	swap(_a.m_width,      _b.m_width);
	swap(_a.m_height,     _b.m_height);
	swap(_a.m_depth,      _b.m_depth);
	swap(_a.m_arrayCount, _b.m_arrayCount);
	swap(_a.m_mipCount,   _b.m_mipCount);
}

// PRIVATE

Texture* Texture::Create(
	GLenum  _target,
	GLsizei _width,
	GLsizei _height,
	GLsizei _depth, 
	GLsizei _arrayCount,
	GLsizei _mipCount,
	GLenum  _format
	)
{
	uint64 id = GetUniqueId();
	Texture* ret = new Texture(id, "", _target, _width, _height, _depth, _arrayCount, _mipCount, _format);
	ret->setNamef("%llu", id);
	Use(ret);
	g_textureViewer.addTextureView(ret);
	return ret;
}



static void Alloc1d(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage1D(_tx.getHandle(), (GLsizei)_tx.getMipCount(), _tx.getFormat(), _tx.getWidth()));
}
static void Alloc1dArray(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage2D(_tx.getHandle(), (GLsizei)_tx.getMipCount(), _tx.getFormat(), _tx.getWidth(), _tx.getArrayCount()));
}
static void Alloc2d(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage2D(_tx.getHandle(), (GLsizei)_tx.getMipCount(), _tx.getFormat(), _tx.getWidth(), _tx.getHeight()));
}
static void Alloc2dArray(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage3D(_tx.getHandle(), (GLsizei)_tx.getMipCount(), _tx.getFormat(), _tx.getWidth(), _tx.getHeight(), _tx.getArrayCount()));
}
static void Alloc3d(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage3D(_tx.getHandle(), (GLsizei)_tx.getMipCount(), _tx.getFormat(), _tx.getWidth(), _tx.getHeight(), _tx.getDepth()));
}
static void AllocCubemap(Texture& _tx, const Image& _img)
{
	glAssert(glTextureStorage2D(_tx.getHandle(), (GLsizei)_tx.getMipCount(), _tx.getFormat(), _tx.getWidth(), _tx.getHeight()));
}

#define Texture_COMPUTE_WHD() \
	GLsizei div = (GLsizei)pow(2.0, (double)_mip); \
	GLsizei w = APT_MAX(_tx.getWidth()  / div, (GLsizei)1); \
	GLsizei h = APT_MAX(_tx.getHeight() / div, (GLsizei)1);\
	GLsizei d = APT_MAX(_tx.getDepth()  / div, (GLsizei)1); 

static void Upload1d(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage1D(_tx.getHandle(), _mip, 0, w, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage1D(_tx.getHandle(), _mip, 0, w, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}
static void Upload1dArray(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage2D(_tx.getHandle(), _mip, 0, _array, w, 1, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage2D(_tx.getHandle(), _mip, 0, _array, w, 1, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}
static void Upload2d(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage2D(_tx.getHandle(), _mip, 0, _array, w, h, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage2D(_tx.getHandle(), _mip, 0, 0, w, h, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}
static void Upload2dArray(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage3D(_tx.getHandle(), _mip, 0, 0, _array, w, h, 1, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage3D(_tx.getHandle(), _mip, 0, 0, _array, w, h, 1, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}
static void Upload3d(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	if (_img.isCompressed()) {
		glAssert(glCompressedTextureSubImage3D(_tx.getHandle(), _mip, 0, 0, _array, w, h, d, _tx.getFormat(), (GLsizei)_img.getRawImageSize(_mip), _img.getRawImage(_array, _mip)));
	} else {
		glAssert(glTextureSubImage3D(_tx.getHandle(), _mip, 0, 0, _array, w, h, d, _srcFormat, _srcType, _img.getRawImage(_array, _mip)));
	}
}
static void UploadCubemap3x2(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType)
{
	Texture_COMPUTE_WHD();
	SCOPED_PIXELSTOREI(GL_UNPACK_ROW_LENGTH, w * 2);
	glAssert(glBindTexture(GL_TEXTURE_CUBE_MAP, _tx.getHandle()));

	int face = 0;
	for (int y = 0; y < 3; ++y) {
		for (int x = 0; x < 2; ++x) {
			char* src = _img.getRawImage(_array, _mip);
			src += x * w * (GLsizei)_img.getTexelSize();
			src += y * h * w * 2 * (GLsizei)_img.getTexelSize();
			glAssert(glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, _mip, 0, 0, w, h, _srcFormat, _srcType, src));
			++face;
		}
	}
}

#undef Texture_COMPUTE_WHD

bool Texture::loadImage(const Image& _img)
{
	SCOPED_PIXELSTOREI(GL_UNPACK_ALIGNMENT, 1);

 // metadata
	m_width      = (GLint)_img.getWidth();
	m_height     = (GLint)_img.getHeight();
	m_depth      = (GLint)_img.getDepth();
	m_arrayCount = (GLint)_img.getArrayCount();

	// \hack \todo always allocate a mip chain in case we call generateMipmap() later - make this optional?
	m_mipCount   = _img.getMipmapCount() == 1 ? (GLint)GetMaxMipCount(m_width, m_height, m_depth) : (GLint) _img.getMipmapCount();

 // target, alloc/upload dispatch functions
	void (*alloc)(Texture& _tx, const Image& _img);
	void (*upload)(Texture& _tx, const Image& _img, GLint _array, GLint _mip, GLenum _srcFormat, GLenum _srcType);

	if (m_target == GL_TEXTURE_CUBE_MAP) {
	 // special-case 3x2 cubemaps
		m_width /= 2;
		m_height /= 3;
		m_mipCount = _img.getMipmapCount() == 1 ? (GLint)GetMaxMipCount(m_width, m_height) : (GLint) _img.getMipmapCount();
		alloc = AllocCubemap;
		upload = UploadCubemap3x2;
	} else {
		switch (_img.getType()) {
			case Image::Type_1d:           m_target = GL_TEXTURE_1D;        alloc = Alloc1d;      upload = Upload1d; break;
			case Image::Type_1dArray:      m_target = GL_TEXTURE_1D_ARRAY;  alloc = Alloc1dArray; upload = Upload1dArray; break;
			case Image::Type_2d:           m_target = GL_TEXTURE_2D;        alloc = Alloc2d;      upload = Upload2d; break;
			case Image::Type_2dArray:      m_target = GL_TEXTURE_2D_ARRAY;  alloc = Alloc2dArray; upload = Upload2dArray; break;
			case Image::Type_3d:           m_target = GL_TEXTURE_3D;        alloc = Alloc3d;      upload = Upload3d; break;
		 // \todo implement cubemaps
			case Image::Type_Cubemap:      APT_ASSERT(false);//m_target = GL_TEXTURE_CUBE_MAP;       upload = Upload2d; break;
			case Image::Type_CubemapArray: APT_ASSERT(false);//m_target = GL_TEXTURE_CUBE_MAP_ARRAY; upload = Upload3d; break;
			default:                       APT_ASSERT(false); return false;
		};
	}

 // src format
	GLenum srcFormat;
	switch (_img.getLayout()) {
		case Image::Layout_R:          srcFormat = m_format = GL_RED;  break;
		case Image::Layout_RG:         srcFormat = m_format = GL_RG;   break;
		case Image::Layout_RGB:        srcFormat = m_format = GL_RGB;  break;
		case Image::Layout_RGBA:       srcFormat = m_format = GL_RGBA; break;
		default:                       APT_ASSERT(false); return false;
	};

 // internal format (request only, we read back the actual format the implementation used later)
	if (_img.isCompressed()) {
		switch (_img.getCompressionType()) {
			case Image::Compression_BC1: 
				switch (_img.getLayout()) {
					case Image::Layout_RGB:  m_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;       break;
					case Image::Layout_RGBA: m_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;      break;
					default:                 APT_ASSERT(false); return false;
				};
				break;
			case Image::Compression_BC2: m_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;      break;
			case Image::Compression_BC3: m_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;      break;
			case Image::Compression_BC4: m_format = GL_COMPRESSED_RED_RGTC1;               break;
			case Image::Compression_BC5: m_format = GL_COMPRESSED_RG_RGTC2;                break;
			case Image::Compression_BC6: m_format = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT; break;
			case Image::Compression_BC7: m_format = GL_COMPRESSED_RGBA_BPTC_UNORM;         break;
		};
	} else {
		switch (_img.getLayout()) {
			case Image::Layout_R:
				switch (_img.getImageDataType()) {
					case DataType::Float32: m_format = GL_R32F; break;
					case DataType::Float16: m_format = GL_R16F; break;
					case DataType::Uint16N: m_format = GL_R16;  break;
					default:                m_format = GL_R8;   break;
				};
				break;
			case Image::Layout_RG:
				switch (_img.getImageDataType()) {
					case DataType::Float32: m_format = GL_RG32F; break;
					case DataType::Float16: m_format = GL_RG16F; break;
					case DataType::Uint16N: m_format = GL_RG16;  break;
					default:                m_format = GL_RG8;   break;
				};
				break;
			case Image::Layout_RGB:			
				switch (_img.getImageDataType()) {
					case DataType::Float32: m_format = GL_RGB32F; break;
					case DataType::Float16: m_format = GL_RGB16F; break;
					case DataType::Uint16N: m_format = GL_RGB16;  break;
					default:                m_format = GL_RGB8;   break;
				};
				break;
			case Image::Layout_RGBA:
				switch (_img.getImageDataType()) {
					case DataType::Float32: m_format = GL_RGBA32F; break;
					case DataType::Float16: m_format = GL_RGBA16F; break;
					case DataType::Uint16N: m_format = GL_RGBA16;  break;
					default:                m_format = GL_RGBA8;   break;
				};
				break;
			default: break;
		};
	}
	
	GLenum srcType = _img.isCompressed() ? GL_UNSIGNED_BYTE : internal::GlDataTypeToEnum(_img.getImageDataType());

 // delete old handle, gen new handle (required since we use immutable storage)
	if (m_handle) {
		glAssert(glDeleteTextures(1, &m_handle));
	}

 // upload data; apt::Image stores each array layer contiguously with its mip chain, so we need to call
 // glTexSubImage* to upload each layer/mip separately
	glAssert(glCreateTextures(m_target, 1, &m_handle));
	alloc(*this, _img);
	for (GLint i = 0; i < _img.getArrayCount(); ++i) {		
		for (GLint j = 0; j < _img.getMipmapCount(); ++j) {
			upload(*this, _img, i, j, srcFormat, srcType);
		}
	}
	updateParams();

	setWrap(GL_REPEAT);
	setMagFilter(GL_LINEAR);
	setMinFilter(_img.getMipmapCount() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

	return true;
}

void Texture::updateParams()
{
	glAssert(glGetTextureLevelParameteriv(m_handle, 0, GL_TEXTURE_INTERNAL_FORMAT, &m_format));
	glAssert(glGetTextureLevelParameteriv(m_handle, 0, GL_TEXTURE_WIDTH,  &m_width));
	glAssert(glGetTextureLevelParameteriv(m_handle, 0, GL_TEXTURE_HEIGHT, &m_height));
	glAssert(glGetTextureLevelParameteriv(m_handle, 0, GL_TEXTURE_DEPTH, m_arrayCount > 1 ? &m_arrayCount : &m_depth));
}