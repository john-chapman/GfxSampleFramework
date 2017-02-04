#include <frm/ui/TextureViewer.h>

#include <frm/gl.h>
#include <frm/Shader.h>
#include <frm/Texture.h>

#include <apt/FileSystem.h>

#include <imgui/imgui.h>

				#include <frm/Input.h>

using namespace frm;
using namespace ui;
using namespace apt;

static std::vector<TextureView> m_txViews;

void TextureViewer::update()
{
 // \todo this is not scalable!

 // add any new textures
	for (int i = 0; i < Texture::GetInstanceCount(); ++i) {
		Texture* tx = Texture::GetInstance(i);
		if (!findTxView(tx)) {
			m_txViews.push_back(TextureView(tx));
		}
	}
 // erase any missing textures
	for (auto it = m_txViews.begin(); it != m_txViews.end(); ++it) {
		if (!Texture::Find(it->m_texture->getId())) {
			m_txViews.erase(it);
			m_selected = -1; // \todo don't just switch back to thumbnail mode
		}
	}
}

void TextureViewer::draw(bool* _open_)
{
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
		if (ImGui::Button("Reload All")) {
			Texture::ReloadAll();
		}
		ImGui::SameLine();
		if (ImGui::Button("Load")) {
			FileSystem::PathStr pth;
			if (FileSystem::PlatformSelect(pth)) {
				FileSystem::StripRoot(pth, pth);
				Texture::Create(pth);
			}
		}
		
		ImGui::Separator();
		
		update();
	
		bool first = true;
		for (unsigned i = 0; i < m_txViews.size(); ++i) {
			const TextureView*  txView = &m_txViews[i];
			const Texture* tx = txView->m_texture;
			if (!filter.PassFilter(tx->getName())) {
				continue;
			}
	
			if (tx->getName()[0] == '#' && !m_showHidden) {
				continue;
			}

		 // compute thumbnail size
			//float txAspect = (float)tx->getWidth() / (float)tx->getHeight();
			//float thumbWidth = kThumbHeight * (float)tx->getWidth() / (float)tx->getHeight();
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
			if (ImGui::ImageButton((ImTextureID)txView, thumbSize, ImVec2(0, 1), ImVec2(1, 0), 1, ImColor(0.5f, 0.5f, 0.5f))) {
				m_selected = i;
			}
		 // basic info tooltip
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
					ImGui::TextColored(kColorTxName, tx->getName());
					ImGui::TextColored(kColorTxInfo, "%s\n%s\n%dx%dx%d", internal::GlEnumStr(tx->getTarget()), internal::GlEnumStr(tx->getFormat()), tx->getWidth(), tx->getHeight(), APT_MAX(tx->getDepth(), tx->getArrayCount()));
				ImGui::EndTooltip();
			}
		}
	
	} else {
		APT_ASSERT(m_selected < m_txViews.size());
		TextureView* txView = &m_txViews[m_selected];
		Texture* tx = const_cast<Texture*>(txView->m_texture); // need to call reload, edit filter modes, etc.
		float txAspect = (float)tx->getWidth() / (float)tx->getHeight();

		if (ImGui::Button("<-")) {
			m_selected = -1;
		}
		if (*tx->getPath() != '\0') {
			ImGui::SameLine();
			if (ImGui::Button("Reload")) {
				tx->reload();
			}
			ImGui::SameLine();
			if (ImGui::Button("Replace")) {
				FileSystem::PathStr pth;
				if (FileSystem::PlatformSelect(pth)) {
					tx->setPath(pth);
					tx->reload();
				 // reset view
					txView->m_offset = vec2(0.0f);
					txView->m_size = vec2(tx->getWidth(), tx->getHeight());
					txView->m_array = 0;
					txView->m_mip = 0;
				}
			}
		}
		ImGui::SameLine();
		ImGui::Checkbox("Show Texel Grid", &m_showTexelGrid);
		ImGui::Separator();

		ImGui::Columns(2);
		float thumbWidth  = ImGui::GetContentRegionAvailWidth();
		float thumbHeight = (float)tx->getHeight() / (float)tx->getWidth() * thumbWidth;
		vec2  thumbSize(thumbWidth, APT_MAX(thumbHeight, 16.0f));
	  // need to flip the UVs here to account for the orientation of the quad output by ImGui
		vec2  uv0 = vec2(0.0f, 1.0f);
		vec2  uv1 = vec2(1.0f, 0.0f);
		if (ImGui::ImageButton((ImTextureID)txView, thumbSize, uv0, uv1, 0)) {
			//m_selected = -1;
		}
		if (m_showTexelGrid) {
			vec2 drawStart = ImGui::GetItemRectMin();
			vec2 drawEnd   = ImGui::GetItemRectMax();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRect(drawStart, drawEnd, kColorGrid);
			drawList->PushClipRect(drawStart, min(drawEnd, vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetWindowSize())));
				if ((drawEnd.x - drawStart.x) > (txView->m_size.x * 3.0f)) { // only draw grid if texel density is low enough
					float scale = thumbSize.x / txView->m_size.x;
					float bias  = (1.0f - fract(txView->m_offset.x)) * scale;
					for (int i = 0, n = (int)txView->m_size.x + 1; i <= n; ++i) {
						float x = drawStart.x + (float)i * scale + bias;
						drawList->AddLine(vec2(x, drawStart.y), vec2(x, drawEnd.y), kColorGrid);
					}
					scale = thumbSize.y / txView->m_size.y;
					bias  = (1.0f - fract(txView->m_offset.y)) * scale;
					for (int i = 0, n = (int)txView->m_size.y + 1; i <= n; ++i) {
						float y = drawEnd.y - (float)i * scale - bias;
						drawList->AddLine(vec2(drawStart.x, y), vec2(drawEnd.x, y), kColorGrid);
					}
				}	
			drawList->PopClipRect();
		}

		if (m_isDragging || ImGui::IsItemHovered()) {
		 // zoom
			vec2 txViewPos = ThumbToTxView(*txView);
			ImGui::BeginTooltip();
				ImGui::Text("%.1f, %.1f", txViewPos.x, txViewPos.y);
			ImGui::EndTooltip();
			vec2 offsetBeforeZoom = ThumbToTxView(*txView);
			vec2 zoomDelta = vec2(txAspect, 1.0f) * vec2(io.MouseWheel * kZoomSpeed);
			txView->m_size = max(txView->m_size - zoomDelta, vec2(txAspect * 4.0f, 4.0f));
			vec2 offsetAfterZoom = ThumbToTxView(*txView);
			vec2 offsetDelta = offsetBeforeZoom - offsetAfterZoom;
			txView->m_offset += offsetDelta;

		 // pan
			if (io.MouseDown[0]) {
				m_isDragging = true;
			}
		}
		if (m_isDragging) {
			if (!io.MouseDown[0]) {
				m_isDragging = false;
			}
			vec2 offset = vec2(io.MouseDelta.x, -io.MouseDelta.y) * vec2(txView->m_texture->getWidth(), txView->m_texture->getHeight()) / vec2(thumbWidth, thumbHeight) * txView->getNormalizedSize();
			txView->m_offset -= offset;
		}
		ImGui::NextColumn();
	
	 // zoom/pan
		if (ImGui::Button("Reset View")) {
			txView->m_offset = vec2(0.0f);
			txView->m_size = vec2(tx->getWidth(), tx->getHeight());
			txView->m_array = 0;
			txView->m_mip = 0;
		}
		ImGui::SameLine();
		ImGui::Text("Zoom: %1.2f%, %1.2f ", txView->m_size.x, txView->m_size.y);
		ImGui::SameLine();
		ImGui::Text("Pan: %1.2f,%1.2f", txView->m_offset.x, txView->m_offset.y);
		ImGui::Spacing();

	 // basic info
		ImGui::AlignFirstTextHeightToWidgets();
		ImGui::TextColored(kColorTxName, tx->getName());
		
		ImGui::TextColored(kColorTxInfo, "Id:     %llu",     tx->getId());
		ImGui::TextColored(kColorTxInfo, "Type:   %s",       internal::GlEnumStr(tx->getTarget()));
		ImGui::TextColored(kColorTxInfo, "Format: %s",       internal::GlEnumStr(tx->getFormat()));
		ImGui::TextColored(kColorTxInfo, "Size:   %dx%dx%d", tx->getWidth(), tx->getHeight(), tx->getDepth());
		ImGui::TextColored(kColorTxInfo, "Array:  %d",       tx->getArrayCount());
		ImGui::TextColored(kColorTxInfo, "Mips:   %d",       tx->getMipCount());	
		
	 // filter mode
		ImGui::Spacing(); ImGui::Spacing();
		int fm = internal::TextureFilterModeToIndex(tx->getMinFilter());
		if (ImGui::Combo("Min Filter", &fm, "NEAREST\0LINEAR\0NEAREST_MIPMAP_NEAREST\0LINEAR_MIPMAP_NEAREST\0NEAREST_MIPMAP_LINEAR\0LINEAR_MIPMAP_LINEAR\0")) { // must match order of internal::kTextureWrapModes (gl.cpp)
			tx->setMinFilter(internal::kTextureFilterModes[fm]);
		}
		fm = internal::TextureFilterModeToIndex(tx->getMagFilter());
		if (ImGui::Combo("Mag Filter", &fm, "NEAREST\0LINEAR\0")) { // must match order of internal::kTextureWrapModes (gl.cpp)
			tx->setMagFilter(internal::kTextureFilterModes[fm]);
		}

	 // anisotropy
		float aniso = tx->getAnisotropy();
		if (ImGui::SliderFloat("Anisotropy", &aniso, 1.0f, 16.0f)) {
			tx->setAnisotropy(aniso);
		}
	
	 // wrap mode
		ImGui::Spacing();
		static const char* kWrapItems = "REPEAT\0MIRRORED_REPEAT\0CLAMP_TO_EDGE\0MIRROR_CLAMP_TO_EDGE\0CLAMP_TO_BORDER\0"; // must match order of internal::kTextureWrapModes (gl.cpp)
		int wm = internal::TextureWrapModeToIndex(tx->getWrapU());
		if (ImGui::Combo("Wrap U", &wm, kWrapItems)) {
			tx->setWrapU(internal::kTextureWrapModes[wm]);
		}
		wm = internal::TextureWrapModeToIndex(tx->getWrapV());
		if (ImGui::Combo("Wrap V", &wm, kWrapItems)) {
			tx->setWrapV(internal::kTextureWrapModes[wm]);
		}
		if (tx->getDepth() > 1) {
			wm = internal::TextureWrapModeToIndex(tx->getWrapW());
			if (ImGui::Combo("Wrap W", &wm, kWrapItems)) {
				tx->setWrapW(internal::kTextureWrapModes[wm]);
			}
		}
	
	 // view options
		ImGui::Checkbox("R", &txView->m_rgbaMask[0]);
		ImGui::SameLine();
		ImGui::Checkbox("G", &txView->m_rgbaMask[1]);
		ImGui::SameLine();
		ImGui::Checkbox("B", &txView->m_rgbaMask[2]);
		ImGui::SameLine();
		ImGui::Checkbox("A", &txView->m_rgbaMask[3]);

		if (tx->getDepth() > 1) {
			ImGui::SliderInt("Layer", &txView->m_array, 0, tx->getDepth() - 1);
		}
		if (tx->getArrayCount() > 1) {
			ImGui::SliderInt("Array", &txView->m_array, 0, tx->getArrayCount() - 1);
		}
		if (tx->getMipCount() > 1) {
			ImGui::SliderInt("Mip", &txView->m_mip, 0, tx->getMipCount() - 1);
		}
	
		ImGui::Columns(1);
	}

	ImGui::End();
}

// PRIVATE

TextureView* TextureViewer::findTxView(const Texture* _tx)
{
	for (size_t i = 0; i < m_txViews.size(); ++i) {
		if (m_txViews[i].m_texture == _tx) {
			return &m_txViews[i];
		}
	}
	return 0;
}

vec2 TextureViewer::ThumbToTxView(const TextureView& _txView)
{
	ImGuiIO io = ImGui::GetIO();
	vec2 thumbPos;
	thumbPos.x = io.MousePos.x - ImGui::GetItemRectMin().x;
	thumbPos.y = ImGui::GetItemRectMax().y - io.MousePos.y;
	thumbPos /= vec2(ImGui::GetItemRectSize()); // y is inverted in thumbnail space
	return _txView.m_offset + thumbPos * _txView.m_size;
}