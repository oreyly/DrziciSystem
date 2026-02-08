#include "FSManager.h"

#include "magic_enum.hpp"

#include <bit>
#include <iostream>


FSManager::FSManager(std::string filePath) : _inited(false)
{
	_filePath = filePath;
	_file = std::fstream(_filePath, std::ios::in | std::ios::out | std::ios::binary);

	_initFromFile();
}

FSManager::~FSManager()
{
	_file.close();
}

void FSManager::Format(uint64_t size)
{
	_file = std::fstream(_filePath, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);

	_createNewStats(size);
	_writeSuperblock();
	_file.flush();
	_writeInodeMap();
	_file.flush();
	_writeDataMap();
	_file.flush();
	_writeEmptyInodes();
	_file.flush();
	_writeEmptyData();
	_file.flush();

	_createDefaultINode();
	_file.flush();

	_actualNodeIndex = 0;
	_inited = true;
}

void FSManager::_initFromFile()
{
	_file.seekg(0, std::ios::beg);

	_file.read(reinterpret_cast<char*>(&_size), sizeof(_size));
	_file.read(reinterpret_cast<char*>(&_iMapStart), sizeof(_iMapStart));
	_file.read(reinterpret_cast<char*>(&_dMapStart), sizeof(_dMapStart));
	_file.read(reinterpret_cast<char*>(&_inodeCount), sizeof(_inodeCount));
	_file.read(reinterpret_cast<char*>(&_dataCount), sizeof(_dataCount));
	_file.read(reinterpret_cast<char*>(&_inodeStart), sizeof(_inodeStart));
	_file.read(reinterpret_cast<char*>(&_dataStart), sizeof(_dataStart));

	if (_file.eof())
	{
		return;
	}

	_actualNodeIndex = 0;
	_inited = true;
}

void FSManager::_createNewStats(uint64_t size)
{
	_iMapStart = SUPERBLOCK_SIZE;
	_inodeCount = static_cast<uint32_t>(std::ceil(((size * INODE_RATIO) / INODE_SIZE) / BYTE_SIZE) * BYTE_SIZE);
	_iMapSize = _inodeCount / BYTE_SIZE;

	_dMapStart = _iMapStart + _iMapSize;
	uint64_t remainingSize = size - SUPERBLOCK_SIZE - _iMapSize - (_inodeCount * INODE_SIZE);
	_dataCount = static_cast<uint32_t>(std::floor((BYTE_SIZE * CLUSTER_SIZE * remainingSize) / (BYTE_SIZE * CLUSTER_SIZE + 1) / (CLUSTER_SIZE * BYTE_SIZE))) * BYTE_SIZE;
	_dMapSize = _dataCount / BYTE_SIZE;

	_size = static_cast<uint64_t>(CLUSTER_SIZE) + _iMapSize + _dMapSize + (static_cast<uint64_t>(_inodeCount) * INODE_SIZE) + ((static_cast<uint64_t>(_dataCount) * CLUSTER_SIZE));

	_inodeStart = _dMapStart + _dMapSize;
	_dataStart = _inodeStart + (_inodeCount * INODE_SIZE);
}

void FSManager::_writeSuperblock()
{
	_file.write(reinterpret_cast<char*>(&_size), sizeof(_size));
	_file.write(reinterpret_cast<char*>(&_iMapStart), sizeof(_iMapStart));
	_file.write(reinterpret_cast<char*>(&_dMapStart), sizeof(_dMapStart));
	_file.write(reinterpret_cast<char*>(&_inodeCount), sizeof(_inodeCount));
	_file.write(reinterpret_cast<char*>(&_dataCount), sizeof(_dataCount));
	_file.write(reinterpret_cast<char*>(&_inodeStart), sizeof(_inodeStart));
	_file.write(reinterpret_cast<char*>(&_dataStart), sizeof(_dataStart));

	std::cout << "Info:" << std::endl;

	std::cout << std::to_string(_size) << std::endl;
	std::cout << std::to_string(_iMapStart) << std::endl;
	std::cout << std::to_string(_dMapStart) << std::endl;
	std::cout << std::to_string(_inodeCount) << std::endl;
	std::cout << std::to_string(_dataCount) << std::endl;
	std::cout << std::to_string(_inodeStart) << std::endl;
	std::cout << std::to_string(_dataStart) << std::endl;
}

void FSManager::_writeInodeMap()
{
	uint8_t zero = 0;

	for (uint32_t i = 0; i < _iMapSize; ++i)
	{
		_file.write(reinterpret_cast<char*>(&zero), sizeof(zero));
	}
}

void FSManager::_writeDataMap()
{
	uint8_t zero = 0;

	for (uint32_t i = 0; i < _dMapSize; ++i)
	{
		_file.write(reinterpret_cast<char*>(&zero), sizeof(zero));
	}
}

void FSManager::_writeEmptyInodes()
{
	char emptyInode[INODE_SIZE] = {0};

	for (uint32_t i = 0; i < _inodeCount; ++i)
	{
		_file.write(reinterpret_cast<char*>(&emptyInode), sizeof(emptyInode));
	}
}

void FSManager::_writeEmptyData()
{
	char emptyCluster[CLUSTER_SIZE] = {0};

	for (uint32_t i = 0; i < _dataCount; ++i)
	{
		_file.write(reinterpret_cast<char*>(&emptyCluster), sizeof(emptyCluster));
	}
}

bool FSManager::_getMapValue(uint32_t mapIndex, uint32_t index)
{
	uint8_t mapByte;

	_file.seekg(mapIndex + (index / BYTE_SIZE), std::ios::beg);
	_file.read(reinterpret_cast<char*>(&mapByte), sizeof(mapByte));

	switch ((mapByte << (index % BYTE_SIZE)) & 0x1 )
	{
		case 0:
			return false;
		case 1:
			return true;
		default:
			throw std::domain_error("Bad internal bool calculation");
	}
}

uint32_t FSManager::_getFreeMapIndex(uint32_t mapIndex, uint32_t mapSize)
{
	std::streampos originalPos = _file.tellg();
	_file.seekg(mapIndex, std::ios::beg);

	for (uint32_t i = 0; i < mapSize; ++i)
	{
		uint8_t mapByte;

		_file.read(reinterpret_cast<char*>(&mapByte), sizeof(mapByte));

		if (mapByte == std::numeric_limits<uint8_t>::max())
		{
			continue;
		}

		uint8_t freeBit = 0b10000000;
		for (int j = 0; j < BYTE_SIZE; ++j)
		{
			if ((mapByte & freeBit) == 0)
			{
				_file.seekg(originalPos);
				return (i * BYTE_SIZE) + j;
			}

			freeBit >>= 1;
		}
	}

	throw std::out_of_range("Map is full");
}

bool FSManager::GetDMapValue(uint32_t index)
{
	return _getMapValue(_iMapStart, index);
}

bool FSManager::GetIMapValue(uint32_t index)
{
	return _getMapValue(_dMapStart, index);
}

uint32_t FSManager::GetFreeIMapIndex()
{
	return _getFreeMapIndex(_iMapStart, _iMapSize);
}

uint32_t FSManager::GetFreeDMapIndex()
{
	return _getFreeMapIndex(_dMapStart, _dMapSize);
}

uint32_t FSManager::CreateINode(uint64_t size, INODE_TYPE type, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator)
{
	uint32_t index = GetFreeIMapIndex();
	SetIMapIndex(index, MAP_CELL_STATE::USED);
	uint32_t freeAddress = 0;

	uint32_t inodeAddress = _inodeStart + (index * INODE_SIZE);

	_file.seekp(inodeAddress, std::ios::beg);

	_file.write(reinterpret_cast<char*>(&size), sizeof(size));
	_file.write(reinterpret_cast<char*>(&type), sizeof(type));

	uint32_t newAddress;

	for (uint32_t i = 0; i < DIRECT_LINK_COUNT; ++i)
	{
		if (!clusterGenerator.next())
		{
			_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
			continue;
		}

		newAddress = _writeNewCluster(clusterGenerator.value());
		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_1_COUNT; ++i)
	{
		newAddress = _createIndirectLink(1, clusterGenerator);

		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_2_COUNT; ++i)
	{
		newAddress = _createIndirectLink(2, clusterGenerator);

		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_3_COUNT; ++i)
	{
		newAddress = _createIndirectLink(3, clusterGenerator);

		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	_file.flush();
	return inodeAddress;
}

uint32_t FSManager::_createIndirectLink(uint8_t rank, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator)
{
	std::array<uint32_t, CLUSTER_SIZE / 4> childClusters = {};

	if (rank == 1)
	{
		for (int i = 0; i < childClusters.size(); ++i)
		{
			if (!clusterGenerator.next())
			{
				if (childClusters[0] == 0)
				{
					return 0;
				}

				break;
			}

			childClusters[i] = _writeNewCluster(clusterGenerator.value());
		}

		return _writeNewCluster(std::span<const uint8_t, CLUSTER_SIZE>(
			reinterpret_cast<const uint8_t*>(childClusters.data()),
			CLUSTER_SIZE));
	}

	for (int i = 0; i < childClusters.size(); ++i)
	{
		uint32_t clusterAddress = _createIndirectLink(rank - 1, clusterGenerator);

		if (clusterAddress == 0)
		{
			if (childClusters[0] == 0)
			{
				return 0;
			}

			break;
		}

		childClusters[i] = clusterAddress;
	}

	return _writeNewCluster(std::span<const uint8_t, CLUSTER_SIZE>(
		reinterpret_cast<const uint8_t*>(childClusters.data()),
		CLUSTER_SIZE));
}

uint32_t FSManager::_writeNewCluster(std::span<const uint8_t, CLUSTER_SIZE> clusterBytes)
{
	uint32_t index = GetFreeDMapIndex();
	SetDMapIndex(index, MAP_CELL_STATE::USED);

	uint32_t address = _dataStart + (index * CLUSTER_SIZE);

	std::streampos originalPos = _file.tellg();
	_file.seekp(address, std::ios::beg);
	_file.write(reinterpret_cast<const char*>(clusterBytes.data()), clusterBytes.size());
	_file.seekg(originalPos);
	_file.flush();

	return address;
}

void FSManager::_rewriteCluster(uint32_t address, std::span<const uint8_t, CLUSTER_SIZE> clusterBytes)
{
	std::streampos originalPos = _file.tellg();
	_file.seekp(address, std::ios::beg);
	_file.write(reinterpret_cast<const char*>(clusterBytes.data()), clusterBytes.size());
	_file.flush();
	_file.seekg(originalPos);
}

void FSManager::SetDMapIndex(uint32_t index, MAP_CELL_STATE value)
{
	_setMapIndex(_dMapStart, index, value);
}

void FSManager::SetIMapIndex(uint32_t index, MAP_CELL_STATE value)
{
	_setMapIndex(_iMapStart, index, value);
}

void FSManager::_setMapIndex(uint32_t mapIndex, uint32_t index, MAP_CELL_STATE value)
{
	uint8_t mapByte;

	std::streampos originalPos = _file.tellg();
	_file.seekg(mapIndex + (index / BYTE_SIZE), std::ios::beg);
	_file.read(reinterpret_cast<char*>(&mapByte), sizeof(mapByte));

	if (value == MAP_CELL_STATE::FREE)
	{
		uint8_t mask = static_cast<uint8_t>(0b01111111);
		mask = std::rotr(mask, index % BYTE_SIZE);

		mapByte &= mask;
	}
	else
	{
		uint8_t mask = static_cast<uint8_t>(0b10000000);
		mask = std::rotr(mask, index % BYTE_SIZE);

		mapByte |= mask;
	}

	_file.seekp(mapIndex + (index / BYTE_SIZE), std::ios::beg);
	_file.write(reinterpret_cast<char*>(&mapByte), sizeof(mapByte));
	_file.flush();
	_file.seekg(originalPos);
}

void FSManager::_createDefaultINode()
{
	SetIMapIndex(0, MAP_CELL_STATE::USED);

	uint64_t defaultSize = CLUSTER_SIZE;
	INODE_TYPE defaultType = INODE_TYPE::FOLDER;
	uint32_t freeAddress = 0;
	std::streampos originalPos = _file.tellg();
	_file.seekp(_inodeStart, std::ios::beg);
	_file.write(reinterpret_cast<char*>(&defaultSize), sizeof(defaultSize));
	originalPos = _file.tellg();
	_file.write(reinterpret_cast<char*>(&defaultType), sizeof(defaultType));
	originalPos = _file.tellg();
	_file.flush();
	std::array<uint8_t, CLUSTER_SIZE> emptyCluster {0};
	uint32_t firstClusterAddress = _writeNewCluster(emptyCluster);
	originalPos = _file.tellg();
	_file.write(reinterpret_cast<char*>(&firstClusterAddress), sizeof(firstClusterAddress));
	originalPos = _file.tellg();
	_file.flush();
	originalPos = _file.tellg();
	for (uint32_t i = 1; i < DIRECT_LINK_COUNT; ++i)
	{
		_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
		originalPos = _file.tellg();
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_1_COUNT; ++i)
	{
		_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
		originalPos = _file.tellg();
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_2_COUNT; ++i)
	{
		_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
		originalPos = _file.tellg();
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_3_COUNT; ++i)
	{
		_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
		originalPos = _file.tellg();
	}
	originalPos = _file.tellg();
	_file.flush();
	originalPos = _file.tellg();
}

std::vector<std::pair<std::string, INODE_TYPE>> FSManager::GetContent()
{
	std::vector<std::pair<std::string, INODE_TYPE>> content;

	for (auto& [_, cluster]: _getInodeClusters())
	{
		for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
		{
			std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
			if (filename.find_first_not_of('\0') == std::string::npos)
			{
				continue;
			}
			uint32_t address = std::bit_cast<uint32_t>(*(reinterpret_cast<const uint8_t(*)[ADDRESS_LENGTH]>(&cluster[i + MAX_FILENAME_LENGTH])));
			INODE_TYPE fileType = _getInodeType(address);

			content.push_back({filename, fileType});
		}
	}

	return content;
}

CoroutineGenerator<std::pair<uint32_t,std::array<uint8_t, FSManager::CLUSTER_SIZE>>> FSManager::_getInodeClusters()
{
	_setReadINodePointer(_actualNodeIndex);
	_file.seekg(INODE_LINK_OFFSET, std::ios::cur);

	uint32_t linkAddress;

	for (uint32_t i = 0; i < DIRECT_LINK_COUNT; ++i)
	{
		_file.read(reinterpret_cast<char*>(&linkAddress), sizeof(linkAddress));

		if (linkAddress == 0)
		{
			co_return;
		}

		co_yield {linkAddress,_readCluster(linkAddress)};
	}
}

void FSManager::_setReadINodePointer(uint32_t index)
{
	_file.seekg(_inodeStart + (index * INODE_SIZE));
}

std::array<uint8_t, FSManager::CLUSTER_SIZE> FSManager::_readCluster(uint32_t address)
{
	std::array<uint8_t, CLUSTER_SIZE> cluster = {};
	std::streampos originalPos = _file.tellg();

	_file.seekg(address, std::ios::beg);
	_file.read(reinterpret_cast<char*>(&cluster), sizeof(cluster));

	_file.seekg(originalPos);

	return cluster;
}

INODE_TYPE FSManager::_getInodeType(uint32_t address)
{
	uint8_t typeInt;
	std::streampos originalPos = _file.tellg();

	_file.seekg(address + sizeof(uint32_t), std::ios::beg);
	_file.read(reinterpret_cast<char*>(&typeInt), sizeof(typeInt));

	_file.seekg(originalPos);

	auto type = magic_enum::enum_cast<INODE_TYPE>(typeInt);

	if (!type.has_value())
	{
		throw std::runtime_error("Unknown inode type");
	}

	return type.value();
}

void FSManager::CreateFolder(std::string name)
{
	auto emptyGenerator = [] () -> CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>
		{
			std::array<uint8_t, CLUSTER_SIZE> emptyCluster{ 0 };
			co_yield emptyCluster;
		}();

	uint32_t newAddress = CreateINode(CLUSTER_SIZE, INODE_TYPE::FOLDER, emptyGenerator);
	_addToFolder(name, newAddress);
}

void FSManager::_addToFolder(std::string name, uint32_t address)
{
	for (auto& [clusterAddress, cluster] : _getInodeClusters())
	{
		for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
		{
			std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
			if (filename.find_first_not_of('\0') != std::string::npos)
			{
				continue;
			}

			std::array<uint8_t, CLUSTER_SIZE> newCluster = cluster;
			std::copy(name.begin(), name.end(), newCluster.begin() + i);
			std::fill(newCluster.begin() + i + name.length(), newCluster.begin() + i + MAX_FILENAME_LENGTH, 0);

			std::memcpy(newCluster.data() + i + MAX_FILENAME_LENGTH, &address, sizeof(address));

			_rewriteCluster(clusterAddress, newCluster);
			return;
		}
	}
}