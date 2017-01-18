#pragma once
#ifndef frm_Shader_h
#define frm_Shader_h

#include <frm/def.h>
#include <frm/gl.h>
#include <frm/Resource.h>

#include <apt/String.h>

#include <vector>

namespace frm
{
	class GlContext;

////////////////////////////////////////////////////////////////////////////////
/// \class ShaderDesc
////////////////////////////////////////////////////////////////////////////////
class ShaderDesc
{
	friend class Shader;
public:
	/// Set the default version string, e.g. "420 compatibility".
	/// \note _str should not include the "#version" directive itself.
	static void SetDefaultVersion(const char* _version);
	static const char* GetDefaultVersion() { return s_defaultVersion; }

	ShaderDesc(): m_version(GetDefaultVersion()) {};

	/// Set the version string, e.g. "420 compatibility".
	/// \note _str should not include the "#version" directive itself.
	void setVersion(const char* _version);
	
	/// Add a define to the shader source for _stage. Supported types are int,
	/// uint, float, vec2, vec3, vec4.
	template <typename T>
	void addDefine(GLenum _stage, const char* _name, const T& _value);
	void addDefine(GLenum _stage, const char* _name);

	/// Add a define to all stages.
	template <typename T>
	void addGlobalDefine(const char* _name, const T& _value)
	{
		for (int i = 0; i < internal::kShaderStageCount; ++i) {
			addDefine(internal::kShaderStages[i], _name, _value);
		}
	}
	void addGlobalDefine(const char* _name)
	{
		for (int i = 0; i < internal::kShaderStageCount; ++i) {
			addDefine(internal::kShaderStages[i], _name);
		}
	}

	/// Process a null-separated list of define strings.
	void addGlobalDefines(const char* _defines);

	/// Clear defines for all stages.
	void clearDefines();

	/// Set source code path for _stage.
	void setPath(GLenum _stage, const char* _path);

	/// Set source code for _stage. This overrides existing source code.
	void setSource(GLenum _stage, const char* _src);

	/// \return A hash of the version string, shader paths, defines and source (if present).
	uint64 getHash() const;

	const char* getVersion() const { return m_version; }

	bool        hasStage(GLenum _stage) const;
	const char* getPath(GLenum _stage) const;

	int         getDefineCount(GLenum _stage) const;
	const char* getDefine(GLenum _stage, int _i) const;
	const char* getSource(GLenum _stage) const;

private:
	typedef apt::String<sizeof("9999 compatibility\0")> VersionStr;
	static VersionStr s_defaultVersion;
	VersionStr m_version;

	struct StageDesc
	{
		typedef apt::String<48> DefineStr;
		typedef apt::String<32> PathStr;
	
		PathStr                 m_path;         //< Source file path (if from a file).
		std::vector<DefineStr>  m_defines;      //< Name + value.       
		std::vector<char>       m_source;       //< Source (not including defines).

		bool isEnabled() const;

	}; // struct StageDesc

	StageDesc m_stages[internal::kShaderStageCount];

}; // class ShaderDesc

////////////////////////////////////////////////////////////////////////////////
/// \class Shader
////////////////////////////////////////////////////////////////////////////////
class Shader: public Resource<Shader>
{
public:

	static Shader* Create(const ShaderDesc& _desc);

	// Load/compile/link directly from a set of paths. _defines is a list of null-separated strings e.g. "DEFINE1 1\0DEFINE2 1\0"
	static Shader* CreateVsFs(const char* _vsPath, const char* _fsPath, const char* _defines = 0);
	static Shader* CreateCs(const char* _csPath, int _localX = 1, int _localY = 1, int _localZ = 1, const char* _defines = 0);

	static void    Destroy(Shader*& _inst_);

	bool load() { return reload(); }

	/// Attempt to reload/compile all stages and link the shader program.
	/// \return false if any stage fails to compile, or if link failed. If a
	///   previous attempt to load the shader was successful the shader remains
	///   valid even if reload() fails.
	bool reload();
	
	/// Retrieve the index of a program resource via glGetProgramResourceIndex.
	GLint getResourceIndex(GLenum _type, const char* _name) const;

	/// Retrieve teh location of a named uniform via glGetUniformLocation;
	GLint getUniformLocation(const char* _name) const;
	
	GLuint getHandle() const { return m_handle; }
	
	const ShaderDesc& getDesc() const { return m_desc; }

	/// Compute shader only.
	int getLocalSizeX() const { return m_localSize[0]; }
	int getLocalSizeY() const { return m_localSize[1]; }
	int getLocalSizeZ() const { return m_localSize[2]; }

private:
	GLuint     m_handle;

	ShaderDesc m_desc;
	GLuint     m_stageHandles[internal::kShaderStageCount];

	int m_localSize[3]; ///< Compute shader only.

	/// Get/free info log for a shader stage.
	static const char* GetStageInfoLog(GLuint _handle);
	static void FreeStageInfoLog(const char*& _log_);

	/// Get/free info log for a shader program.
	static const char* GetProgramInfoLog(GLuint _handle);
	static void FreeProgramInfoLog(const char*& _log_);

	Shader(uint64 _id, const char* _name);
	~Shader();	

	/// Load the specified stage, return status.
	bool loadStage(int _i);

	/// Set the name automatically based on desc.
	void setAutoName();

}; // class Shader

} // namespace frm

#endif // frm_Shader_h
