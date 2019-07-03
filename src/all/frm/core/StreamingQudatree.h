#pragma once

#include <frm/core/def.h>
#include <frm/core/math.h>

#include <apt/Pool.h>
#include <apt/Quadtree.h>

#include <EASTL/vector.h>

namespace frm {

////////////////////////////////////////////////////////////////////////////////
// StreamingQuadtree
// Manages quadtree subdivision, with load/release requests.
//
// Node subdivision is controlled by periodically setting the 'pivot' (subdivision 
// center) and servicing data requests. Each level in the quadtree has a
// corresponding LOD sphere centered on the pivot; a node is subdivided if it 
// intersects the corresponding sphere. LOD sphere radii are controlled by a 
// single scale value (see setLodScale()).
//
// Nodes are identified by their linear quadtree index. Node origin is at node
// center in XY, the node base in Z.
//
// Quadtree space (suffix Q) is in [-1,1] (XY), [0,1] (Z) with 0 being the origin 
// of the root node (like NDC).
//
// \todo
// - Don't store Node*/void* in the quadtrees, use a special pool which can be
//   accessed by a 16 bit index (and just store 16 bits).
////////////////////////////////////////////////////////////////////////////////
class StreamingQuadtree
{
public:

	typedef uint16 NodeIndex;
	static constexpr NodeIndex NodeIndex_Invalid = ~NodeIndex(0);

	enum NodeState_
	{
		NodeState_Invalid,
		NodeState_QueuedForLoad,
		NodeState_Loaded,
		NodeState_QueuedForRelease,
	
		NodeState_Count
	};
	typedef uint8 NodeState;

	struct Node
	{
		NodeIndex m_index   = 0;
		int       m_level   = 0;
		vec3      m_originQ = vec3(0.0f); // XY in [-1,1], Z in [0,1].
		float     m_widthQ  = 2.0f;       // XY size.
		float     m_heightQ = 1.0f;       // Z size.
	};

	StreamingQuadtree(int _levelCount, int _nodePoolSize = 512);
	~StreamingQuadtree();

	void      update();
	void      drawDebug(const mat4& _world);

	void      setPivot(const vec3& _pivotQ, const vec3& _directionQ = vec3(0.0f, 0.0f, 1.0f));
	void      setLodScale(float _lodScale);

	void*     getNodeData(NodeIndex _nodeIndex) const { return m_dataQuadtree[_nodeIndex]; }
	void      setNodeData(NodeIndex _nodeIndex, void* _data);

	NodeIndex popLoadQueue();
	size_t    getLoadQueueCount() const    { return m_loadQueue.size(); }
	NodeIndex popReleaseQueue();
	size_t    getReleaseQueueCount() const { return m_releaseQueue.size(); }	

protected:
	
	typedef apt::Quadtree<NodeIndex, Node*>     Quadtree_Node;
	typedef apt::Quadtree<NodeIndex, NodeState> Quadtree_State;
	typedef apt::Quadtree<NodeIndex, void*>     Quadtree_Data;

	apt::Pool<Node>          m_nodePool;
	Quadtree_Node            m_nodeQuadtree;
	Quadtree_State           m_stateQuadtree;
	Quadtree_Data            m_dataQuadtree;
	int                      m_maxLevel        = 0;
	vec3                     m_pivotQ          = vec3(0.0f);
	vec3                     m_directionQ      = vec3(0.0f, 0.0f, 1.0f);
	float                    m_lodScale        = 1.0f;
	eastl::vector<float>     m_lodRadii2;
	bool                     m_updateDrawList  = true;
	eastl::vector<NodeIndex> m_drawList;
	eastl::vector<NodeIndex> m_loadQueue;
	eastl::vector<NodeIndex> m_releaseQueue;

	bool isLeaf(const Node* _node) const;
	bool wantSplit(const Node* _node) const;
	void split(const Node* _node);
	void merge(const Node* _node);
	void queueForLoad(NodeIndex _nodeIndex);
	void queueForRelease(NodeIndex _nodeIndex);
	void releaseNode(NodeIndex _nodeIndex);
	void sortLoadQueue();

}; // class StreamingQuadtree

} // namespace frm

