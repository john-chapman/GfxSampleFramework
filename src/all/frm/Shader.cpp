#include <frm/Shader.h>

#include <frm/gl.h>
#include <frm/Camera.h> // Camera_Clip* define passed to shader
#include <frm/GlContext.h>
#include <frm/ShaderPreprocessor.h>

#include <apt/hash.h>
#include <apt/log.h>
#include <apt/math.h>
#include <apt/String.h>

#include <EASTL/vector.h>

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
						 // compute local size
							if (desc.hasStage(GL_COMPUTE_SHADER) && ImGui::TreeNode("Local Size")) {
								ivec3 sz = ivec3(sh->getLocalSizeX(), sh->getLocalSizeY(), sh->getLocalSizeZ());
								bool reload = false;
								if (ImGui::InputInt3("Local Size", &sz.x, ImGuiInputTextFlags_EnterReturnsTrue)) {
									sh->setLocalSize(sz.x, sz.y, sz.z);
								}
								
								ImGui::TreePop();
							}
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

template <>
void ShaderDesc::addDefine<const char*>(GLenum _stage, const char* _name, const char* const & _value)
{
	int i = internal::ShaderStageToIndex(_stage);

 // find/modify an existing define
	for (auto& def : m_stages[i].m_defines) {
		if (def.find(_name)) {
			def.setf("%s %s", _name, _value);
			return;
		}
	}

 // not found, push a new one
	m_stages[i].m_defines.push_back(StageDesc::DefineStr("%s %s", _name, _value));
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
				m_stages[i].m_defines.push_back(StageDesc::DefineStr(_defines));
			}
			_defines = strchr(_defines, 0);
			APT_ASSERT(_defines);
			++_defines;
		}
	}
}

void ShaderDesc::clearDefines()
{
	for (int i = 0; i < internal::kShaderStageCount; ++i) {
		m_stages[i].m_defines.clear();
	}
}

void ShaderDesc::setPath(GLenum _stage, const char* _path)
{
	int i = internal::ShaderStageToIndex(_stage);
	m_stages[i].m_path.set(_path);
}

void ShaderDesc::setSource(GLenum _stage, const char* _src)
{
	int i = internal::ShaderStageToIndex(_stage);
	m_stages[i].m_source.clear();
	while (*_src) {
		m_stages[i].m_source.push_back(*_src);
		++_src;
	}
	m_stages[i].m_source.push_back('\0');
}

uint64 ShaderDesc::getHash() const 
{
	uint64 ret = HashString<uint64>((const char*)m_version);
	
	for (int i = 0; i < internal::kShaderStageCount; ++i) {
		const StageDesc& stage = m_stages[i];
		if (!stage.isEnabled()) {
			continue;
		}

		if (!stage.m_path.isEmpty()) {
			ret = HashString<uint64>((const char*)stage.m_path, ret);
		}
		for (auto def = stage.m_defines.begin(); def != stage.m_defines.end(); ++def) {
			ret = HashString<uint64>((const char*)*def, ret);
		}
	}
	return ret;
}

bool ShaderDesc::hasStage(GLenum _stage) const
{
	int i = internal::ShaderStageToIndex(_stage);
	return m_stages[i].isEnabled();
}

const char* ShaderDesc::getPath(GLenum _stage) const
{
	int i = internal::ShaderStageToIndex(_stage);
	APT_ASSERT(m_stages[i].isEnabled());
	return m_stages[i].m_path;
}

int ShaderDesc::getDefineCount(GLenum _stage) const
{
	return (int)m_stages[internal::ShaderStageToIndex(_stage)].m_defines.size();
}

const char* ShaderDesc::getDefine(GLenum _stage, int _i) const
{
	int stage = internal::ShaderStageToIndex(_stage);
	if (_i >= (int)m_stages[stage].m_defines.size()) {
		return "Invalid stage/index";
	}
	return m_stages[stage].m_defines[_i];
}

const char* ShaderDesc::getSource(GLenum _stage) const
{
	int stage = internal::ShaderStageToIndex(_stage);
	if (!m_stages[stage].isEnabled()) {
		return "Invalid stage";
	}
	return m_stages[internal::ShaderStageToIndex(_stage)].m_source.data();
}

// PRIVATE

ShaderDesc::VersionStr ShaderDesc::s_defaultVersion("430");

bool ShaderDesc::StageDesc::isEnabled() const
{
	return !(m_path.isEmpty() && m_source.size() == 0);
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
	APT_ASSERT((_localX * _localY * _localZ) <= GlContext::GetCurrent()->kMaxComputeInvocations);

	ShaderDesc desc;
	desc.addGlobalDefines(_defines);
	desc.setPath(GL_COMPUTE_SHADER, _csPath);
	desc.addDefine(GL_COMPUTE_SHADER, "LOCAL_SIZE_X", _localX);
	desc.addDefine(GL_COMPUTE_SHADER, "LOCAL_SIZE_Y", _localY);
	desc.addDefine(GL_COMPUTE_SHADER, "LOCAL_SIZE_Z", _localZ);
	Shader* ret = Create(desc);
	ret->m_localSize[0] = _localX;
	ret->m_localSize[1] = _localY;
	ret->m_localSize[2] = _localZ;
	return ret;
}

void Shader::Destroy(Shader*& _inst_)
{
	delete _inst_;
}

bool Shader::reload()
{
	bool ret = true;

	if (m_name.isEmpty()) {
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
			APT_LOG_ERR("Program %d link failed:", handle);
			APT_LOG("\tstages:");
			for (int i = 0; i < internal::kShaderStageCount; ++i) {
				const ShaderDesc::StageDesc& stage = m_desc.m_stages[i];
				if (stage.isEnabled()) {
					APT_LOG("\t\t%s", internal::GlEnumStr(internal::kShaderStages[i]));
					if (!stage.m_path.isEmpty()) {
						APT_LOG("\t\tpath: '%s'", (const char*)stage.m_path);
					}
					if (stage.m_defines.size() > 0) {
						APT_LOG("\t\tdefines:");
						for (auto it = stage.m_defines.begin(); it != stage.m_defines.end(); ++it) {
							APT_LOG("\t\t\t%s", (const char*)*it);
						}
					}
				}
			}
			const char* log = GetProgramInfoLog(handle);
			APT_LOG("\tlog:\n%s", log);
			FreeProgramInfoLog(log);

			glAssert(glDeleteProgram(handle));
			if (m_handle == 0) {
			 // handle is 0, meaning we didn't successfully load a shader previously
				setState(State_Error);
			}
			ret = false;
			//APT_ASSERT(false);
		} else {
			APT_LOG("Program %d link succeeded.", handle);
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

void Shader::setLocalSize(int _x, int _y, int _z)
{
	APT_ASSERT(m_desc.hasStage(GL_COMPUTE_SHADER));
	m_desc.addDefine(GL_COMPUTE_SHADER, "LOCAL_SIZE_X", _x);
	m_desc.addDefine(GL_COMPUTE_SHADER, "LOCAL_SIZE_Y", _y);
	m_desc.addDefine(GL_COMPUTE_SHADER, "LOCAL_SIZE_Z", _z);
	if (loadStage(internal::ShaderStageToIndex(GL_COMPUTE_SHADER), false)) {
		m_localSize[0] = _x;
		m_localSize[1] = _y;
		m_localSize[2] = _z;
	}
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


static void Append(const char* _str, eastl::vector<char>& _out_)
{
	while (*_str) {
		_out_.push_back(*_str);
		++_str;
	}
}
static void AppendLine(const char* _str, eastl::vector<char>& _out_)
{
	Append(_str, _out_);
	_out_.push_back('\n');
}

bool Shader::loadStage(int _i, bool _loadSource)
{
	ShaderDesc::StageDesc& desc = m_desc.m_stages[_i];
	APT_ASSERT(desc.isEnabled());

 // process source file if required
	if (_loadSource && !desc.m_path.isEmpty()) {
		ShaderPreprocessor sp;
		if (!sp.process(desc.m_path)) {
			return false;
		}
		const char* src = sp.getResult();
		desc.m_source.clear();
		while (*src) {
			desc.m_source.push_back(*src);
			++src;
		}
		desc.m_source.push_back('\0');
	}

 // build final source
	eastl::vector<char> src;
	Append("#version ", src);
	AppendLine(m_desc.m_version, src);
	for (auto it = desc.m_defines.begin(); it != desc.m_defines.end(); ++it) {
		Append("#define ", src);
		AppendLine(*it, src);
	}
	
	Append("#define ", src);
	AppendLine(internal::GlEnumStr(internal::kShaderStages[_i]) + 3, src); // \hack +3 removes the 'GL_', which is reserved in the shader

	Append("#define ", src);
	#if   defined(Camera_ClipD3D)
		AppendLine("Camera_ClipD3D", src);
	#elif defined(Camera_ClipOGL)
		AppendLine("Camera_ClipOGL", src);
	#endif

	Append(desc.m_source.data(), src);
	src.push_back('\n');
	src.push_back('\0');

 // gen stage handle if required
	if (m_stageHandles[_i] == 0) {
		glAssert(m_stageHandles[_i] = glCreateShader(internal::kShaderStages[_i]));
	}

 // upload source code
	const GLchar* pd = src.data();
	GLint ps = (GLint)src.size();
	glAssert(glShaderSource(m_stageHandles[_i], 1, &pd, &ps));

 // compile
	glAssert(glCompileShader(m_stageHandles[_i]));
	GLint ret = GL_FALSE;
	glAssert(glGetShaderiv(m_stageHandles[_i], GL_COMPILE_STATUS, &ret));
	
 // print info log on fail
	if (ret == GL_FALSE) {		
		//APT_LOG_DBG("\tsrc: \n\n%s", (const char*)src.data());
		
		APT_LOG_ERR("%s %d compile failed:", internal::GlEnumStr(internal::kShaderStages[_i]), m_stageHandles[_i]);
		if (!desc.m_path.isEmpty()) {
			APT_LOG("\tpath: '%s'", (const char*)desc.m_path);
		}
		if (desc.m_defines.size() > 0) {
			APT_LOG("\tdefines:");
			for (auto it = desc.m_defines.begin(); it != desc.m_defines.end(); ++it) {
				APT_LOG("\t\t%s", (const char*)*it);
			}
		}
		const char* log = GetStageInfoLog(m_stageHandles[_i]);
		APT_LOG("\tlog: %s", log);
		FreeStageInfoLog(log);

		//APT_ASSERT(false);

	} else {
		APT_LOG("%s %d compile succeeded.", internal::GlEnumStr(internal::kShaderStages[_i]), m_stageHandles[_i]);
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
