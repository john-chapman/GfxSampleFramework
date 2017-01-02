#include <frm/TextureAtlas.h>

#include <frm/Texture.h>

#include <apt/Image.h>

#ifdef frm_TextureAtlas_DEBUG
	#include <imgui/imgui.h>
	static std::vector<frm::TextureAtlas::Region*> s_dbgRegionList;
#endif

using namespace frm;
using namespace apt;

/*******************************************************************************

                                 TextureAtlas

*******************************************************************************/

struct TextureAtlas::Node
{
	Node* m_parent;
	Node* m_children[4];

	bool m_isEmpty;
	uint16 m_sizeX, m_sizeY;
	uint16 m_startX, m_startY;

	Node(Node* _parent)
		: m_parent(_parent)
		, m_isEmpty(true)
	{
		m_children[0] = 0;
		m_children[1] = 0;
		m_children[2] = 0;
		m_children[3] = 0;
	}

	bool isLeaf() const  { return m_children[0] == 0; }
	bool isEmpty() const { return m_isEmpty; }
};


// PUBLIC

TextureAtlas* TextureAtlas::Create(GLsizei _width, GLsizei _height, GLenum _format, GLsizei _mipCount)
{
	uint64 id = GetUniqueId();
	APT_ASSERT(!Find(id)); // id collision
	TextureAtlas* ret = new TextureAtlas(id, "", _format, _width, _height, _mipCount);
	ret->setNamef("%llu", id);
	Use((Texture*&)ret);
	return ret;
}

void TextureAtlas::Destroy(TextureAtlas*& _inst_)
{
	APT_ASSERT(_inst_);
	TextureAtlas* inst = _inst_; // make a copy because Unuse will nullify _inst_
	Release((Texture*&)_inst_);
	if (inst->getRefCount() == 0) {
		delete inst;
	}
}


TextureAtlas::Region* TextureAtlas::alloc(GLsizei _width, GLsizei _height)
{
	Node* node = insert(m_root, _width, _height);
	if (node) {
		Region* ret = m_regionPool.alloc();
		ret->m_uvScale = vec2(_width, _height) * m_rsize; // note it's the requested size, not the node size
		ret->m_uvBias = vec2(node->m_startX, node->m_startY) * m_rsize;
		
		if (isCompressed()) {
		 // compressed atlas, the smallest usable region is 4x4, hence the max lod is log2(w/4)
			ret->m_lodMax = APT_MIN((int)log2((double)(_width / 4)), (int)log2((double)(_height / 4)));
		} else {
		 // uncompressed, the smallest usable region is log2(w)
			ret->m_lodMax = APT_MIN((int)log2((double)(_width)), (int)log2((double)(_height)));
		}
		
		#ifdef frm_TextureAtlas_DEBUG
			s_dbgRegionList.push_back(ret);
		#endif
		return ret;
	}

	return 0;
}

TextureAtlas::Region* TextureAtlas::alloc(const apt::Image& _img, RegionId _id)
{
	APT_ASSERT(_img.getType() == Image::k2d);
	Region* ret = alloc((GLsizei)_img.getWidth(), (GLsizei)_img.getHeight());

	GLenum srcFormat;
	switch (_img.getLayout()) {
		case Image::Layout::kR:    srcFormat = GL_RED;  break;
		case Image::Layout::kRG:   srcFormat = GL_RG;   break;
		case Image::Layout::kRGB:  srcFormat = GL_RGB;  break;
		case Image::Layout::kRGBA: srcFormat = GL_RGBA; break;
		default:                   APT_ASSERT(false); return false;
	};
	GLenum srcType = _img.isCompressed() ? GL_UNSIGNED_BYTE : internal::GlDataTypeToEnum(_img.getImageDataType());
	int mipMax = APT_MIN(APT_MIN((int)getMipCount(), (int)_img.getMipmapCount()), ret->m_lodMax + 1);
	for (int mip = 0; mip < mipMax; ++mip) {
		if (_img.isCompressed()) { // \hack, see Texture.h
			srcType = (GLenum)_img.getRawImageSize(mip);
		}
		upload(*ret, _img.getRawImage(0, mip), srcFormat, srcType, mip);
	}

	if (_id != 0) {
		m_regionMap.push_back(RegionRef());
		m_regionMap.back().m_id = _id;
		m_regionMap.back().m_region = ret;
		m_regionMap.back().m_refCount = 1;
	}
	return ret;
}

void TextureAtlas::free(Region*& _region_)
{
	APT_ASSERT(_region_);
	#ifdef APT_DEBUG
		for (auto it = m_regionMap.begin(); it != m_regionMap.end(); ++it) {
			if (it->m_region == _region_) {
				APT_ASSERT(false); // named regions must be freed via unuseFree()
			}
		}
	#endif

	Node* node = find(*_region_);
	APT_ASSERT(node);
	remove(node);

	m_regionPool.free(_region_);
	_region_ = 0;
}

TextureAtlas::Region* TextureAtlas::findUse(RegionId _id)
{
	for (auto it = m_regionMap.begin(); it != m_regionMap.end(); ++it) {
		if (it->m_id == _id) {
			++it->m_refCount;
			return it->m_region;
		}
	}
	return 0; // not found
}

void TextureAtlas::unuseFree(Region*& _region_)
{
	for (auto it = m_regionMap.begin(); it != m_regionMap.end(); ++it) {
		if (it->m_region == _region_) {
			if (--it->m_refCount == 0) {
				m_regionMap.erase(it);
				free(_region_);
			}
			_region_ = 0; // always null the ptr
			return;
		}
	}
	APT_ASSERT(false); // region not found, didn't set the region name via alloc()?
}

void TextureAtlas::upload(const Region& _region, const void* _data, GLenum _dataFormat, GLenum _dataType, GLint _mip)
{
	APT_ASSERT(_mip < getMipCount());
	
	GLsizei x = (GLsizei)(_region.m_uvBias.x  * (float)getWidth());
	GLsizei y = (GLsizei)(_region.m_uvBias.y  * (float)getHeight());
	GLsizei w = (GLsizei)(_region.m_uvScale.x * (float)getWidth());
	GLsizei h = (GLsizei)(_region.m_uvScale.y * (float)getHeight());
	
	GLsizei div = (GLsizei)pow(2.0, (double)_mip);
	w = w  / div;
	h = h  / div;
	x = x  / div;
	y = y  / div;

	setSubData(x, y, 0, w, h, 0, _data, _dataFormat, _dataType, _mip);
}


// PROTECTED

TextureAtlas::TextureAtlas(
	uint64      _id, 
	const char* _name,
	GLenum      _format,
	GLsizei     _width, 
	GLsizei     _height,
	GLsizei     _mipCount	
	)
	: Texture(_id, _name, GL_TEXTURE_2D, _width, _height, 0, 0, _mipCount, _format)
	, m_regionPool(256)
{

	m_rsize = 1.0f / vec2(getWidth(), getHeight());

 // init allocator
 	m_nodePool = new Pool<Node>(128);
	m_root = m_nodePool->alloc(Node(0));
	m_root->m_startX = m_root->m_startY = 0;
	m_root->m_sizeX = getWidth();
	m_root->m_sizeY = getHeight();
}

TextureAtlas::~TextureAtlas()
{
 // destroy allocator
	m_nodePool->free(m_root);
	delete m_nodePool;
}

// PRIVATE

TextureAtlas::Node* TextureAtlas::insert(Node* _root, uint16 _sizeX, uint16 _sizeY)
{
	if (!_root->isEmpty()) {
		return 0;
	}
	
	if (_root->isLeaf()) {
	 // node is too small
		if (_root->m_sizeX < _sizeX || _root->m_sizeY < _sizeY) {
			return 0;
		}
	 // node is best fit
		uint16 nextSizeX = _root->m_sizeX / 2;
		uint16 nextSizeY = _root->m_sizeY / 2;
		if (nextSizeX < _sizeX || nextSizeY < _sizeY) {
			_root->m_isEmpty = false;
			return _root;
		}

	 // subdivide the node
	 // +---+---+
	 // | 0 | 1 |
	 // +---+---+
	 // | 3 | 2 |
	 // +---+---+
		for (int i = 0; i < 4; ++i) {
			_root->m_children[i] = m_nodePool->alloc(Node(_root));
			_root->m_children[i]->m_sizeX = nextSizeX;
			_root->m_children[i]->m_sizeY = nextSizeY;
		}
		_root->m_children[0]->m_startX = _root->m_startX;
		_root->m_children[0]->m_startY = _root->m_startY;
		_root->m_children[1]->m_startX = _root->m_startX + nextSizeX;
		_root->m_children[1]->m_startY = _root->m_startY;
		_root->m_children[2]->m_startX = _root->m_startX + nextSizeX;
		_root->m_children[2]->m_startY = _root->m_startY + nextSizeY;		
		_root->m_children[3]->m_startX = _root->m_startX;
		_root->m_children[3]->m_startY = _root->m_startY + nextSizeY;

		return insert(_root->m_children[0], _sizeX, _sizeY);

	} else {
		Node* ret = 0;
		for (int i = 0; i < 4; ++i) {
			ret = insert(_root->m_children[i], _sizeX, _sizeY);
			if (ret != 0) {
				break;
			}
		}
		return ret;
	}

}

void TextureAtlas::remove(Node*& _node_)
{
	APT_ASSERT(_node_->isLeaf());
	_node_->m_isEmpty = true;
	Node* parent = _node_->m_parent;

 // remove parent if all children are empty leaves
 	for (int i = 0; i < 4; ++i) {
		bool canRemove = parent->m_children[i]->isEmpty() && parent->m_children[i]->isLeaf();
		if (!canRemove) {
			return;
		}
	}
	for (int i = 0; i < 4; ++i) {
		m_nodePool->free(parent->m_children[i]);
		parent->m_children[i] = 0;
	}
	if (parent != m_root) {
		remove(parent);
	}
}

// \todo improve find(), search for the node corner rather than the node center
TextureAtlas::Node* TextureAtlas::find(const Region& _region)
{
 // convert region -> node center
	uint16 startX = (uint16)(_region.m_uvBias.x  * (float)getWidth());
	uint16 startY = (uint16)(_region.m_uvBias.y  * (float)getHeight());
	uint16 sizeX  = (uint16)(_region.m_uvScale.x * (float)getWidth());
	uint16 sizeY  = (uint16)(_region.m_uvScale.y * (float)getHeight());
	uint16 originX = startX + sizeX / 2;
	uint16 originY = startY + sizeY / 2;

	return find(m_root, originX, originY);
}

TextureAtlas::Node* TextureAtlas::find(Node* _root, uint16 _originX, uint16 _originY)
{
	uint16 x0 = _root->m_startX;
	uint16 y0 = _root->m_startY;
	uint16 x1 = x0 + _root->m_sizeX; 
	uint16 y1 = y0 + _root->m_sizeY;
	if (_originX >= x0 && _originX <= x1 && _originY >= y0 && _originY <= y1) {
		if (_root->isLeaf()) {
			return _root;
		}
		for (int i = 0; i < 4; ++i) {
			Node* ret = find(_root->m_children[i], _originX, _originY);
			if (ret) {
				return ret;
			}
		}
	}
	return 0;
}



#ifdef frm_TextureAtlas_DEBUG
	static const ImU32 kDbgColorBackground = ImColor(0.1f, 0.1f, 0.1f, 1.0f);
	static const ImU32 kDbgColorLines      = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
	static const float kDbgLineThickness   = 1.0f;
	void TextureAtlas::debug()
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const vec2 drawSize = ImGui::GetContentRegionAvail();
		const vec2 drawStart = vec2(ImGui::GetWindowPos()) + vec2(ImGui::GetCursorPos());
		const vec2 drawEnd   = drawStart + drawSize;
		drawList->AddRectFilled(drawStart, drawStart + drawSize, kDbgColorBackground);
	
		const vec2 txSize = vec2(getWidth(), getHeight());
		const vec2 buttonStart = ImGui::GetCursorPos();
		for (int i = 0; i < s_dbgRegionList.size(); ++i) {
			ImGui::PushID(i);
			vec2 start = s_dbgRegionList[i]->m_uvBias * drawSize;
			vec2 size  = s_dbgRegionList[i]->m_uvScale * drawSize;
			ImGui::SetCursorPos(buttonStart + start);
			if (ImGui::Button("", size)) {
				free(s_dbgRegionList[i]);
				s_dbgRegionList.erase(s_dbgRegionList.begin() + i);
			} else {
				if (ImGui::IsItemHovered()) {
					ImGui::BeginTooltip();
						ImGui::Text("Uv Bias:  %1.2f, %1.2f", s_dbgRegionList[i]->m_uvBias.x, s_dbgRegionList[i]->m_uvBias.y);
						ImGui::Text("Uv Scale: %1.2f, %1.2f", s_dbgRegionList[i]->m_uvScale.x, s_dbgRegionList[i]->m_uvScale.y);
						ImGui::Text("Max Lod:  %d", s_dbgRegionList[i]->m_lodMax);
					ImGui::EndTooltip();
				}
			}
			ImGui::PopID();
		}
	
		drawList->AddLine(vec2(drawStart.x, drawStart.y), vec2(drawEnd.x,   drawStart.y), kDbgColorLines, kDbgLineThickness);
		drawList->AddLine(vec2(drawEnd.x,   drawStart.y), vec2(drawEnd.x,   drawEnd.y),   kDbgColorLines, kDbgLineThickness);
		drawList->AddLine(vec2(drawEnd.x,   drawEnd.y),   vec2(drawStart.x, drawEnd.y),   kDbgColorLines, kDbgLineThickness);
		drawList->AddLine(vec2(drawStart.x, drawEnd.y),   vec2(drawStart.x, drawStart.y), kDbgColorLines, kDbgLineThickness);
		debugDrawNode(m_root, drawStart, drawSize);
	}
	void TextureAtlas::debugDrawNode(const Node* _node, const vec2& _drawStart, const vec2& _drawSize)
	{
		if (!_node->isLeaf()) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
	
	
			vec2 a, b;
			a = vec2(_node->m_startX + _node->m_sizeX / 2, _node->m_startY                  ) * m_rsize;
			b = vec2(_node->m_startX + _node->m_sizeX / 2, _node->m_startY + _node->m_sizeY ) * m_rsize;
			drawList->AddLine(_drawStart + a * _drawSize, _drawStart + b * _drawSize, kDbgColorLines, kDbgLineThickness);
			
			a = vec2(_node->m_startX,                      _node->m_startY + _node->m_sizeY / 2) * m_rsize;
			b = vec2(_node->m_startX + _node->m_sizeX,     _node->m_startY + _node->m_sizeY / 2) * m_rsize;
			drawList->AddLine(_drawStart + a * _drawSize, _drawStart + b * _drawSize, kDbgColorLines, kDbgLineThickness);
			
			for (int i = 0; i < 4; ++i) {
				debugDrawNode(_node->m_children[i], _drawStart, _drawSize);
			}
		}
	}
#endif // frm_TextureAtlas_DEBUG
