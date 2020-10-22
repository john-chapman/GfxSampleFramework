#include "StreamingQuadtree.h"

#include <frm/core/Profiler.h>

#include <im3d/im3d.h>
#include <imgui/imgui.h>

#include <EASTL/sort.h>

namespace frm {

// PUBLIC

StreamingQuadtree::StreamingQuadtree(int _levelCount, int _nodePoolSize)
	: m_nodePool(_nodePoolSize)
	, m_nodeQuadtree(_levelCount, nullptr)
	, m_stateQuadtree(_levelCount, NodeState_Invalid)
	, m_dataQuadtree(_levelCount, nullptr)
	, m_maxLevel(_levelCount - 1)
{
	m_lodRadii2.resize(_levelCount);
	setLodScale(m_lodScale);

	m_nodeQuadtree[0] = m_nodePool.alloc();
	queueForLoad(0);
}

StreamingQuadtree::~StreamingQuadtree()
{
}

void StreamingQuadtree::update()
{
	PROFILER_MARKER_CPU("StreamingQuadtree::update");

	if (m_updateDrawList)
	{
		m_drawList.clear();
		m_nodeQuadtree.traverse(
			[&](NodeIndex _nodeIndex, int _nodeLevel)
			{
				PROFILER_MARKER_CPU("Visit Node");

				#if 0
				{
				 // \todo it's probaly possible to update the draw list during the split/merge traversal but the node selection code below need
				 // to be merged with the split/merge logic
					if (m_stateQuadtree[_nodeIndex] == NodeState_Loaded)
					{
						NodeIndex firstChildIndex = m_nodeQuadtree.getFirstChildIndex(_nodeIndex, _nodeLevel);
						if (firstChildIndex == NodeIndex_Invalid)
						{
							m_drawList.push_back(_nodeIndex);
						}
						else
						{
							for (NodeIndex childIndex = firstChildIndex; childIndex < (firstChildIndex + 4); ++childIndex)
							{
								if (m_stateQuadtree[childIndex] != NodeState_Loaded)
								{
									m_drawList.push_back(_nodeIndex);
									break;
								}
							}
						}
					}
				}
				#endif

				Node* node = m_nodeQuadtree[_nodeIndex];
				if (wantSplit(node))
				{
					split(node);
					return true;
				}
				else
				{
					merge(node);
					return false;
				}
			},
			0
			);

		#if 1
		{
		 // \todo see the comment in the first traversal above
			m_nodeQuadtree.traverse(
				[&](NodeIndex _nodeIndex, int _nodeLevel)
				{
					if (m_stateQuadtree[_nodeIndex] != NodeState_Loaded)
					{
						return false;
					}
					NodeIndex firstChildIndex = m_nodeQuadtree.getFirstChildIndex(_nodeIndex, _nodeLevel);
					if (firstChildIndex == NodeIndex_Invalid)
					{
						m_drawList.push_back(_nodeIndex);
						return false;
					}
					for (NodeIndex childIndex = firstChildIndex; childIndex < (firstChildIndex + 4); ++childIndex)
					{
						if (m_stateQuadtree[childIndex] != NodeState_Loaded)
						{
							m_drawList.push_back(_nodeIndex);
							return false;
						}
					}
					return true;
				},
				0
				);		
		}
		#endif

		m_updateDrawList = false;
	}
	sortLoadQueue();
}

void StreamingQuadtree::drawDebug(const mat4& _world)
{
	/*ImGui::Text("Total node count: %u", m_nodePool.getUsedCount());
	ImGui::Text("Load queue:       %u", m_loadQueue.size());
	ImGui::Text("Release queue:    %u", m_releaseQueue.size());
	ImGui::Text("Draw list:        %u", m_drawList.size());
	if (ImGui::SliderFloat("LOD Scale", &m_lodScale, 1e-4f, 2.0f))
	{
		setLodScale(m_lodScale);
	}*/

	Im3d::PushDrawState();
	Im3d::PushMatrix(_world);

	Im3d::SetSize(1.0f);
	Im3d::SetColor(Im3d::Color(1.0f, 1.0f, 1.0f, 0.5f));
	Im3d::DrawAlignedBox(vec3(-1.0f, -1.0f, 0.0f), vec3(1.0f, 1.0f, 1.0f));

	Im3d::SetSize(2.0f);

	m_nodeQuadtree.traverse(
		[&](NodeIndex _nodeIndex, int _nodeLevel)
		{
			const Im3d::Color kLevelColors[] =
			{
				Im3d::Color_Red,
				Im3d::Color_Green,
				Im3d::Color_Blue,
				Im3d::Color_Magenta,
				Im3d::Color_Yellow,
				Im3d::Color_Cyan
			};

			Node* node = m_nodeQuadtree[_nodeIndex];
			if (!node)
			{
				return false;
			}
			if (!isLeaf(node))
			{
				return true;
			}
			vec3 boxSize = vec3(node->m_widthQ / 2.0f, node->m_widthQ / 2.0f, node->m_heightQ);
			vec3 boxMin  = node->m_originQ + vec3(-boxSize.xy(), 0.0f);
			vec3 boxMax  = node->m_originQ + vec3( boxSize.xy(), boxSize.z);
			Im3d::SetColor(kLevelColors[_nodeLevel % FRM_ARRAY_COUNT(kLevelColors)]);
			Im3d::SetAlpha(1.0f);
			Im3d::DrawAlignedBox(boxMin, boxMax);
	
			NodeState state = m_stateQuadtree[_nodeIndex];
			if (m_stateQuadtree[_nodeIndex] != NodeState_Loaded)
			{
				Im3d::PushEnableSorting();
				Im3d::SetColor(state == NodeState_QueuedForLoad ? Im3d::Color_Yellow : Im3d::Color_Cyan);
				Im3d::SetAlpha(0.25f);
				Im3d::DrawAlignedBoxFilled(boxMin, boxMax);				
				Im3d::PopEnableSorting();
			}
			return false;
		},
		0
		);

	Im3d::SetColor(Im3d::Color_White);
	Im3d::SetAlpha(0.1f);
	Im3d::PushEnableSorting();
	for (NodeIndex nodeIndex : m_drawList)
	{
		Node* node = m_nodeQuadtree[nodeIndex];
		if (!node)
		{
			continue;
		}
		vec3 boxSize = vec3(node->m_widthQ / 2.0f, node->m_widthQ / 2.0f, node->m_heightQ);
		vec3 boxMin  = node->m_originQ + vec3(-boxSize.xy(), 0.0f);
		vec3 boxMax  = node->m_originQ + vec3( boxSize.xy(), boxSize.z);
		Im3d::DrawAlignedBoxFilled(boxMin, boxMax);	

		int nodeLevel = Quadtree<NodeIndex, Node*>::FindLevel(nodeIndex);
		uvec2 cartesian = Quadtree<NodeIndex, Node*>::ToCartesian(nodeIndex, nodeLevel);
		Im3d::Text((boxMin + boxMax) * 0.5f, 1.0f, Im3d::Color_White, Im3d::TextFlags_AlignLeft, "[%d] %u, %u\n%.2f, %.2f", nodeLevel, cartesian.x, cartesian.y, node->m_originQ.x * 0.5f + 0.5f, node->m_originQ.y * 0.5f + 0.5f);
	}
	Im3d::PopEnableSorting();

	Im3d::PopMatrix();
	Im3d::PopDrawState();
}

void StreamingQuadtree::setPivot(const vec3& _pivotQ, const vec3& _directionQ)
{
	m_directionQ = _directionQ;
	if (length2(_pivotQ - m_pivotQ) < FLT_EPSILON) // \todo use leaf node half width instead of FLT_EPSILON
	{
		return;
	}
	m_pivotQ = _pivotQ;
	m_updateDrawList = true;
}

void StreamingQuadtree::setLodScale(float _lodScale)
{
	m_lodScale = _lodScale;
	m_lodRadii2.back() = 2.0f / (float)(1 << m_maxLevel); // leaf node width
	for (int i = (int)m_lodRadii2.size() - 2; i != -1; --i)
	{
		m_lodRadii2[i] = m_lodRadii2[i + 1] * (1.0f + _lodScale);
	}
	for (float& radius : m_lodRadii2)
	{
		radius = radius * radius;
	}
	m_updateDrawList = true;
}

void StreamingQuadtree::setNodeData(NodeIndex _nodeIndex, void* _data)
{
	FRM_ASSERT(m_nodeQuadtree[_nodeIndex]);
	m_dataQuadtree[_nodeIndex] = _data;
	if (_data)
	{
		m_updateDrawList = true; // node may be split in the next update
		m_stateQuadtree[_nodeIndex] = NodeState_Loaded;
	}
	else
	{
		releaseNode(_nodeIndex);
	}
}

StreamingQuadtree::NodeIndex StreamingQuadtree::popLoadQueue()
{
	if (m_loadQueue.empty())
	{
		return NodeIndex_Invalid;
	}
	NodeIndex ret = m_loadQueue.back();
	m_loadQueue.pop_back();
	return ret;
}

StreamingQuadtree::NodeIndex StreamingQuadtree::popReleaseQueue()
{
	if (m_releaseQueue.empty())
	{
		return NodeIndex_Invalid;
	}
	NodeIndex ret = m_releaseQueue.back();
	m_releaseQueue.pop_back();
	return ret;
}

void StreamingQuadtree::releaseAll()
{
	merge(m_nodeQuadtree[0]);
	queueForRelease(0);
}

// PROTECTED

bool StreamingQuadtree::isLeaf(const Node* _node) const
{
	FRM_ASSERT(_node);
	NodeIndex firstChildIndex = m_nodeQuadtree.getFirstChildIndex(_node->m_index, _node->m_level);
	return firstChildIndex == NodeIndex_Invalid || m_stateQuadtree[firstChildIndex] == NodeState_Invalid;
}

bool StreamingQuadtree::wantSplit(const Node* _node) const
{
	FRM_ASSERT(_node);
	
 // split only if not a leaf node and if loaded
	if (_node->m_level == m_maxLevel || m_stateQuadtree[_node->m_index] != NodeState_Loaded)
	{
		return false;
	}

	float halfWidthQ = _node->m_widthQ / 2.0f;
	vec3 boxMin = vec3(_node->m_originQ.x - halfWidthQ, _node->m_originQ.y - halfWidthQ, 0.0f);
	vec3 boxMax = vec3(_node->m_originQ.x + halfWidthQ, _node->m_originQ.y + halfWidthQ, _node->m_heightQ);
	
 // sphere-AABB intersection
	float d2 = 0.0f;
	for (int i = 0; i < 3; ++i)
	{
		float v = m_pivotQ[i];
		float d = 0.0f;
		if (v < boxMin[i]) 
		{
			d = boxMin[i] - v;
		}
		if (v > boxMax[i]) 
		{
			d = v - boxMax[i];
		}
		d2 += d * d;
	}
	return d2 < m_lodRadii2[_node->m_level];
}

void StreamingQuadtree::split(const Node* _node)
{
	PROFILER_MARKER_CPU("StreamingQuadtree::split");

	FRM_ASSERT(_node);
	FRM_ASSERT(_node->m_level != m_maxLevel);

	NodeIndex firstChildIndex = m_nodeQuadtree.getFirstChildIndex(_node->m_index, _node->m_level);
 
 // can't make any assumptions about the state of child nodes since the release queue can be processed arbitrarily
	//if (m_nodeQuadtree[firstChildIndex])
	//{
	//	queueForLoad(firstChildIndex + 0);
	//	queueForLoad(firstChildIndex + 1);
	//	queueForLoad(firstChildIndex + 2);
	//	queueForLoad(firstChildIndex + 3);
	//	return;
	//}
	
	for (NodeIndex childIndex = firstChildIndex; childIndex < (firstChildIndex + 4); ++childIndex)
	{
		Node*& child = m_nodeQuadtree[childIndex];
		if (!child)
		{
			child           = m_nodePool.alloc();
			child->m_index  = childIndex;
			child->m_level  = _node->m_level + 1;
			child->m_widthQ = _node->m_widthQ / 2.0f;
		}
		queueForLoad(childIndex);
	}
	float childOffset = _node->m_widthQ / 4.0f;
	m_nodeQuadtree[firstChildIndex + 0]->m_originQ = _node->m_originQ + vec3(-childOffset, -childOffset, _node->m_originQ.z);
	m_nodeQuadtree[firstChildIndex + 1]->m_originQ = _node->m_originQ + vec3(-childOffset,  childOffset, _node->m_originQ.z);
	m_nodeQuadtree[firstChildIndex + 2]->m_originQ = _node->m_originQ + vec3( childOffset, -childOffset, _node->m_originQ.z);
	m_nodeQuadtree[firstChildIndex + 3]->m_originQ = _node->m_originQ + vec3( childOffset,  childOffset, _node->m_originQ.z);
}

void StreamingQuadtree::merge(const Node* _node)
{
	if (!_node || isLeaf(_node))
	{
		return;
	}

	PROFILER_MARKER_CPU("StreamingQuadtree::merge");

	NodeIndex firstChildIndex = m_nodeQuadtree.getFirstChildIndex(_node->m_index, _node->m_level);
	for (NodeIndex childIndex = firstChildIndex; childIndex < (firstChildIndex + 4); ++childIndex)
	{
		Node*& child = m_nodeQuadtree[childIndex];
		if (!child)
		{
			continue;
		}
		FRM_ASSERT(child->m_index == childIndex);
		merge(child);
		queueForRelease(childIndex);
	}
}

void StreamingQuadtree::queueForLoad(NodeIndex _nodeIndex)
{
	PROFILER_MARKER_CPU("StreamingQuadtree::queueForLoad");

	FRM_ASSERT(m_nodeQuadtree[_nodeIndex]);
	NodeState& state = m_stateQuadtree[_nodeIndex];
	if (state == NodeState_QueuedForLoad || state == NodeState_Loaded)
	{
		return;
	}

 // cancel pending release (in which case node can return to a loaded state and we don't need to queue it)
	if (state == NodeState_QueuedForRelease)
	{
		auto it = eastl::find(m_releaseQueue.begin(), m_releaseQueue.end(), _nodeIndex);
		FRM_ASSERT(it != m_releaseQueue.end());
		m_releaseQueue.erase_unsorted(it);
		FRM_ASSERT(m_dataQuadtree[_nodeIndex] != nullptr); // if queued for release the data should still be present
		state = NodeState_Loaded;
	}
 // else push into load queue
	else
	{
		FRM_ASSERT(eastl::find(m_loadQueue.begin(), m_loadQueue.end(), _nodeIndex) == m_loadQueue.end()); // shouldn't already be in the queue
		m_loadQueue.push_back(_nodeIndex);
		state = NodeState_QueuedForLoad;
	}
}

void StreamingQuadtree::queueForRelease(NodeIndex _nodeIndex)
{
	PROFILER_MARKER_CPU("StreamingQuadtree::queueForRelease");

	FRM_ASSERT(m_nodeQuadtree[_nodeIndex]);
	NodeState& state = m_stateQuadtree[_nodeIndex];
	if (state == NodeState_QueuedForRelease|| state == NodeState_Invalid)
	{
		return;
	}

 // cancel pending load (in which case the node can be freed and we don't need to queue it)
	if (state == NodeState_QueuedForLoad)
	{
		auto it = eastl::find(m_loadQueue.begin(), m_loadQueue.end(), _nodeIndex);
		FRM_ASSERT(it != m_loadQueue.end());
		m_loadQueue.erase_unsorted(it);
		FRM_ASSERT(m_dataQuadtree[_nodeIndex] == nullptr); // if queued for load the data should not be present
		releaseNode(_nodeIndex);
	}
 // else push into release queue
	else
	{
		FRM_ASSERT(eastl::find(m_releaseQueue.begin(), m_releaseQueue.end(), _nodeIndex) == m_releaseQueue.end()); // shouldn't already be in the queue
		m_releaseQueue.push_back(_nodeIndex);
		state = NodeState_QueuedForRelease;
	}
}

void StreamingQuadtree::releaseNode(NodeIndex _nodeIndex)
{
	PROFILER_MARKER_CPU("StreamingQuadtree::releaseNode");

	FRM_ASSERT(m_dataQuadtree[_nodeIndex] == nullptr);
	FRM_ASSERT(m_nodeQuadtree[_nodeIndex]);
	m_nodePool.free(m_nodeQuadtree[_nodeIndex]);
	m_nodeQuadtree[_nodeIndex] = nullptr;
	m_stateQuadtree[_nodeIndex] = NodeState_Invalid;
}

void StreamingQuadtree::sortLoadQueue()
{
	PROFILER_MARKER_CPU("StreamingQuadtree::sortLoadQueue");

	if (m_loadQueue.size() > 2) 
	{
		eastl::sort(m_loadQueue.begin(), m_loadQueue.end(), 
			[this](NodeIndex _a, NodeIndex _b)
			{
				vec3 da = m_pivotQ - m_nodeQuadtree[_a]->m_originQ;
				vec3 db = m_pivotQ - m_nodeQuadtree[_b]->m_originQ;
				return Dot(m_directionQ, db) < Dot(m_directionQ, da);
			}
			);
	}
}

} // namespace frm