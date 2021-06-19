#include "SplinePath.h"

#include <frm/core/geom.h>
#include <frm/core/interpolation.h>
#include <frm/core/log.h>
#include <frm/core/memory.h>
#include <frm/core/AppSample3d.h>
#include <frm/core/Input.h>
#include <frm/core/FileSystem.h>
#include <frm/core/Json.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializer.h>

#include <imgui/imgui.h>
#include <im3d/im3d.h>

namespace frm {

// PUBLIC

SplinePath* SplinePath::CreateUnique()
{
	SplinePath* ret = FRM_NEW(SplinePath(GetUniqueId(), ""));
	ret->setNamef("SplinePath%llu", ret->getId());
	Use(ret);
	return ret;
}

SplinePath* SplinePath::Create(Serializer& _serializer_)
{
	SplinePath* ret = CreateUnique();
	ret->serialize(_serializer_);
	return ret;
}

SplinePath* SplinePath::Create(const char* _path)
{
	Id id = GetHashId(_path);
	SplinePath* ret = Find(id);
	if (!ret)
	{
		ret = FRM_NEW(SplinePath(id, _path));
		ret->m_path = _path;
	}
	Use(ret);

	return ret;
}

void SplinePath::Destroy(SplinePath*& _splinePath_)
{
	FRM_DELETE(_splinePath_);
}

bool SplinePath::Edit(SplinePath*& _splinePath_, bool* _open_)
{
	auto SelectSplinePath = [](PathStr& path_) -> bool
		{
			if (FileSystem::PlatformSelect(path_, { "*.spline" }))
			{
				FileSystem::SetExtension(path_, "spline");
				path_ = FileSystem::MakeRelative(path_.c_str());
				return true;
			}
			return false;
		};

	bool ret = false;

	if (!_splinePath_)
	{
		_splinePath_ = CreateUnique();
	}

	String<32> windowTitle = "Spline Path Editor";
	if (_splinePath_ && !_splinePath_->m_path.isEmpty())
	{
		windowTitle.appendf(" -- '%s'", _splinePath_->m_path.c_str());
	}
	windowTitle.append("###SplinePathEditor");

	if (_splinePath_ && ImGui::Begin(windowTitle.c_str(), _open_, ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New"))
				{			
					Release(_splinePath_);
					_splinePath_ = CreateUnique();
					ret = true;
				}

				if (ImGui::MenuItem("Open.."))
				{
					PathStr newPath;
					if (SelectSplinePath(newPath))
					{
						if (newPath != _splinePath_->m_path)
						{
							SplinePath* newSplinePath = Create(newPath.c_str());
							if (CheckResource(newSplinePath))
							{
								Release(_splinePath_);
								_splinePath_ = newSplinePath;
								ret = true;
							}
							else
							{
								Release(newSplinePath);
							}
						}
					}
				}
				
				if (ImGui::MenuItem("Save", nullptr, nullptr, !_splinePath_->m_path.isEmpty()))
				{
					if (!_splinePath_->m_path.isEmpty())
					{
						Json json;
						SerializerJson serializer(json, SerializerJson::Mode_Write);
						if (_splinePath_->serialize(serializer))
						{
							Json::Write(json, _splinePath_->m_path.c_str());
						}
					}
				}

				if (ImGui::MenuItem("Save As.."))
				{
					if (SelectSplinePath(_splinePath_->m_path))
					{
						Json json;
						SerializerJson serializer(json, SerializerJson::Mode_Write);
						if (_splinePath_->serialize(serializer))
						{
							Json::Write(json, _splinePath_->m_path.c_str());
						}
						ret = true;
					}
				}

				if (ImGui::MenuItem("Reload", nullptr, nullptr, !_splinePath_->m_path.isEmpty()))
				{
					_splinePath_->reload();
				}

				ImGui::EndMenu();
			}
			
			ImGui::EndMenuBar();
		}

		ret |= _splinePath_->edit();

		ImGui::End();
	}
	return ret;
}

vec3 SplinePath::samplePosition(float _t, int* _hint_)
{
	int seg = findSegment(_t, _hint_);

	if_unlikely (seg < 0)
	{
		return vec3(0.0f);
	}

	vec3 p0 = m_eval[seg].xyz();
	vec3 p1 = m_eval[seg + 1].xyz();
	float t = (_t - m_eval[seg].w) / (m_eval[seg + 1].w - m_eval[seg].w);
	return lerp(p0, p1, t);
}

void SplinePath::append(const vec3& _position)
{
	m_raw.push_back(_position);
	m_eval.clear();
}

bool SplinePath::reload()
{
	if (m_path.isEmpty())
	{
		// Not from a file, do nothing.
		return true;
	}

	Json json;
	if (!Json::Read(json, m_path.c_str()))
	{
		FRM_LOG_ERR("SplinePath: Failed to load '%s', file not found.", m_path.c_str());
		return false;
	}

	SerializerJson serializer(json, SerializerJson::Mode_Read);
	if (!serialize(serializer))
	{
		const char* err = serializer.getError();
		FRM_LOG_ERR("SplinePath: Error serializing '%s', '%s'.", m_path.c_str(), err ? err : "Unkown error.");
		return false;
	}

	build();

	return true;
}

bool SplinePath::edit()
{
	// \todo Reset editor state when selection changes?
	struct EditorState
	{
		int selected = -1;
	};
	static EditorState s_editorState;

	bool ret = false;

	ImGui::PushID(this);
	Im3d::PushEnableSorting();

	if (s_editorState.selected >= m_raw.size())
	{
		s_editorState.selected = -1;
	}
	
	bool isLoop = false;
	if (m_raw.size() > 1)
	{
		isLoop = m_raw.front() == m_raw.back();
	}

	if (m_raw.size() > 2 && ImGui::Checkbox("Loop", &isLoop))
	{
		if (isLoop)
		{
			m_raw.push_back(m_raw.front());
		}
		else
		{
			m_raw.pop_back();
		}

		ret = true;
	}

	if (ImGui::Button(ICON_FA_PLUS " Add"))
	{
		if (isLoop)
		{
			m_raw.pop_back();
		}
		
		int newIndex = s_editorState.selected + 1;
		vec3 newValue = vec3(0.0f);
		m_raw.insert(m_raw.begin() + newIndex, newValue);
		s_editorState.selected = newIndex;

		if (isLoop)
		{
			m_raw.push_back(m_raw.front());
		}

		ret = true;
	}

	if (s_editorState.selected >= 0)
	{
		if (Im3d::GizmoTranslation("SplinePath", &m_raw[s_editorState.selected].x))
		{
			ret = true;

			if (isLoop && s_editorState.selected == 0)
			{
				m_raw.back() = m_raw.front();
			}
		}

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_TIMES " Delete") || ImGui::IsKeyPressed(Keyboard::Key_Delete))
		{
			if (isLoop && s_editorState.selected == 0 && m_raw.size() > 2)
			{
				m_raw.back() = m_raw[1];
			}

			ret = true;
			m_raw.erase(m_raw.begin() + s_editorState.selected);
			s_editorState.selected = -1;
		}
	}

	ImGui::Spacing();
	ImGui::Text("%u points, length = %1.3f", m_raw.size(), m_length);

	const bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
	const Ray cursorRayW = ((AppSample3d*)AppSample3d::GetCurrent())->getCursorRayW();
	float nearestIntersection = FLT_MAX;
	int nearestIndex = -1;
	if (m_raw.size() > 0)
	{
		Im3d::PushDrawState();
			Im3d::SetSize(3.0f);
			for (int i = 0; i < (int)m_raw.size() - (isLoop ? 1 : 0); ++i)
			{
				if (i == s_editorState.selected)
				{
					continue;
				}
				
				const float radius = 0.25f; // \todo Constant screen space size.
				const Sphere sphere(m_raw[i], radius);

				float intersection = 0.0f;
				if (Intersect(cursorRayW, sphere, intersection, intersection))
				{
					Im3d::SetColor(Im3d::Color_Green);
					if (intersection < nearestIntersection)
					{
						nearestIndex = i;
						nearestIntersection = intersection;
					}
				}
				else
				{
					Im3d::SetColor(Im3d::Color_Yellow);
				}

				Im3d::DrawSphere(sphere.m_origin, sphere.m_radius);
			}
		Im3d::PopDrawState();

		if (mouseClicked && nearestIndex >= 0)
		{
			s_editorState.selected = nearestIndex;
		}
	}

	if_unlikely (ret || m_eval.empty())
	{
		build();
	}

	Im3d::PushDrawState();
		Im3d::SetColor(Im3d::Color_Magenta);
		Im3d::SetSize(6.0f);
		Im3d::SetAlpha(0.8f);
		draw();

		#if 1
			for (const vec4& p : m_eval)
			{
				Im3d::DrawPoint(p.xyz(), 8.0f, Im3d::Color_Black);
			}
		#else
			const int kSampleCount = 1024;
			for (int i = 0; i < kSampleCount; ++i)
			{
				float t = (float)i / (float)(kSampleCount - 1);
				Im3d::DrawPoint(samplePosition(t), 8.0f, Im3d::Color_Black);
			}
		#endif
	Im3d::PopDrawState();

	Im3d::PopEnableSorting();
	ImGui::PopID();

	return ret;
}

void SplinePath::draw() const
{
	if (m_eval.size() < 2)
	{
		return;
	}
	
	Im3d::BeginLineStrip();
		for (const vec4& p : m_eval)
		{
			Im3d::Vertex(p.xyz());
		}
	Im3d::End();
}

bool SplinePath::serialize(Serializer& _serializer_)
{
	uint rawSize = m_raw.size();
	if (_serializer_.beginArray(rawSize, "m_raw"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{
			m_raw.clear();
			m_raw.reserve(rawSize);
			vec3 p;
			while (_serializer_.value(p))
			{
				m_raw.push_back(p);
			}
		}
		else
		{
			for (vec3& p : m_raw)
			{
				_serializer_.value(p);
			}
		}
		_serializer_.endArray();
	}
	else
	{
		_serializer_.setError("Failed to serialize 'm_raw'");
	}

	return true;
}

// PRIVATE

SplinePath::SplinePath(uint64 _id, const char* _name)
	: Resource<SplinePath>(_id, _name)
{
}

SplinePath::~SplinePath()
{
}

void SplinePath::build()
{
	PROFILER_MARKER_CPU("SplinePath::build");

	if (m_raw.size() < 2)
	{
		return;
	}

	m_eval.clear();
	const bool isLoop = m_raw.front() == m_raw.back();
	const int m = isLoop ? 1 : 0; 
	const int n = (int)m_raw.size() - (isLoop ? 0 : 1); 
	for (int i = m; i < n; ++i)
	{
		subdiv(i);
	}

	m_length = 0.0f;
	m_eval[0].w = 0.0f;
	for (int i = 1, n = (int)m_eval.size(); i < n; ++i)
	{
		float seglen = length(m_eval[i].xyz() - m_eval[i - 1].xyz());
		m_length += seglen;
		m_eval[i].w = m_eval[i - 1].w + seglen;
	}

	for (int i = 1, n = (int)m_eval.size(); i < n; ++i)
	{
		m_eval[i].w /= m_length;
	}
}

void SplinePath::subdiv(int _segment, float _t0, float _t1, float _maxError, int _limit)
{
	const bool isLoop = m_raw.front() == m_raw.back();
	auto GetClampIndices = [this, isLoop](int _i, int& i0_, int& i1_, int& i2_, int& i3_)
		{
			if (isLoop && _i == m_raw.size() - 1)
			{
				_i = 0;
			}
			i0_ = _i - 1;
			i1_ = _i;
			i2_ = _i + 1;			
			i3_ = _i + 2;

			if (isLoop)
			{
				i0_ = (i0_ < 0) ? (int)m_raw.size() - 2 : i0_;
				i2_ = i2_ % ((int)m_raw.size() - 1);
				i3_ = i3_ % ((int)m_raw.size() - 1);
			}
			else
			{
				i0_ = Max(i0_, 0);
				i2_ = Min(i2_, (int)m_raw.size() - 1);
				i3_ = Min(i3_, (int)m_raw.size() - 1);
			}
		};


	int i0, i1, i2, i3;
	GetClampIndices(_segment, i0, i1, i2, i3);

	vec3 beg = cuberp(m_raw[i0], m_raw[i1], m_raw[i2], m_raw[i3], _t0);
	vec3 end = cuberp(m_raw[i0], m_raw[i1], m_raw[i2], m_raw[i3], _t1);
	if (_limit == 0)
	{
		m_eval.push_back(vec4(beg, 0.0f));
		m_eval.push_back(vec4(end, 0.0f));
		return;
	}
	--_limit;

	float tm = (_t0 + _t1) * 0.5f;
	vec3 mid = cuberp(m_raw[i0], m_raw[i1], m_raw[i2], m_raw[i3], tm);
	float a = length(mid - beg);
	float b = length(end - mid);
	float c = length(end - beg);
	if ((a + b) - c < _maxError)
	{
		m_eval.push_back(vec4(beg, 0.0f));
		m_eval.push_back(vec4(end, 0.0f));
		return;
	}

	subdiv(_segment, _t0, tm, _maxError, _limit);
	subdiv(_segment, tm, _t1, _maxError, _limit);
}

int SplinePath::findSegment(float _t, int* _hint_)
{
	// \todo This is broken if _hint_ is provided and t is descending.

	if_unlikely (m_raw.size() < 2)
	{
		return -1;
	}

	if_unlikely (m_eval.empty())
	{
		build();
	}

	int ret = -1;
	if (_hint_ == nullptr)
	{ 
		// No hint, use binary search.
		int lo = 0, hi = (int)m_eval.size() - 2;
		while (hi - lo > 1)
		{
			int mid = (hi + lo) / 2;		
			if (_t > m_eval[mid].w)
			{
				lo = mid;
			}
			else
			{
				hi = mid;
			}
		}

		ret = (_t > m_eval[hi].w) ? hi : lo;
	}
	else
	{
		// Hint, use linear search.
		*_hint_ = Clamp(*_hint_, 0, (int)m_eval.size() - 2);
		ret = *_hint_;
		while (_t > m_eval[ret + 1].w)
		{
			ret = (ret+ 1) % (int)m_eval.size();
		}

		*_hint_ = ret;
	}

	return ret;
}

} // namespace frm
