#pragma once

#include "INODE_TYPE.h"
#include "CoroutineGenerator.h"
#include "MAP_CELL_STATE.h"

#include <string>
#include <fstream>
#include <cstdint>
#include <generator>
#include <array>
#include <vector>
#include <span>

class FSManager
{
private:
	static constexpr uint32_t SUPERBLOCK_SIZE = 32;
	static constexpr uint32_t INODE_SIZE = 69;

	static constexpr float INODE_RATIO = 0.12f;
	static constexpr uint32_t CLUSTER_SIZE = 1024;
	static constexpr uint32_t BYTE_SIZE = 8;

	static constexpr uint32_t DIRECT_LINK_COUNT = 12;
	static constexpr uint32_t INDIRECT_LINK_1_COUNT = 1;
	static constexpr uint32_t INDIRECT_LINK_2_COUNT = 1;
	static constexpr uint32_t INDIRECT_LINK_3_COUNT = 1;

	static constexpr uint32_t INODE_LINK_OFFSET = sizeof(uint64_t) + sizeof(INODE_TYPE);
	static constexpr uint32_t MAX_FILENAME_LENGTH = 12;
	static constexpr uint32_t ADDRESS_LENGTH = sizeof(uint32_t);
public:
	FSManager(std::string filePath);
	~FSManager();

	void Format(uint64_t size);

	void CreateFolder(std::string name);
	uint32_t CreateINode(uint64_t size, INODE_TYPE type, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator);

	bool GetIMapValue(uint32_t index);
	bool GetDMapValue(uint32_t index);

	std::vector<std::pair<std::string, INODE_TYPE>> GetContent();

	uint32_t GetFreeIMapIndex();
	uint32_t GetFreeDMapIndex();

	void SetDMapIndex(uint32_t index, MAP_CELL_STATE value);
	void SetIMapIndex(uint32_t index, MAP_CELL_STATE value);

private:
	bool _inited;

	std::string _filePath;
	std::fstream _file;

	uint64_t _size;
	uint32_t _iMapStart;
	uint32_t _dMapStart;

	uint32_t _iMapSize;
	uint32_t _dMapSize;
	
	uint32_t _inodeCount;
	uint32_t _dataCount;

	uint32_t _inodeStart;
	uint32_t _dataStart;

	uint32_t _actualNodeIndex;

	void _initFromFile();

	void _createNewStats(uint64_t size);
	void _writeSuperblock();
	void _writeInodeMap();
	void _writeDataMap();
	void _writeEmptyInodes();
	void _writeEmptyData();

	void _createDefaultINode();

	void _addToFolder(std::string name, uint32_t address);

	void _setReadINodePointer(uint32_t index);
	CoroutineGenerator<std::pair<uint32_t, std::array<uint8_t, FSManager::CLUSTER_SIZE>>> _getInodeClusters();
	INODE_TYPE _getInodeType(uint32_t address);

	uint32_t _writeCluster(std::span<const uint8_t, CLUSTER_SIZE> clusterBytes);
	std::array<uint8_t, CLUSTER_SIZE> _readCluster(uint32_t address);

	bool _getMapValue(uint32_t mapIndex, uint32_t index);
	uint32_t _getFreeMapIndex(uint32_t mapIndex, uint32_t mapSize);
	void _setMapIndex(uint32_t mapIndex, uint32_t index, MAP_CELL_STATE value);

	uint32_t _createIndirectLink(uint8_t rank, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator);
};