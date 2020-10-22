#pragma once

#include <frm/core/frm.h>
#include <frm/core/memory.h>
#include <frm/core/types.h>
#include <frm/core/math.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/vector.h>

namespace frm {

///////////////////////////////////////////////////////////////////////////////
// Quadtree
// Generic linear quadtree.
//
// tIndex is the type used for indexing nodes and determines the absolute max
// level of subdivision possible. This should be a uint* type (uint8, uint16, 
// uint32, uint64).
//
// tNode is the node type. Typically this will be a pointer or index into a
// separate node data pool. Use the _init arg of the ctor to init the quadtree
// with 'invalid' nodes.
//
// Internally each level is stored sequentially with the root level at index 0.
// Within each level, nodes are laid out in Morton order:
//  +---+---+
//  | 0 | 2 |
//  +---+---+
//  | 1 | 3 |
//  +---+---+
// Use linearize()/delinearize() functions to convert to/from a linear layout
// e.g. for conversion to a texture.
//
// \todo (also applies to Octree.h)
// - Split out the bit interleaving code into a Morton helper (fast path for index sizes that case use magic bits https://graphics.stanford.edu/~seander/bithacks.html).
// - Implement linearize/delinearize.
// - Bitmap specialization for tNode == bool.
// - Make static functions private.
// - Better implementation of FindNeighbor()?
///////////////////////////////////////////////////////////////////////////////
template <typename tIndex, typename tNode>
class Quadtree
{
	int                  m_levelCount;
	eastl::vector<tNode> m_nodes;

public:
	typedef tIndex Index;
	typedef tNode  Node;
	static constexpr Index Index_Invalid  = ~Index(0);

	// Absolute max number of levels given number of index bits = bits/2.
	static constexpr int    GetAbsoluteMaxLevelCount()                                        { return (int)(sizeof(Index) * CHAR_BIT) / 2; }

	// Node count at _level = 4^_level.
	static constexpr Index  GetNodeCount(int _level)                                          { return 1 << (2 * _level); }

	// Width (in nodes) at _level = sqrt(GetNodeCount(_level)).
	static constexpr Index  GetWidth(int _level)                                              { return 1 << _level; }

	// Total node count = 4*(leafCount - 1)/3+1.
	static constexpr Index  GetTotalNodeCount(int _levelCount)                                { return 4 * (GetNodeCount(_levelCount - 1) - 1) / 3 + 1; }

	// Index of first node at _level.
	static constexpr Index  GetLevelStartIndex(int _level)                                    { return (_level == 0) ? 0 : GetTotalNodeCount(_level); } 

	// Neighbor at signed offset from _nodeIndex (or Index_Invalid if offset is outside the quadtree).
	static           Index  FindNeighbor(Index _nodeIndex, int _nodeLevel, int _offsetX, int _offsetY);

	// Given _index, find the quadtree level.
	static           int    FindLevel(Index _nodeIndex);

	// Convert _nodeIndex to a Cartesian offset relative to the quadtree origin at _nodeLevel.
	static           uvec2  ToCartesian(Index _nodeIndex, int _nodeLevel);

	// Convert Cartesian coordinates to an index.
	static           Index  ToIndex(Index _x, Index _y, int _nodeLevel);


	Quadtree(int _levelCount = GetAbsoluteMaxLevelCount(), Node _init = Node());
	~Quadtree();

	// Depth-first traversal of the quadtree starting at _root, call _onVisit for each node. 
	// _onVisit should be of the form ()(tIndex _nodeIndex, int _nodeLevel) -> bool.
	// Traversal proceeds to a node's children only if _onVisit returns true.
	template<typename OnVisit>
	void        traverse(OnVisit&& _onVisit, Index _rootIndex = 0);

	// Find a valid neighbor at _offsetX, _offsetY from the given node.
	Index       findValidNeighbor(Index _nodeIndex, int _nodeLevel, int _offsetX, int _offsetY, Node _invalidNode = Node());

	// Width of a node in leaf nodes at _levelIndex (e.g. quadtree width at level 0, 1 at max level).
	Index       getNodeWidth(int _levelIndex) const                                          { return GetWidth(FRM_MAX(m_levelCount - _levelIndex - 1, 0)); }

	// Node access.
	Node&       operator[](Index _index)                                                     { FRM_STRICT_ASSERT(_index < GetTotalNodeCount(m_levelCount)); return m_nodes[_index]; }
	const Node& operator[](Index _index) const                                               { FRM_STRICT_ASSERT(_index < GetTotalNodeCount(m_levelCount)); return m_nodes[_index]; }
	int         getTotalNodeCount() const                                                    { return GetTotalNodeCount(m_levelCount); }
	Index       getIndex(const Node& _node) const                                            { return &_node - m_nodes; }
	Index       getParentIndex(Index _childIndex, int _childLevel) const;
	Index       getFirstChildIndex(Index _parentIndex, int _parentLevel) const;

	// Level access.
	const Node* getLevel(int _levelIndex) const                                              { FRM_STRICT_ASSERT(_levelIndex < m_levelCount); return m_nodes + GetLevelStartIndex(_level); }
	Node*       getLevel(int _levelIndex)                                                    { FRM_STRICT_ASSERT(_levelIndex < m_levelCount); return m_nodes + GetLevelStartIndex(_level); }
	Index       getNodeCount(int _levelIndex) const                                          { return GetNodeCount(_levelIndex); }
	int         getLevelCount() const                                                        { return m_levelCount; }

	// Linearize/delinearize nodes for a level. This is useful e.g. when converting to/from a texture representation.
	void        linearize(int _levelIndex, Node* out_) const                                 { FRM_ASSERT(false); } // \todo
	void        delinearize(int _levelIndex, const Node* _in)                                { FRM_ASSERT(false); } // \todo
};


/*******************************************************************************

                                  Quadtree

*******************************************************************************/

#define FRM_QUADTREE_TEMPLATE_DECL template <typename tIndex, typename tNode>
#define FRM_QUADTREE_CLASS_DECL    Quadtree<tIndex, tNode>

FRM_QUADTREE_TEMPLATE_DECL 
tIndex FRM_QUADTREE_CLASS_DECL::FindNeighbor(Index _nodeIndex, int _nodeLevel, int _offsetX, int _offsetY)
{
	if (_nodeIndex == Index_Invalid)
	{
		return Index_Invalid;
	}
	uvec2 offset = ToCartesian(_nodeIndex, _nodeLevel) + uvec2(_offsetX, _offsetY);
	return ToIndex(offset.x, offset.y, _nodeLevel);
}

FRM_QUADTREE_TEMPLATE_DECL 
int FRM_QUADTREE_CLASS_DECL::FindLevel(Index _nodeIndex)
{
	for (int i = 0, n = GetAbsoluteMaxLevelCount(); i < n; ++i)
	{
		if (_nodeIndex < GetLevelStartIndex(i + 1))
		{
			return i;
		}
	}
	return -1;
}

FRM_QUADTREE_TEMPLATE_DECL 
uvec2 FRM_QUADTREE_CLASS_DECL::ToCartesian(Index _nodeIndex, int _nodeLevel)
{
 // traverse the index LSB -> MSB, deinterleaving bits
	_nodeIndex -= GetLevelStartIndex(_nodeLevel);
	Index width = 1;
	uvec2 ret = uvec2(0u);
	for (int i = 0; i < _nodeLevel; ++i, width <<= 1)
	{
		ret.y += (_nodeIndex & 1) * width; 
		_nodeIndex = _nodeIndex >> 1;
		ret.x += (_nodeIndex & 1) * width;
		_nodeIndex = _nodeIndex >> 1;
	}
	return ret;
}

FRM_QUADTREE_TEMPLATE_DECL 
tIndex FRM_QUADTREE_CLASS_DECL::ToIndex(Index _x, Index _y, int _nodeLevel)
{
 // _x or _y are outside the quadtree
	Index w = GetWidth(_nodeLevel);
	if (_x >= w || _y >= w)
	{
		return Index_Invalid;
	}

 // interleave _x and _y to produce the Morton code, add level offset
	Index ret = 0;
	for (Index i = 0; i < (sizeof(Index) * CHAR_BIT); ++i)
	{
		const Index mask = 1 << i;
		const Index base = i;
		ret = ret 
			| (_y & mask) << (base + 0) 
			| (_x & mask) << (base + 1)
			;
	}
	return ret + GetLevelStartIndex(_nodeLevel);
}


FRM_QUADTREE_TEMPLATE_DECL 
FRM_QUADTREE_CLASS_DECL::Quadtree(int _levelCount, Node _init)
	: m_levelCount(_levelCount)
{
	FRM_STATIC_ASSERT(!DataTypeIsSigned(FRM_DATA_TYPE_TO_ENUM(Index))); // use an unsigned type
	FRM_ASSERT(_levelCount <= GetAbsoluteMaxLevelCount()); // not enough bits in tIndex

	Index totalNodeCount = GetTotalNodeCount(_levelCount);
	m_nodes.reserve(totalNodeCount);
	for (Index i = 0, n = GetTotalNodeCount(_levelCount); i < n; ++i) 
	{
		m_nodes.emplace_back(_init);	
	}
}

FRM_QUADTREE_TEMPLATE_DECL 
FRM_QUADTREE_CLASS_DECL::~Quadtree()
{
	m_nodes.clear();
}

FRM_QUADTREE_TEMPLATE_DECL 
tIndex FRM_QUADTREE_CLASS_DECL::getParentIndex(Index _childIndex, int _childLevel) const
{
	if (_childLevel == 0) 
	{
		return Index_Invalid;
	}
	Index  childOffset  = GetLevelStartIndex(_childLevel);
	Index  parentOffset = GetLevelStartIndex(FRM_MAX(_childLevel - 1, 0));
	return parentOffset + ((_childIndex - childOffset) >> 2);
}

FRM_QUADTREE_TEMPLATE_DECL 
tIndex FRM_QUADTREE_CLASS_DECL::getFirstChildIndex(Index _parentIndex, int _parentLevel) const
{
	if (_parentLevel >= m_levelCount -1)
	{
		return Index_Invalid;
	}
	Index  parentOffset = GetLevelStartIndex(_parentLevel);
	Index  childOffset  = GetLevelStartIndex(_parentLevel + 1);
	return childOffset + ((_parentIndex - parentOffset) << 2);
}

FRM_QUADTREE_TEMPLATE_DECL
tIndex FRM_QUADTREE_CLASS_DECL::findValidNeighbor(Index _nodeIndex, int _nodeLevel, int _offsetX, int _offsetY, Node _invalidNode)
{
	Index ret = FindNeighbor(_nodeIndex, _nodeLevel, _offsetX, _offsetY); // get neighbor index at the same level
	while (ret != Index_Invalid && m_nodes[ret] == _invalidNode) // search up the tree until a valid node is found
	{
		ret = getParentIndex(ret, _nodeLevel--);
	}
	return ret;
}

FRM_QUADTREE_TEMPLATE_DECL 
template<typename OnVisit>
void FRM_QUADTREE_CLASS_DECL::traverse(OnVisit&& _onVisit, Index _root)
{
	struct NodeAddr { Index m_index; int m_level; }; // store level in the stack, avoid calling FindLevel()
	eastl::fixed_vector<NodeAddr, GetAbsoluteMaxLevelCount() * 4> tstack; // depth-first traversal has a small upper limit on the stack size
	tstack.push_back({ _root, FindLevel(_root) });
	while (!tstack.empty()) 
	{
		NodeAddr node = tstack.back();
		tstack.pop_back();
		if (eastl::forward<OnVisit>(_onVisit)(node.m_index, node.m_level) && node.m_level < m_levelCount - 1) 
		{
			Index firstChildIndex = getFirstChildIndex(node.m_index, node.m_level);
			tstack.push_back({ firstChildIndex + 0u, node.m_level + 1 });
			tstack.push_back({ firstChildIndex + 1u, node.m_level + 1 });
			tstack.push_back({ firstChildIndex + 2u, node.m_level + 1 });
			tstack.push_back({ firstChildIndex + 3u, node.m_level + 1 });
		}
	}
}

#undef FRM_QUADTREE_TEMPLATE_DECL
#undef FRM_QUADTREE_CLASS_DECL

} // namespace frm
