#include <frm/ui/ShaderViewer.h>

#include <frm/gl.h>
#include <frm/Shader.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace ui;
using namespace apt;


void ShaderViewer::draw(bool* _open_)
{
	static const vec4 kColorStage[] =
	{
		vec4(0.2f, 0.2f, 0.8f, 1.0f),//GL_COMPUTE_SHADER,
		vec4(0.3f, 0.7f, 0.1f, 1.0f),//GL_VERTEX_SHADER,
		vec4(0.5f, 0.5f, 0.0f, 1.0f),//GL_TESS_CONTROL_SHADER,
		vec4(0.5f, 0.5f, 0.0f, 1.0f),//GL_TESS_EVALUATION_SHADER,
		vec4(0.7f, 0.2f, 0.7f, 1.0f),//GL_GEOMETRY_SHADER,
		vec4(0.7f, 0.3f, 0.1f, 1.0f) //GL_FRAGMENT_SHADER
	};
	
	ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetItemsLineHeightWithSpacing()), ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2), ImGuiSetCond_FirstUseEver);
	if (!ImGui::Begin("Shader Viewer", _open_)) {
		ImGui::End();
		return; // window collapsed, early-out
	}

	ImGuiIO& io = ImGui::GetIO();

	ImGui::AlignFirstTextHeightToWidgets();
	ImGui::Text("%d shaders", Shader::GetInstanceCount());
	ImGui::SameLine();
	if (ImGui::Button("Reload All (F9)")) {
		Shader::ReloadAll();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Show Hidden", &m_showHidden);

	ImGui::Separator();
	ImGui::Spacing();

	ImGui::BeginChild("shaderList", ImVec2(ImGui::GetWindowWidth() * 0.2f, 0.0f), true);
		static ImGuiTextFilter filter;
		filter.Draw("Filter##ShaderName");
		
		for (int i = 0; i < Shader::GetInstanceCount(); ++i) {
			Shader* sh = Shader::GetInstance(i);
			const ShaderDesc& desc = sh->getDesc();

			if (!filter.PassFilter(sh->getName())) {
				continue;
			}

			if (sh->getName()[0] == '#' && !m_showHidden) {
				continue;
			}

		 // color list items by the last active shader stage
			//vec4 col = kColorStage[5];
			//for (int stage = 0; stage < internal::kShaderStageCount; ++stage) {
			//	if (desc.hasStage(internal::kShaderStages[stage])) {
			//		col = kColorStage[stage] * 2.0f;
			//	}
			//}			
			//ImGui::PushStyleColor(ImGuiCol_Text, col);
				ImGui::Selectable(sh->getName(), i == m_selectedShader);
			//ImGui::PopStyleColor();
			
			if (ImGui::IsItemClicked()) {
				m_selectedShader = i;
				if (m_selectedStage != -1 && !desc.hasStage(m_selectedStage)) {
					m_selectedStage = -1;
				}
			}
		}
	ImGui::EndChild();

	if (m_selectedShader != -1) {
		Shader* sh = Shader::GetInstance(m_selectedShader);
		const ShaderDesc& desc = sh->getDesc();

		ImGui::SameLine();
		ImGui::BeginChild("programInfo");
			for (int i = 0; i < internal::kShaderStageCount; ++i) {
				if (desc.hasStage(internal::kShaderStages[i])) {
					ImGui::SameLine();
					vec4 col = kColorStage[i] * (m_selectedStage == internal::kShaderStages[i] ? 1.0f : 0.75f);
					ImGui::PushStyleColor(ImGuiCol_Button, col);
						if (ImGui::Button(internal::GlEnumStr(internal::kShaderStages[i]) + 3) || m_selectedStage == -1) {
							m_selectedStage = internal::kShaderStages[i];
						}
					ImGui::PopStyleColor();
				}
			}
			
			ImGui::SameLine();
			if (ImGui::Button("Reload")) {
				sh->reload();
			}
			
			if (m_selectedStage != -1) {
				ImGui::PushStyleColor(ImGuiCol_Border, kColorStage[internal::ShaderStageToIndex(m_selectedStage)]);
					
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
					ImGui::BeginChild("stageInfo", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_AlwaysAutoResize);
					 // defines
						if (desc.getDefineCount(m_selectedStage) > 0 && ImGui::TreeNode("Defines")) {
							for (int i = 0, n = desc.getDefineCount(m_selectedStage); i < n; ++i) {
								ImGui::Text(desc.getDefine(m_selectedStage, i));
							}

							ImGui::TreePop();
						}
					 // resources
						if (sh->getHandle() != 0) { // introspection only if shader was loaded
							static const int kMaxResNameLength = 128;
							char resName[kMaxResNameLength];

							GLint uniformCount;
							static bool s_showBlockUniforms = false;
							glAssert(glGetProgramInterfaceiv(sh->getHandle(), GL_UNIFORM, GL_ACTIVE_RESOURCES, &uniformCount));
							if (uniformCount > 0 && ImGui::TreeNode("Uniforms")) {
								ImGui::Checkbox("Show Block Uniforms", &s_showBlockUniforms);
								ImGui::Columns(5);
								ImGui::Text("Name");     ImGui::NextColumn();
								ImGui::Text("Index");    ImGui::NextColumn();
								ImGui::Text("Location"); ImGui::NextColumn();
								ImGui::Text("Type");     ImGui::NextColumn();
								ImGui::Text("Count");    ImGui::NextColumn();
								ImGui::Separator();

								for (int i = 0; i < uniformCount; ++i) {
									GLenum type;
									GLint count;
									GLint loc;
									glAssert(glGetActiveUniform(sh->getHandle(), i, kMaxResNameLength - 1, 0, &count, &type, resName));
									glAssert(loc = glGetProgramResourceLocation(sh->getHandle(), GL_UNIFORM, resName));
									if (loc == -1 && !s_showBlockUniforms) {
										continue;
									}
									ImGui::Text(resName);                   ImGui::NextColumn();
									ImGui::Text("%d", i);                   ImGui::NextColumn();
									ImGui::Text("%d", loc);                 ImGui::NextColumn();
									ImGui::Text(internal::GlEnumStr(type)); ImGui::NextColumn();
									ImGui::Text("[%d]", count);             ImGui::NextColumn();
								}

								ImGui::Columns(1);
								ImGui::TreePop();
								ImGui::Spacing();
							}

							GLint uniformBlockCount;
							glAssert(glGetProgramInterfaceiv(sh->getHandle(), GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &uniformBlockCount));
							if (uniformBlockCount > 0 && ImGui::TreeNode("Uniform Blocks")) {
								ImGui::Columns(3);
								ImGui::Text("Name");         ImGui::NextColumn();
								ImGui::Text("Index");        ImGui::NextColumn();
								ImGui::Text("Size");         ImGui::NextColumn();
								ImGui::Separator();

								for (int i = 0; i < uniformBlockCount; ++i) {
									glAssert(glGetProgramResourceName(sh->getHandle(), GL_UNIFORM_BLOCK, i, kMaxResNameLength - 1, 0, resName));
									static const GLenum kProps[] = { GL_BUFFER_DATA_SIZE };
									GLint props[1];
									glAssert(glGetProgramResourceiv(sh->getHandle(), GL_UNIFORM_BLOCK, i, 1, kProps, 1, 0, props));
									ImGui::Text(resName);              ImGui::NextColumn();
									ImGui::Text("%d", i);              ImGui::NextColumn();
									ImGui::Text("%d bytes", props[0]); ImGui::NextColumn();
								}

								ImGui::Columns(1);
								ImGui::TreePop();
								ImGui::Spacing();
							}

							GLint storageBlockCount;
							glAssert(glGetProgramInterfaceiv(sh->getHandle(), GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &storageBlockCount));
							if (storageBlockCount> 0 && ImGui::TreeNode("Shader Storage Blocks")) {
								ImGui::Columns(3);
								ImGui::Text("Name");     ImGui::NextColumn();
								ImGui::Text("Index");    ImGui::NextColumn();
								ImGui::Text("Size");     ImGui::NextColumn();
								ImGui::Separator();

								for (int i = 0; i < storageBlockCount; ++i) {
									glAssert(glGetProgramResourceName(sh->getHandle(), GL_SHADER_STORAGE_BLOCK, i, kMaxResNameLength - 1, 0, resName));
									static const GLenum kProps[] = { GL_BUFFER_DATA_SIZE };
									GLint props[1];
									glAssert(glGetProgramResourceiv(sh->getHandle(), GL_SHADER_STORAGE_BLOCK, i, 1, kProps, 1, 0, props));
									ImGui::Text(resName); ImGui::NextColumn();
									ImGui::Text("%d", i);        ImGui::NextColumn();
									ImGui::Text("%d bytes", props[0]); ImGui::NextColumn();
								}

								ImGui::Columns(1);
								ImGui::TreePop();
								ImGui::Spacing();
							}
						} // if (sh->getHandle() != 0)


						if (ImGui::TreeNode("Source")) {
							ImGui::TextUnformatted(desc.getSource(m_selectedStage));

							ImGui::TreePop();
						}

					ImGui::EndChild();

				ImGui::PopStyleColor();
			}
		ImGui::EndChild();

	}


	ImGui::End();
}
