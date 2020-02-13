#pragma once

#include <frm/core/AppSample3d.h>

typedef frm::AppSample3d AppBase;

class StreamingQuadtreeTest: public AppBase
{
public:
	StreamingQuadtreeTest();
	virtual ~StreamingQuadtreeTest();

	virtual bool init(const frm::ArgList& _args) override;
	virtual void shutdown() override;
	virtual bool update() override;
	virtual void draw() override;

protected:

	frm::StreamingQuadtree* m_streamingQuadtree  = nullptr;
	int                     m_maxLoadPerFrame    = 1;
	int                     m_maxReleasePerFrame = 1;
};
