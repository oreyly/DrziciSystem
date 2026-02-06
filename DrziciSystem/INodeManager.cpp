#include "INodeManager.h"

INodeManager::INodeManager(FSManager& fsManager) : _fsManager(fsManager)
{

}

void INodeManager::CreateINode()
{
	uint32_t index = _fsManager.GetFreeIMapIndex();

}