#include "XFormComponent.h"

#include <frm/core/interpolation.h>
#include <frm/core/Profiler.h>
#include <frm/core/Serializable.inl>
#include <frm/core/world/WorldEditor.h>

#include <im3d/im3d.h>
#include <imgui/imgui.h>

#include <EASTL/vector.h>

namespace frm {

/******************************************************************************

                                    XForm

******************************************************************************/

FRM_FACTORY_DEFINE(XForm);

static eastl::vector<XForm::CallbackReference*> s_callbacks;

// PUBLIC

XForm::CallbackReference::CallbackReference(const char* _name, Callback* _callback)
	: m_callback(_callback)
	, m_name(_name)
	, m_nameHash(_name)
{
	FRM_ASSERT(FindCallback(m_nameHash) == nullptr);
	if (FindCallback(m_nameHash) != nullptr)
	{
		FRM_LOG_ERR("XForm: Callback '%s' already exists", _name);
		return;
	}
	s_callbacks.push_back(this);
}


int XForm::GetCallbackCount()
{
	return (int)s_callbacks.size();
}

const XForm::CallbackReference* XForm::GetCallback(int _i)
{
	return s_callbacks[_i];
}

const XForm::CallbackReference* XForm::FindCallback(StringHash _nameHash)
{
	for (auto& ret : s_callbacks)
	{
		if (ret->m_nameHash == _nameHash)
		{
			return ret;
		}
	}
	return nullptr;
}

const XForm::CallbackReference* XForm::FindCallback(Callback* _callback)
{
	for (auto& ret : s_callbacks)
	{
		if (ret->m_callback == _callback)
		{
			return ret;
		}
	}
	return nullptr;
}

bool XForm::EditCallback(const CallbackReference*& _callback_, const char* _name)
{
	bool ret = false;
	ImGui::PushID(_callback_);

	if (ImGui::Button(_name))
	{
		ImGui::OpenPopup("Select Callback");
	}

	if (ImGui::BeginPopup("Select Callback"))
	{
		if (ImGui::Selectable("NONE"))
		{
			_callback_ = nullptr;
		}

		for (const CallbackReference* ref : s_callbacks)
		{
			if (ref != _callback_)
			{
				if (ImGui::Selectable(ref->m_name))
				{
					_callback_ = ref;
					ret = true;
					break;
				}
			}
		}

		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::Text(_callback_ ? _callback_->m_name : "NONE");

	ImGui::PopID();
	return ret;
}

bool XForm::SerializeCallback(Serializer& _serializer_, const CallbackReference*& _callback_, const char* _name)
{
	if (_serializer_.getMode() == Serializer::Mode_Read)
	{
		String<64> callbackName;
		if (!Serialize(_serializer_, callbackName, _name))
		{
			return false;
		}

		if (callbackName.isEmpty())
		{
			_callback_ = nullptr;
			return true;
		}

		const CallbackReference* ref = FindCallback(StringHash((const char*)callbackName));
		if (!ref)
		{
			FRM_LOG_ERR("XForm: Invalid callback '%s'", callbackName.c_str());
			_callback_ = nullptr;
			return false;
		}
		_callback_ = ref;
	}
	else
	{
		String<64> callbackName = _callback_ ? _callback_->m_name : "";
		return Serialize(_serializer_, callbackName, _name);
	}
	return true;
}

static void XFormCallback_Reset(XForm* _xform_, SceneNode* _node_) { _xform_->reset(); }
FRM_XFORM_REGISTER_CALLBACK("Reset", &XFormCallback_Reset);

static void XFormCallback_RelativeReset(XForm* _xform_, SceneNode* _node_) { _xform_->relativeReset(); }
FRM_XFORM_REGISTER_CALLBACK("Relative Reset", &XFormCallback_RelativeReset);

static void XFormCallback_Reverse(XForm* _xform_, SceneNode* _node_) { _xform_->reverse(); }
FRM_XFORM_REGISTER_CALLBACK("Reverse", &XFormCallback_Reverse);

// PRIVATE

/******************************************************************************

                                XFormComponent

******************************************************************************/

FRM_COMPONENT_DEFINE(XFormComponent, 0);

// PUBLIC

void XFormComponent::Update(Component** _from, Component** _to, float _dt, World::UpdatePhase _phase)
{
	PROFILER_MARKER_CPU("XFormComponent::Update");

	if (_phase != World::UpdatePhase::PrePhysics)
	{
		return;
	}

	for (; _from != _to; ++_from)
	{
		XFormComponent* component = (XFormComponent*)*_from;
		SceneNode* node = component->getParentNode();
		for (XForm* xform : component->m_xforms)
		{
			xform->apply(_dt, node);
		}
	}
}

void XFormComponent::addXForm(XForm* _xform_)
{
	m_xforms.push_back(_xform_);
}

void XFormComponent::removeXForm(XForm*& _xform_)
{
	auto it = eastl::find(m_xforms.begin(), m_xforms.end(), _xform_);
	if (it != m_xforms.end())
	{
		m_xforms.erase(it);
		XForm::Destroy(_xform_);
	}
}

XFormComponent::~XFormComponent()
{
	while (!m_xforms.empty())
	{
		XForm::Destroy(m_xforms.back());
		m_xforms.pop_back();
	}
}

// PRIVATE

bool XFormComponent::editImpl()
{
	bool ret = false;

	// \todo \hack Local editor state, need a more comprehensive editor system.
	static XFormComponent* s_currentXFormComponent = nullptr;
	static XForm*          s_currentXForm          = nullptr;

	if (s_currentXFormComponent != this)
	{
		s_currentXFormComponent = this;
		s_currentXForm = nullptr;
	}
	
	if (ImGui::Button(ICON_FA_PLUS " Create"))
	{
		ImGui::OpenPopup("CreateXForm");
	}
	if (ImGui::BeginPopup("CreateXForm"))
	{
		static ImGuiTextFilter filter;
		filter.Draw("Filter##CreateXForm");	
		for (int i = 0; i < XForm::GetClassRefCount(); ++i)
		{
			const XForm::ClassRef* cref = XForm::GetClassRef(i);
			if (filter.PassFilter(cref->getName()))
			{
				if (ImGui::Selectable(cref->getName()))
				{
					s_currentXForm = XForm::Create(cref);
					addXForm(s_currentXForm);
					ret = true;
					ImGui::CloseCurrentPopup();
				}
			}
		}
		
		ImGui::EndPopup();
	}

	if (s_currentXForm)
	{
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_TIMES " Destroy"))
		{
			removeXForm(s_currentXForm);
		}
	}

	if (!m_xforms.empty())
	{

		if (ImGui::ListBoxHeader("##XForms", ImVec2(-1, 150)))
		{
			int moveFrom = -1, moveTo = -1;
			for (int i = 0; i < (int)m_xforms.size(); ++i)
			{
				XForm* xform = m_xforms[i];
				ImGui::PushID(xform);

				const char* xformName = xform->getClassRef()->getName();
				if (ImGui::Selectable(xformName, xform == s_currentXForm))
				{
					s_currentXForm = xform;
				}
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_SourceNoHoldToOpenOthers))
				{
					ImGui::Text(xformName);
					ImGui::SetDragDropPayload("XForm", &i, sizeof(i));
					ImGui::EndDragDropSource();
				}
				if (ImGui::BeginDragDropTarget())
				{
					const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("XForm", ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
					if (payload)
					{
						moveFrom = *(int*)payload->Data;
						moveTo = i;
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::PopID();
			}

			if (moveFrom != moveTo && moveFrom != -1 && moveTo != -1)
			{
				moveXForm(moveFrom, moveTo);
				ImGui::SetDragDropPayload("XForm", &moveTo, sizeof(int)); // Update payload immediately so on the next frame if we move the mouse to an earlier item our index payload will be correct. This is odd and showcase how the DnD api isn't best presented in this example.
				ret = true;
			}

			ImGui::ListBoxFooter();
		}

		ImGui::Spacing();
		if (s_currentXForm)
		{
			ret |= s_currentXForm->edit();
		}
	}

	return ret;
}

bool XFormComponent::serializeImpl(Serializer& _serializer_)
{
	SerializeAndValidateClass(_serializer_);
	uint xformCount = m_xforms.size();
	if (_serializer_.beginArray(xformCount, "XForms"))
	{
		if (_serializer_.getMode() == Serializer::Mode_Read)
		{			
			while (!m_xforms.empty())
			{
				XForm::Destroy(m_xforms.back());
				m_xforms.pop_back();
			}

			for (uint i = 0; i < xformCount; ++i)
			{
				if (_serializer_.beginObject())
				{
					String<32> className;
					if (!Serialize(_serializer_, className, "_class"))
					{
						_serializer_.setError("XForm: Error serializing _class.");
					}
					else
					{
						XForm* xform = XForm::Create(StringHash(className.c_str()));
						if (xform && xform->serialize(_serializer_))
						{
							m_xforms.push_back(xform);
						}						
					}

					_serializer_.endObject();
				}
			}
		}
		else
		{
			for (XForm* xform : m_xforms)
			{
				if (_serializer_.beginObject())
				{
					xform->serialize(_serializer_);
					_serializer_.endObject();
				}
			}
		}

		_serializer_.endArray();
	}

	return _serializer_.getError() == nullptr;
}

void XFormComponent::moveXForm(int _from, int _to)
{
	int copyDst = (_from < _to) ? _from : _to + 1;
	int copySrc = (_from < _to) ? _from + 1 : _to;
	int copyCount = (_from < _to) ? _to - _from : _from - _to;
	XForm* tmp = m_xforms[_from];
	memmove(&m_xforms[copyDst], &m_xforms[copySrc], (size_t)copyCount * sizeof(XForm*));
	m_xforms[_to] = tmp;
}

/******************************************************************************

                                XFormSpin

******************************************************************************/

FRM_XFORM_DECLARE(XFormSpin)
{
public:

	void reset() override
	{
		m_rotation = 0.0f;
	}

	void reverse() override
	{
		m_rate = -m_rate;
	}
	
	void apply(float _dt, SceneNode* _node_) override
	{
		m_rotation += m_rate * _dt;
		m_rotation = Fract(m_rotation / kTwoPi) * kTwoPi;

		_node_->setLocal(_node_->getLocal() * RotationMatrix(m_axis, m_rotation));
	}

	bool edit() override
	{
		bool ret = false;

		float turnsPerSecond = m_rate / kTwoPi;
		if (ImGui::SliderFloat("Rate (turns/second)", &turnsPerSecond, -12.0f, 12.0f))
		{
			m_rate = turnsPerSecond * kTwoPi;
			ret = true;
		}

		if (ImGui::SliderFloat3("Axis", &m_axis.x, -1.0f, 1.0f))
		{
			m_axis = normalize(m_axis);
			ret = true;
		}

		ImGui::Spacing();
		ImGui::Text("m_rotation = %1.4f", m_rotation);

		return ret;
	}

	bool serialize(Serializer& _serializer_) override
	{
		bool ret = SerializeAndValidateClass(_serializer_);
		ret |= Serialize(_serializer_, m_rate, "m_rate");
		ret |= Serialize(_serializer_, m_axis, "m_axis");
		return ret;
	}

private:

	vec3  m_axis       = vec3(0.0f, 0.0f, 1.0f);
	float m_rate       = 0.0f; // radians/s
	float m_rotation   = 0.0f;
};

FRM_XFORM_DEFINE(XFormSpin, 0);

/******************************************************************************

                              XFormPositionTarget

******************************************************************************/

FRM_XFORM_DECLARE(XFormPositionTarget)
{
public:

	void reset() override
	{
		m_time = 0.0f;
	}

	void relativeReset() override
	{
		m_end   = m_position + (m_end - m_start);
		m_start = m_position;
		m_time  = 0.0f;
	}

	void reverse() override
	{
		eastl::swap(m_start, m_end);
		m_time = FRM_MAX(m_duration - m_time, 0.0f);
	}
	
	void apply(float _dt, SceneNode* _node_) override
	{
		m_time = FRM_MIN(m_time + _dt, m_duration);
		if (m_onComplete && m_time >= m_duration)
		{
			m_onComplete->m_callback(this, _node_);
		}
		m_position = smooth(m_start, m_end, m_time / m_duration);

		mat4 local = _node_->getLocal();
		SetTranslation(local, m_position);
		_node_->setLocal(local);
	}

	bool edit() override
	{
		bool ret = false;

		ret |= ImGui::SliderFloat("Duration (s)", &m_duration, 0.0f, 10.0f);
	
		ret |= Im3d::GizmoTranslation("XFormPositionTarget::m_start", &m_start.x);
		ret |= Im3d::GizmoTranslation("XFormPositionTarget::m_end",   &m_end.x);
		Im3d::PushDrawState();
			Im3d::SetColor(Im3d::Color_Yellow);
			Im3d::SetSize(4.0f);
			Im3d::BeginLines();
				Im3d::SetAlpha(0.2f);
				Im3d::Vertex(m_start);
				Im3d::SetAlpha(1.0f);
				Im3d::Vertex(m_end);
			Im3d::End();
		Im3d::PopDrawState();

		ret |= EditCallback(m_onComplete, "On Complete");

		return ret;
	}

	bool serialize(Serializer& _serializer_) override
	{
		bool ret = SerializeAndValidateClass(_serializer_);
		ret |= Serialize(_serializer_, m_start,    "m_start");
		ret |= Serialize(_serializer_, m_end,      "m_end");
		ret |= Serialize(_serializer_, m_duration, "m_duration");
		ret |= SerializeCallback(_serializer_, m_onComplete, "m_onComplete");
		return ret;
	}

private:

	vec3                     m_start      = vec3(0.0f);
	vec3                     m_end        = vec3(0.0f);
	float                    m_duration   = 1.0f;
	vec3                     m_position   = vec3(0.0f);
	float                    m_time       = 0.0f;
	const CallbackReference* m_onComplete = nullptr;
};

FRM_XFORM_DEFINE(XFormPositionTarget, 0);

} // namespace frm