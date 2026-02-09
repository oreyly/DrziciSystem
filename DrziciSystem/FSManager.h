#pragma once

#include "INODE_TYPE.h"
#include "CoroutineGenerator.h"
#include "MAP_CELL_STATE.h"
#include "ReturnCodes.h"

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
	static constexpr uint32_t INODE_SIZE = 73;

	static constexpr float INODE_RATIO = 0.12f;
	static constexpr uint32_t CLUSTER_SIZE = 1024;
	static constexpr uint32_t BYTE_SIZE = 8;

	static constexpr uint32_t DIRECT_LINK_COUNT = 12;
	static constexpr uint32_t INDIRECT_LINK_1_COUNT = 1;
	static constexpr uint32_t INDIRECT_LINK_2_COUNT = 1;
	static constexpr uint32_t INDIRECT_LINK_3_COUNT = 1;

	static constexpr uint32_t INODE_LINK_OFFSET = sizeof(uint64_t) + sizeof(INODE_TYPE) + sizeof(uint32_t);
	static constexpr uint32_t MAX_FILENAME_LENGTH = 12;
	static constexpr uint32_t ADDRESS_LENGTH = sizeof(uint32_t);
public:
	bool Inited;

	FSManager(std::string filePath);
	~FSManager();

	void Format(uint64_t size);

	RETURN_CODES CreateFolder(std::string path,std::string name);
	uint32_t CreateINode(uint64_t size, INODE_TYPE type, uint32_t parent, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator);

	MAP_CELL_STATE GetIMapValue(uint32_t index);
	MAP_CELL_STATE GetDMapValue(uint32_t index);

	uint32_t GetIMapAddress(uint32_t index);
	uint32_t GetDMapAddress(uint32_t index);

	std::vector<std::pair<std::string, INODE_TYPE>> GetContent(uint32_t address);

	uint32_t GetFreeIMapIndex();
	uint32_t GetFreeDMapIndex();

	void SetDMapIndex(uint32_t index, MAP_CELL_STATE value);
	void SetIMapIndex(uint32_t index, MAP_CELL_STATE value);

	void SetDMapAddress(uint32_t address, MAP_CELL_STATE value);
	void SetIMapAddress(uint32_t address, MAP_CELL_STATE value);

	uint32_t GetInodeFromPath(std::string path);
	INODE_TYPE GetInodeType(uint32_t address);

	RETURN_CODES DeleteFolder(std::string path);
	std::vector<std::string> GetCurrentPath();
	RETURN_CODES OpenFolder(std::string path);
	uint64_t GetINodeSize(uint32_t address);
	uint32_t GetINodeIndex(uint32_t address);
	std::string GetINodeName(uint32_t address);
	std::vector<uint64_t> GetStats();

	void FileFromPC(std::filesystem::path sourcePath, std::filesystem::path targetPath);
	void PrintFile(uint32_t address);
	void DeleteFile(uint32_t address, std::string name);
	void MoveFile(uint32_t address, std::filesystem::path targetPath);
	void CopyFile(uint32_t address, std::filesystem::path targetPath);
	void FileToPC(uint32_t address, std::ofstream& oFile);
	void ExtendFile(uint32_t sourceAddress, uint32_t targetAddress);

private:
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

	uint32_t _actualNodeAddress;

	void _initFromFile();

	uint32_t _getSubInodeAddress(uint32_t parentAddress, std::string subName);

	void _createNewStats(uint64_t size);
	void _writeSuperblock();
	void _writeInodeMap();
	void _writeDataMap();
	void _writeEmptyInodes();
	void _writeEmptyData();

	void _createDefaultINode();

	void _addToFolder(uint32_t parentAddress, std::string name, uint32_t address);

	void _setReadINodePointer(uint32_t index);
	CoroutineGenerator<std::pair<uint32_t, std::array<uint8_t, FSManager::CLUSTER_SIZE>>> _getInodeClusters(uint32_t inodeAddress);
	CoroutineGenerator<std::pair<uint32_t, std::array<uint8_t, FSManager::CLUSTER_SIZE>>> _getInodeIndirectClusters(uint8_t rank, uint32_t clusterAddress);

	uint32_t _writeNewCluster(std::span<const uint8_t, CLUSTER_SIZE> clusterBytes);
	void _rewriteCluster(uint32_t address, std::span<const uint8_t, CLUSTER_SIZE> clusterBytes);
	std::array<uint8_t, CLUSTER_SIZE> _readCluster(uint32_t address);

	MAP_CELL_STATE _getMapValue(uint32_t mapStart, uint32_t index);
	uint32_t _getFreeMapIndex(uint32_t mapStart, uint32_t mapSize);
	void _setMapIndex(uint32_t mapStart, uint32_t index, MAP_CELL_STATE value);
	uint32_t _getMapAddress(uint32_t mapStart, uint32_t index, uint32_t structureSize);

	uint32_t _createIndirectLink(uint8_t rank, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator);
	void _deleteINode(uint32_t address, std::string name);
	void _deleteIndirectLink(uint8_t rank, uint32_t address);

	void _extendINode(uint32_t address, uint64_t bonusSize, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusters);
	uint32_t _extendIndirectLink(uint8_t rank, uint32_t oldAddress, uint32_t& clusterToSkip, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator);
};