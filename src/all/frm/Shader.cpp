#include <frm/Shader.h>

#include <frm/gl.h>
#include <frm/GlContext.h>
#include <frm/ShaderPreprocessor.h>

#include <apt/hash.h>
#include <apt/log.h>
#include <apt/math.h>
#include <apt/String.h>

using namespace frm;
using namespace apt;

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
				setState(State::kError);
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
			setState(State::kLoaded);
		}
	} else {
		if (m_handle == 0) {
			setState(State::kError);
		}
	}
	return ret;
}


GLint Shader::getResourceIndex(GLenum _type, const char* _name) const
{
	APT_ASSERT(getState() == State::kLoaded);
	if (getState() != State::kLoaded) {
		return -1;
	}
	GLint ret = 0;
	glAssert(ret = glGetProgramResourceIndex(m_handle, _type, _name));
	return ret;
}

GLint Shader::getUniformLocation(const char* _name) const
{
	APT_ASSERT(getState() == State::kLoaded);
	if (getState() != State::kLoaded) {
		return -1;
	}
	GLint ret = 0;
	glAssert(ret = glGetUniformLocation(m_handle, _name));
	return ret;
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
	setState(State::kUnloaded);
}


static void Append(const char* _str, std::vector<char>& _out_)
{
	while (*_str) {
		_out_.push_back(*_str);
		++_str;
	}
}

static void AppendLine(const char* _str, std::vector<char>& _out_)
{
	Append(_str, _out_);
	_out_.push_back('\n');
}

bool Shader::loadStage(int _i)
{
	ShaderDesc::StageDesc& desc = m_desc.m_stages[_i];
	APT_ASSERT(desc.isEnabled());

 // process source file if required
	if (!desc.m_path.isEmpty()) {
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
	std::vector<char> src;
	Append("#version ", src);
	AppendLine(m_desc.m_version, src);
	for (auto it = desc.m_defines.begin(); it != desc.m_defines.end(); ++it) {
		Append("#define ", src);
		AppendLine(*it, src);
	}
	
	Append("#define ", src);
	AppendLine(internal::GlEnumStr(internal::kShaderStages[_i]) + 3, src); // \hack +3 removes the 'GL_', which is reserved in the shader

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
