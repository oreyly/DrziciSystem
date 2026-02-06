#pragma once

#include "FSManager.h"

class INodeManager
{
public:
	INodeManager(FSManager& fsManager);

	void CreateINode();

private:
	FSManager& _fsManager;

	uint32_t _getFreeIndex();
};