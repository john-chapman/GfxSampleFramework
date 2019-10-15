#include "File.h"

#include <frm/core/memory.h>

namespace frm {

// PUBLIC

void File::setData(const char* _data, uint _size)
{
	m_data.reserve(_size);
	if (_data)
	{
		m_data.assign(_data, _data + _size);
	}
	else
	{
		m_data.assign(_size, '\0');
	}
}

void File::appendData(const char* _data, uint _size)
{
	uint currentSize = getDataSize();
	m_data.reserve(currentSize + _size);
	if (_data)
	{
		m_data.insert(m_data.begin() + currentSize, _data, _data + _size);
	}
	else
	{
		m_data.insert(m_data.begin() + currentSize, _size, '\0');
	}
}

void File::reserveData(uint _capacity)
{
	m_data.reserve(_capacity);
}

} // namespace frm
