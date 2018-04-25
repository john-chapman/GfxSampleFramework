#include <frm/Shader.h>

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/GlContext.h>
#include <frm/Texture.h>

#include <apt/hash.h>
#include <apt/log.h>
#include <apt/math.h>
#include <apt/FileSystem.h>
#include <apt/String.h>
#include <apt/TextParser.h>

#include <EASTL/vector.h>
#include <EASTL/vector_map.h>

#include <imgui/imgui.h>

using namespace frm;
using namespace apt;

/*******************************************************************************

                               ShaderViewer

*******************************************************************************/
struct ShaderViewer
{
	bool   m_showHidden;
	int    m_selectedShader;
	GLenum m_selectedStage;

	ShaderViewer()
		: m_showHidden(false)
		, m_selectedShader(-1)
		, m_selectedStage(-1)
	{
	}

	void draw(bool* _open_)	
	{
		static ImGuiTextFilter filter;
		static const vec4 kColorStage[] =
		{
			vec4(0.2f, 0.2f, 0.8f, 1.0f),//GL_COMPUTE_SHADER,
			vec4(0.3f, 0.7f, 0.1f, 1.0f),//GL_VERTEX_SHADER,
			vec4(0.5f, 0.5f, 0.0f, 1.0f),//GL_TESS_CONTROL_SHADER,
			vec4(0.5f, 0.5f, 0.0f, 1.0f),//GL_TESS_EVALUATION_SHADER,
			vec4(0.7f, 0.2f, 0.7f, 1.0f),//GL_GEOMETRY_SHADER,
			vec4(0.7f, 0.3f, 0.1f, 1.0f) //GL_FRAGMENT_SHADER
		};
		
		ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing()), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Shader Viewer", _open_)) {
			ImGui::End();
			return; // window collapsed, early-out
		}

		ImGuiIO& io = ImGui::GetIO();

		ImGui::AlignTextToFramePadding();
		ImGui::Text("%d shaders", Shader::GetInstanceCount());
		ImGui::SameLine();
		ImGui::Checkbox("Show Hidden", &m_showHidden);
		ImGui::SameLine();
		ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.2f);
			filter.Draw("Filter##ShaderName");
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Reload All (F9)")) {
			Shader::ReloadAll();
		}

		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginChild("shaderList", ImVec2(ImGui::GetWindowWidth() * 0.2f, 0.0f), true);
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
				for (int i = 0; i < frm::internal::kShaderStageCount; ++i) {
					if (desc.hasStage(frm::internal::kShaderStages[i])) {
						ImGui::SameLine();
						vec4 col = kColorStage[i] * (m_selectedStage == frm::internal::kShaderStages[i] ? 1.0f : 0.75f);
						ImGui::PushStyleColor(ImGuiCol_Button, col);
							if (ImGui::Button(frm::internal::GlEnumStr(frm::internal::kShaderStages[i]) + 3) || m_selectedStage == -1) {
								m_selectedStage = frm::internal::kShaderStages[i];
							}
						ImGui::PopStyleColor();
					}
				}
				
				ImGui::SameLine();
				if (ImGui::Button("Reload")) {
					sh->reload();
				}
				
				if (m_selectedStage != -1) {
					ImGui::PushStyleColor(ImGuiCol_Border, kColorStage[frm::internal::ShaderStageToIndex(m_selectedStage)]);
						
						ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
						ImGui::BeginChild("stageInfo", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_AlwaysAutoResize);
						 // dependencies
							if (ImGui::TreeNode("Dependencies")) {
								for (int i = 0, n = desc.getDependencyCount(m_selectedStage); i < n; ++i) {
									ImGui::Text("(%d) '%s'", i, desc.getDependency(m_selectedStage, i));
								}

								ImGui::TreePop();
							}
						 // compute local size
							if (desc.hasStage(GL_COMPUTE_SHADER) && ImGui::TreeNode("Local Size")) {
								ivec3 sz = sh->getLocalSize();
								bool reload = false;
								if (ImGui::InputInt3("Local Size", &sz.x, ImGuiInputTextFlags_EnterReturnsTrue)) {
									sh->setLocalSize(sz.x, sz.y, sz.z);
								}
								
								ImGui::TreePop();
							}
						 // defines
							if (desc.getDefineCount(m_selectedStage) > 0 && ImGui::TreeNode("Defines")) {
								for (int i = 0, n = desc.getDefineCount(m_selectedStage); i < n; ++i) {
									ImGui::Text("%s -- %s", desc.getDefineName(m_selectedStage, i), desc.getDefineValue(m_selectedStage, i));
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
										ImGui::Text(resName);                        ImGui::NextColumn();
										ImGui::Text("%d", i);                        ImGui::NextColumn();
										ImGui::Text("%d", loc);                      ImGui::NextColumn();
										ImGui::Text(frm::internal::GlEnumStr(type)); ImGui::NextColumn();
										ImGui::Text("[%d]", count);                  ImGui::NextColumn();
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
};

static ShaderViewer g_shaderViewer;
void Shader::ShowShaderViewer(bool* _open_)
{
	g_shaderViewer.draw(_open_);	
}

/*******************************************************************************

                                ShaderDesc

*******************************************************************************/

// PUBLIC

void ShaderDesc::SetDefaultVersion(const char* _version)
{
	s_defaultVersion.set(_version);
}


ShaderDesc::ShaderDesc()
	: m_version(GetDefaultVersion())
	, m_localSize(1) 
{
	for (int i = 0; i < internal::kShaderStageCount; ++i) {
		m_stages[i].m_stage = internal::kShaderStages[i];
	}
}

void ShaderDesc::setVersion(const char* _version) 
{
	m_version = _version;
}

void ShaderDesc::setPath(GLenum _stage, const char* _path)
{
	int i = internal::ShaderStageToIndex(_stage);
	m_stages[i].m_path.set(_path);
}

const char* ShaderDesc::getPath(GLenum _stage) const
{
	int i = internal::ShaderStageToIndex(_stage);
	APT_ASSERT(m_stages[i].isEnabled());
	return (const char*)m_stages[i].m_path;
}

void ShaderDesc::setSource(GLenum _stage, const char* _src)
{
	int i = internal::ShaderStageToIndex(_stage);
	m_stages[i].m_source.set(_src);
}

const char* ShaderDesc::getSource(GLenum _stage) const
{
	int stage = internal::ShaderStageToIndex(_stage);
	if (!m_stages[stage].isEnabled()) {
		return nullptr;
	}
	return (const char*)m_stages[internal::ShaderStageToIndex(_stage)].m_source;
}

int ShaderDesc::getDependencyCount(GLenum _stage) const
{
	return (int)m_stages[internal::ShaderStageToIndex(_stage)].m_dependencies.size();
}
const char* ShaderDesc::getDependency(GLenum _stage, int _i) const
{
	auto& stage = m_stages[internal::ShaderStageToIndex(_stage)];
	APT_ASSERT(_i < stage.m_dependencies.size());
	return (const char*)stage.m_dependencies[_i];
}
bool ShaderDesc::hasDependency(const char* _path) const
{
	for (auto& stage : m_stages) {
		if (stage.isEnabled()) {
			if (stage.hasDependency(_path)) {
				return true;
			}
		}
	}
	return false;
}

template <>
void ShaderDesc::addDefine<const char*>(GLenum _stage, const char* _name, const char* const& _value)
{
	StageDesc& stage = m_stages[internal::ShaderStageToIndex(_stage)];
	auto val = stage.m_defines.find(_name);
	if (val == stage.m_defines.end()) {
		stage.m_defines.insert(eastl::make_pair(Str(_name), Str(_value)));
	} else {
		val->second.set(_value);
	}
}

template <>
void ShaderDesc::addDefine<int>(GLenum _stage, const char* _name, const int& _value)
{
	addDefine(_stage, _name, (const char*)String<8>("%d", _value));
}
template <>
void ShaderDesc::addDefine<uint>(GLenum _stage, const char* _name, const uint& _value)
{
	addDefine(_stage, _name, (const char*)String<8>("%u", _value));
}
template <>
void ShaderDesc::addDefine<float>(GLenum _stage, const char* _name, const float& _value)
{
	addDefine(_stage, _name, (const char*)String<12>("%f", _value));
}
template <>
void ShaderDesc::addDefine<vec2>(GLenum _stage, const char* _name, const vec2& _value)
{
	addDefine(_stage, _name, (const char*)String<32>("%vec2(%f,%f)", _value.x, _value.y));
}
template <>
void ShaderDesc::addDefine<vec3>(GLenum _stage, const char* _name, const vec3& _value)
{
	addDefine(_stage, _name, (const char*)String<40>("%vec3(%f,%f,%f)", _value.x, _value.y, _value.z));
}
template <>
void ShaderDesc::addDefine<vec4>(GLenum _stage, const char* _name, const vec4& _value)
{
	addDefine(_stage, _name, (const char*)String<48>("%vec4(%f,%f,%f,%f)", _value.x, _value.y, _value.z, _value.w));
}

void ShaderDesc::addDefine(GLenum _stage, const char* _name)
{
	addDefine(_stage, _name, 1);
}

void ShaderDesc::addGlobalDefines(const char* _defines)
{
	if (_defines) {
		while (*_defines != '\0') {
			for (int i = 0; i < internal::kShaderStageCount; ++i) {
			 // split the string into name/value at the first whitespace
				TextParser tp = _defines;
				Str name, val;
				tp.advanceToNextWhitespace();
				name.set(_defines, tp.getCharCount());
				tp.skipWhitespace();
				val.set(tp);
				m_stages[i].m_defines.push_back(eastl::make_pair(name, val));
			}
			_defines = strchr(_defines, 0);
			APT_ASSERT(_defines);
			++_defines;
		}
	}
}

void ShaderDesc::clearDefines()
{
	for (auto& stage : m_stages) {
		stage.m_defines.clear();
	}
}

void ShaderDesc::clearDefines(GLenum _stage)
{
	m_stages[internal::ShaderStageToIndex(_stage)].m_defines.clear();
}

int ShaderDesc::getDefineCount(GLenum _stage) const
{
	return (int)m_stages[internal::ShaderStageToIndex(_stage)].m_defines.size();
}

const char* ShaderDesc::getDefineName(GLenum _stage, int _i) const
{
	auto& stage = m_stages[internal::ShaderStageToIndex(_stage)];
	APT_ASSERT(_i < stage.m_defines.size());
	return (const char*)(stage.m_defines.begin() + _i)->first;
}

const char* ShaderDesc::getDefineValue(GLenum _stage, int _i) const
{
	auto& stage = m_stages[internal::ShaderStageToIndex(_stage)];
	APT_ASSERT(_i < stage.m_defines.size());
	return (const char*)(stage.m_defines.begin() + _i)->second;
}

void ShaderDesc::setLocalSize(int _x, int _y, int _z)
{
	APT_ASSERT(hasStage(GL_COMPUTE_SHADER));
	m_localSize = ivec3(_x, _y, _z);
	addDefine(GL_COMPUTE_SHADER, "LOCAL_SIZE_X", _x);
	addDefine(GL_COMPUTE_SHADER, "LOCAL_SIZE_Y", _y);
	addDefine(GL_COMPUTE_SHADER, "LOCAL_SIZE_Z", _z);
}

void ShaderDesc::addVirtualInclude(const char* _name, const char* _value)
{
	auto val = m_vincludes.find(_name);
	if (val == m_vincludes.end()) {
		m_vincludes.insert(eastl::make_pair(Str(_name), Str(_value)));
	} else {
		val->second.set(_value);
	}
}

void ShaderDesc::clearVirtualIncludes()
{
	m_vincludes.clear();
}

uint64 ShaderDesc::getHash() const 
{
	uint64 ret = HashString<uint64>((const char*)m_version);
	
	for (auto& stage : m_stages) {
		if (!stage.isEnabled()) {
			continue;
		}
		if (!stage.m_path.isEmpty()) {
			ret = HashString<uint64>((const char*)stage.m_path, ret);
		}
		for (auto& def : stage.m_defines) {
			ret = HashString<uint64>((const char*)def.first, ret);
			ret = HashString<uint64>((const char*)def.second, ret);
		}
	}
	for (auto& vinc : m_vincludes) {
		ret = HashString<uint64>((const char*)vinc.first, ret);
		ret = HashString<uint64>((const char*)vinc.second, ret);
	}
	return ret;
}

bool ShaderDesc::hasStage(GLenum _stage) const
{
	int i = internal::ShaderStageToIndex(_stage);
	return m_stages[i].isEnabled();
}

const char* ShaderDesc::findVirtualInclude(const char* _name) const
{
	auto val = m_vincludes.find(_name);
	if (val == m_vincludes.end()) {
		return nullptr;
	}
	return (const char*)val->second;
}

// PRIVATE

ShaderDesc::VersionStr ShaderDesc::s_defaultVersion; // see GlContextImpl

bool ShaderDesc::StageDesc::isEnabled() const
{
	return !(m_path.isEmpty() && m_source.isEmpty());
}

bool ShaderDesc::StageDesc::hasDependency(const char* _path) const
{
	for (auto& dep : m_dependencies) {
		if (dep == _path) {
			return true;
		}
	}
	return false;
}

bool ShaderDesc::StageDesc::loadSource(const ShaderDesc& _shaderDesc, const char* _path)
{
	if (!_path) {
	 // first call without specifying the path loads from m_path
		m_dependencies.clear();
		_path = (const char*)m_path;
	}
	if (hasDependency(_path)) {
	 // already included, it's not an error we just skip it
		return true; 
	}

	File f;
	if (!FileSystem::Read(f, _path)) {
		return false;
	}

 // add to dependencies (keep depCount as the file number for the #line pragma)
	int depCount = (int)m_dependencies.size();
	m_dependencies.push_back(_path);

 // line pragma starts new file
	m_source.appendf("// -------- %s\n", _path);
	m_source.appendf("#line 1 %d\n", depCount);
	
 // process the file
	eastl::vector<char> tmp; // vector push_back has better perf than String append
	TextParser tp = f.getData();
	int lineCount = 1;
	int commentBlock = 0; // if >0 we're inside a comment block
	bool commentLine = false;
	while (!tp.isNull()) {
		if (tp.isLineEnd()) {
			++lineCount;
			commentLine = false;
		} else if (*tp == '/') {
		 // potential comment block/line comment
			if (tp[1] == '/') { // comment line
				commentLine = true;
			} else if (tp[1] == '*') { // comment block
				++commentBlock;
			}

		} else if (*tp == '*') {
		 // potential comment block ending
			if (tp[1] == '/') {
				--commentBlock;
				if (commentBlock < 0) {
					APT_LOG_ERR("Shader: Comment block error ('%s' line %d)", _path, lineCount);
					return false;
				}
			}

		} else if (*tp == '#' && commentBlock == 0 && !commentLine) {
		 // potential include directive
			if (strncmp(tp, "#include", sizeof("#include") - 1) == 0)  {
			 	tp.advanceToNextWhitespace();
				tp.skipWhitespace();
				
				if (*tp == '"') {
					tp.advance(); // step over '"'
					const char* beg = tp;
					if (tp.advanceToNext('"') != '"') {
						APT_LOG_ERR("Shader: error in #include directive ('%s' line %d)", _path, lineCount - 1);
						return false;
					}
					Str path;
					path.set(beg, tp - beg);
					tmp.push_back('\0');
					m_source.append(tmp.data());
					tmp.clear();
					if (!loadSource(_shaderDesc, (const char*)path)) {
						return false;
					}
				 // line pragma to resume
					m_source.appendf("\n// -------- %s\n", _path);
					m_source.appendf("#line %d %d\n", lineCount + 1, depCount);
				
				} else {
				 // no quotes = virtual include
					const char* beg = tp;
					tp.advanceToNextWhitespace();
					Str key;
					key.set(beg, tp - beg);
					const char* vinc = _shaderDesc.findVirtualInclude((const char*)key);
					if (!vinc) {
						APT_LOG_ERR("Shader: unknown virtual include '%s' ('%s' line %d)", (const char*)key, _path, lineCount - 1);
						return false;
					}
					tmp.push_back('\0');
					m_source.append(tmp.data());
					tmp.clear();
					m_source.append(vinc);

				 // line pragma to resume
					m_source.appendf("\n#line %d %d\n", lineCount, depCount);

				}
				tp.skipLine();
				++lineCount;

				continue; // don't advance tp or push to the result
			}
		}
		tmp.push_back(*tp);
		tp.advance();
	}
	tmp.push_back('\0');
	m_source.append(tmp.data());

	return true;
}

String<0> ShaderDesc::StageDesc::getLogInfo() const
{
	String<0> ret;
	ret.setCapacity(256);
	ret.appendf("\tstage: %s\n", internal::GlEnumStr(m_stage));
	if (!m_path.isEmpty()) {
		ret.appendf("\tpath: '%s'\n", (const char*)m_path);
	}
	if (m_dependencies.size() > 0) {
		ret.appendf("\tdependencies:\n");
		for (int i = 0, n = (int)m_dependencies.size(); i < n; ++i) {
			ret.appendf("\t\t(%d) '%s'\n", i, (const char*)m_dependencies[i]);
		}
	}
	if (m_defines.size() > 0) {
		ret.appendf("\tdefines:\n");
		for (auto& def : m_defines) {
			ret.appendf("\t\t%s  %s\n", (const char*)def.first, (const char*)def.second);
		}
	}
	return ret;
}

/*******************************************************************************

                                  Shader

*******************************************************************************/

// PUBLIC

Shader* Shader::Create(const ShaderDesc& _desc)
{
	Id id = _desc.getHash();
	Shader* ret = Find(id);
	if (!ret) {
		ret = new Shader(id, ""); // "" forces an auto generated name during reload()
		ret->m_desc = _desc;
	}
	Use(ret);
	return ret;
}
Shader* Shader::CreateVsFs(const char* _vsPath, const char* _fsPath, const char* _defines)
{
	ShaderDesc desc;
	desc.addGlobalDefines(_defines);
	desc.setPath(GL_VERTEX_SHADER, _vsPath);
	desc.setPath(GL_FRAGMENT_SHADER, _fsPath);
	return Create(desc);
}
Shader* Shader::CreateVsGsFs(const char* _vsPath, const char* _gsPath, const char* _fsPath, const char* _defines)
{
	ShaderDesc desc;
	desc.addGlobalDefines(_defines);
	desc.setPath(GL_VERTEX_SHADER, _vsPath);
	desc.setPath(GL_GEOMETRY_SHADER, _gsPath);
	desc.setPath(GL_FRAGMENT_SHADER, _fsPath);
	return Create(desc);
}
Shader* Shader::CreateCs(const char* _csPath, int _localX, int _localY, int _localZ, const char* _defines)
{
	APT_ASSERT(_localX <= GlContext::GetCurrent()->kMaxComputeLocalSize[0]);
	APT_ASSERT(_localY <= GlContext::GetCurrent()->kMaxComputeLocalSize[1]);
	APT_ASSERT(_localZ <= GlContext::GetCurrent()->kMaxComputeLocalSize[2]);
	APT_ASSERT((_localX * _localY * _localZ) <= GlContext::GetCurrent()->kMaxComputeInvocationsPerGroup);

	ShaderDesc desc;
	desc.setPath(GL_COMPUTE_SHADER, _csPath);
	desc.addGlobalDefines(_defines);
	desc.setLocalSize(_localX, _localY, _localZ);
	Shader* ret = Create(desc);
	return ret;
}

void Shader::Destroy(Shader*& _inst_)
{
	delete _inst_;
}

void Shader::FileModified(const char* _path)
{
	for (int i = 0, n = GetInstanceCount(); i < n; ++i) {
		auto shader = GetInstance(i);
		if (shader->getDesc().hasDependency(_path)) {
		 // \todo could potentially avoid reloading all stages
			shader->reload();
		}
	}
}

bool Shader::reload()
{
	bool ret = true;

	if (getName()[0] == '\0') {
		setAutoName();
	}

 // load stages
	for (int i = 0; i < internal::kShaderStageCount; ++i) {
		if (m_desc.m_stages[i].isEnabled()) {
			ret &= loadStage(i);
		}
	}

 // attach/link stages
	if (ret) {
		GLuint handle;
		glAssert(handle = glCreateProgram());
		for (int i = 0; i < internal::kShaderStageCount; ++i) {
			if (m_desc.m_stages[i].isEnabled()) {
				glAssert(glAttachShader(handle, m_stageHandles[i]));
			}
		}

		glAssert(glLinkProgram(handle));
		
		GLint linkStatus = GL_FALSE;
		glAssert(glGetProgramiv(handle, GL_LINK_STATUS, &linkStatus));
		if (linkStatus == GL_FALSE) {
			APT_LOG_ERR("'%s' link failed", getName());
			String<0> log("\tstages:\n");
			for (int i = 0; i < internal::kShaderStageCount; ++i) {
				const ShaderDesc::StageDesc& stage = m_desc.m_stages[i];
				if (stage.isEnabled()) {
					log.append((const char*)stage.getLogInfo());
				}
			}
			const char* programLog = GetProgramInfoLog(handle);
			log.append(programLog);
			FreeProgramInfoLog(programLog);
			APT_LOG("'%s' link error log:\n%s", getName(), (const char*)log);

			glAssert(glDeleteProgram(handle));
			if (m_handle == 0) {
			 // handle is 0, meaning we didn't successfully load a shader previously
				setState(State_Error);
			}

			ret = false;
			//APT_ASSERT(false);

		} else {
			APT_LOG("'%s' link succeeded", getName());
			if (m_handle != 0) {
				glAssert(glDeleteProgram(m_handle));
			}
			m_handle = handle;
			ret = true;
			setState(State_Loaded);
		}
	} else {
		if (m_handle == 0) {
			setState(State_Error);
		}
	}
	return ret;
}


GLint Shader::getResourceIndex(GLenum _type, const char* _name) const
{
	APT_ASSERT(getState() == State_Loaded);
	if (getState() != State_Loaded) {
		return -1;
	}
	GLint ret = 0;
	glAssert(ret = glGetProgramResourceIndex(m_handle, _type, _name));
	return ret;
}

GLint Shader::getUniformLocation(const char* _name) const
{
	APT_ASSERT(getState() == State_Loaded);
	if (getState() != State_Loaded) {
		return -1;
	}
	GLint ret = 0;
	glAssert(ret = glGetUniformLocation(m_handle, _name));
	return ret;
}

bool Shader::setLocalSize(int _x, int _y, int _z)
{
	APT_ASSERT(m_desc.hasStage(GL_COMPUTE_SHADER));
	m_desc.setLocalSize(_x, _y, _z);
	return loadStage(internal::ShaderStageToIndex(GL_COMPUTE_SHADER), false);
}

ivec3 Shader::getDispatchSize(int _outWidth, int _outHeight, int _outDepth)
{
	ivec3 localSize = getLocalSize();
	return APT_MAX((ivec3(_outWidth, _outHeight, _outDepth) + localSize - 1) / localSize, ivec3(1));
}

ivec3 Shader::getDispatchSize(const Texture* _tx, int _level)
{
	ivec3 localSize = getLocalSize();
	ivec3 ret = APT_MAX(ivec3(_tx->getWidth() >> _level, _tx->getHeight() >> _level, _tx->getDepth() >> _level), ivec3(1));
	return APT_MAX((ret + localSize - 1) / localSize, ivec3(1));
}

// PRIVATE

const char* Shader::GetStageInfoLog(GLuint _handle)
{
	GLint len;
	glAssert(glGetShaderiv(_handle, GL_INFO_LOG_LENGTH, &len));
	char* ret = new GLchar[len];
	glAssert(glGetShaderInfoLog(_handle, len, 0, ret));
	return ret;
}
void Shader::FreeStageInfoLog(const char*& _log_)
{
	delete[] _log_;
	_log_ = "";
}

const char* Shader::GetProgramInfoLog(GLuint _handle)
{
	GLint len;
	glAssert(glGetProgramiv(_handle, GL_INFO_LOG_LENGTH, &len));
	GLchar* ret = new GLchar[len];
	glAssert(glGetProgramInfoLog(_handle, len, 0, ret));
	return ret;

}
void Shader::FreeProgramInfoLog(const char*& _log_)
{
	delete[] _log_;
	_log_ = "";
}

Shader::Shader(uint64 _id, const char* _name)
	: Resource<Shader>(_id, _name)
	, m_handle(0)
{
	APT_ASSERT(GlContext::GetCurrent());
	memset(m_stageHandles, 0, sizeof(m_stageHandles));
}

Shader::~Shader()
{
	for (int i = 0; i < internal::kShaderStageCount; ++i) {
		if (m_stageHandles[i] != 0) {
			glAssert(glDeleteShader(m_stageHandles[i]));
			m_stageHandles[i] = 0;
		}
	}
	if (m_handle != 0) {
		glAssert(glDeleteProgram(m_handle));
		m_handle = 0;
	}
	setState(State_Unloaded);
}

bool Shader::loadStage(int _i, bool _loadSource)
{
	ShaderDesc::StageDesc& stageDesc = m_desc.m_stages[_i];
	APT_ASSERT(stageDesc.isEnabled());

 // process source file if required
	if (_loadSource && !stageDesc.m_path.isEmpty()) {
		stageDesc.m_source.clear();
		if (!stageDesc.loadSource(m_desc)) {
			return false;
		}
	}

 // build final source
	String<0> src;
	// version pragma
	src.appendf("#version %s\n", (const char*)m_desc.m_version);
	// defines
	for (auto& def : stageDesc.m_defines) {
		src.appendf("#define %s %s\n", (const char*)def.first, (const char*)def.second);
	}
	src.appendf("#define %s\n", internal::GlEnumStr(stageDesc.m_stage) + 3); // \hack +3 removes the 'GL_' which is reserved in the shader language
	
	src.append("#define ");
	#if   defined(FRM_NDC_Z_NEG_ONE_TO_ONE)
		src.append("FRM_NDC_Z_NEG_ONE_TO_ONE 1\n");
	#elif defined(FRM_NDC_Z_ZERO_TO_ONE)
		src.append("FRM_NDC_Z_ZERO_TO_ONE 1\n");
	#endif
	
	src.append((const char*)stageDesc.m_source);

	//APT_LOG_DBG((const char*)(src));

 // gen stage handle if required
	if (m_stageHandles[_i] == 0) {
		glAssert(m_stageHandles[_i] = glCreateShader(stageDesc.m_stage));
	}
 // upload source code
	const GLchar* pd = (const char*)src;
	GLint ps = (GLint)src.getLength();
	glAssert(glShaderSource(m_stageHandles[_i], 1, &pd, &ps));

 // compile
	glAssert(glCompileShader(m_stageHandles[_i]));
	GLint ret = GL_FALSE;
	glAssert(glGetShaderiv(m_stageHandles[_i], GL_COMPILE_STATUS, &ret));
	
 // print error log
	if (ret == GL_FALSE) {		
		const char* pth = apt::internal::StripPath((const char*)stageDesc.m_path);
		APT_LOG_ERR("'%s' compile failed", pth);
		auto log = stageDesc.getLogInfo();
		const char* stageLog = GetStageInfoLog(m_stageHandles[_i]);
		log.append(stageLog);
		FreeStageInfoLog(stageLog);
		APT_LOG("'%s' compilation error log:\n%s", pth, (const char*)log);

		#if 0
			APT_LOG_DBG("'%s' source:\n%s", pth, (const char*)src);
			APT_ASSERT(false);
		#endif

	} else {
		APT_LOG("'%s' compile succeeded", apt::internal::StripPath((const char*)stageDesc.m_path));
	}
	
	return ret == GL_TRUE;
}

void Shader::setAutoName()
{
	bool first = true;
	for (int i = 0; i < internal::kShaderStageCount; ++i) {
		if (m_desc.m_stages[i].isEnabled()) {
			if (!first) {
				m_name.append("__");
			}
			first = false;

		 	const ShaderDesc::StageDesc& stage = m_desc.m_stages[i];
			const char* beg = stage.m_path.findLast("/\\");
			const char* end = stage.m_path.findLast(".");
			beg = beg ? beg + 1 : (const char*)stage.m_path;
			end = end ? end : (const char*)stage.m_path + stage.m_path.getLength();

			m_name.append(beg, end - beg);
		}
	}
}
