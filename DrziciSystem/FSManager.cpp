#include "FSManager.h"

#include "magic_enum.hpp"

#include <bit>
#include <iostream>
#include <string_view>
#include <ranges>
#include <filesystem>


FSManager::FSManager(std::string filePath) : Inited(false)
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

	_actualNodeAddress = _inodeStart;
	Inited = true;
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

	_iMapSize = _inodeCount / BYTE_SIZE;
	_dMapSize = _dataCount / BYTE_SIZE;

	if (_file.eof())
	{
		return;
	}

	_actualNodeAddress = _inodeStart;
	Inited = true;
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
	/*
	std::cout << "Info:" << std::endl;

	std::cout << std::to_string(_size) << std::endl;
	std::cout << std::to_string(_iMapStart) << std::endl;
	std::cout << std::to_string(_dMapStart) << std::endl;
	std::cout << std::to_string(_inodeCount) << std::endl;
	std::cout << std::to_string(_dataCount) << std::endl;
	std::cout << std::to_string(_inodeStart) << std::endl;
	std::cout << std::to_string(_dataStart) << std::endl;*/
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

MAP_CELL_STATE FSManager::_getMapValue(uint32_t mapStart, uint32_t index)
{
	uint8_t mapByte;

	_file.seekg(mapStart + (index / BYTE_SIZE), std::ios::beg);
	_file.read(reinterpret_cast<char*>(&mapByte), sizeof(mapByte));
	uint8_t mask = static_cast<uint8_t>(0b10000000);

	mapByte = (mapByte << (index % BYTE_SIZE)) & mask;

	switch (std::rotl(mapByte, 1))
	{
		case 0:
			return MAP_CELL_STATE::FREE;
		case 1:
			return MAP_CELL_STATE::USED;
		default:
			throw std::domain_error("Bad internal bool calculation");
	}
}

uint32_t FSManager::_getFreeMapIndex(uint32_t mapStart, uint32_t mapSize)
{
	std::streampos originalPos = _file.tellg();
	_file.seekg(mapStart, std::ios::beg);

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

MAP_CELL_STATE FSManager::GetDMapValue(uint32_t index)
{
	return _getMapValue(_dMapStart, index);
}

MAP_CELL_STATE FSManager::GetIMapValue(uint32_t index)
{
	return _getMapValue(_iMapStart, index);
}

uint32_t FSManager::GetFreeIMapIndex()
{
	return _getFreeMapIndex(_iMapStart, _iMapSize);
}

uint32_t FSManager::GetFreeDMapIndex()
{
	return _getFreeMapIndex(_dMapStart, _dMapSize);
}

uint32_t FSManager::CreateINode(uint64_t size, INODE_TYPE type, uint32_t parent, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator)
{
	std::streampos originalPos = _file.tellg();
	uint32_t index = GetFreeIMapIndex();
	SetIMapIndex(index, MAP_CELL_STATE::USED);
	uint32_t freeAddress = 0;

	uint32_t inodeAddress = _inodeStart + (index * INODE_SIZE);

	_file.seekp(inodeAddress, std::ios::beg);

	_file.write(reinterpret_cast<char*>(&size), sizeof(size));
	_file.write(reinterpret_cast<char*>(&type), sizeof(type));
	_file.write(reinterpret_cast<char*>(&parent), sizeof(parent));

	uint32_t newAddress;

	for (uint32_t i = 0; i < DIRECT_LINK_COUNT; ++i)
	{
		if (!clusterGenerator.next())
		{
			_file.seekg(inodeAddress + INODE_LINK_OFFSET + (i * sizeof(uint32_t)), std::ios::beg);
			_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
			continue;
		}

		newAddress = _writeNewCluster(clusterGenerator.value());
		_file.seekg(inodeAddress + INODE_LINK_OFFSET + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_1_COUNT; ++i)
	{
		newAddress = _createIndirectLink(1, clusterGenerator);

		_file.seekg(inodeAddress + INODE_LINK_OFFSET + (DIRECT_LINK_COUNT * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_2_COUNT; ++i)
	{
		newAddress = _createIndirectLink(2, clusterGenerator);

		_file.seekg(inodeAddress + INODE_LINK_OFFSET + ((DIRECT_LINK_COUNT + INDIRECT_LINK_1_COUNT) * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_3_COUNT; ++i)
	{
		newAddress = _createIndirectLink(3, clusterGenerator);

		_file.seekg(inodeAddress + INODE_LINK_OFFSET + ((DIRECT_LINK_COUNT + INDIRECT_LINK_1_COUNT + INDIRECT_LINK_2_COUNT) * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	_file.flush();
	_file.seekg(originalPos, std::ios::beg);
	return inodeAddress;
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

void FSManager::_setMapIndex(uint32_t mapStart, uint32_t index, MAP_CELL_STATE value)
{
	uint8_t mapByte;

	std::streampos originalPos = _file.tellg();
	_file.seekg(mapStart + (index / BYTE_SIZE), std::ios::beg);
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

	_file.seekp(mapStart + (index / BYTE_SIZE), std::ios::beg);
	_file.write(reinterpret_cast<char*>(&mapByte), sizeof(mapByte));
	_file.flush();
	_file.seekg(originalPos);
}

void FSManager::_createDefaultINode()
{
	std::streampos originalPos = _file.tellg();
	SetIMapIndex(0, MAP_CELL_STATE::USED);

	uint64_t defaultSize = CLUSTER_SIZE;
	INODE_TYPE defaultType = INODE_TYPE::FOLDER;
	uint32_t freeAddress = 0;
	_file.seekp(_inodeStart, std::ios::beg);
	_file.write(reinterpret_cast<char*>(&defaultSize), sizeof(defaultSize));
	_file.write(reinterpret_cast<char*>(&defaultType), sizeof(defaultType));
	_file.write(reinterpret_cast<char*>(&_inodeStart), sizeof(_inodeStart));
	_file.flush();
	std::array<uint8_t, CLUSTER_SIZE> emptyCluster {0};
	uint32_t firstClusterAddress = _writeNewCluster(emptyCluster);
	_file.write(reinterpret_cast<char*>(&firstClusterAddress), sizeof(firstClusterAddress));
	for (uint32_t i = 1; i < DIRECT_LINK_COUNT; ++i)
	{
		_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_1_COUNT; ++i)
	{
		_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_2_COUNT; ++i)
	{
		_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_3_COUNT; ++i)
	{
		_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
	}

	_file.flush();
	_file.seekg(originalPos);
}

std::vector<std::pair<std::string, INODE_TYPE>> FSManager::GetContent(uint32_t address)
{
	std::vector<std::pair<std::string, INODE_TYPE>> content;

	for (auto& [_, cluster]: _getInodeClusters(address))
	{
		for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
		{
			std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
			if (filename[0] == '\0')
			{
				continue;
			}
			uint32_t address = std::bit_cast<uint32_t>(*(reinterpret_cast<const uint8_t(*)[ADDRESS_LENGTH]>(&cluster[i + MAX_FILENAME_LENGTH])));
			INODE_TYPE fileType = GetInodeType(address);

			content.push_back({filename, fileType});
		}
	}

	return content;
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
	_file.read(reinterpret_cast<char*>(&cluster), sizeof(cluster)); // incp D:\src\ceplusplus\DrziciSystem\ahojda.txt 4.txt

	_file.seekg(originalPos);

	return cluster;
}

INODE_TYPE FSManager::GetInodeType(uint32_t address)
{
	uint8_t typeInt;
	std::streampos originalPos = _file.tellg();

	_file.seekg(address + sizeof(uint64_t), std::ios::beg);
	_file.read(reinterpret_cast<char*>(&typeInt), sizeof(typeInt));

	_file.seekg(originalPos);

	auto type = magic_enum::enum_cast<INODE_TYPE>(typeInt);

	if (!type.has_value())
	{
		throw std::runtime_error("Unknown inode type");
	}

	return type.value();
}

void FSManager::_addToFolder(uint32_t parentAddress, std::string name, uint32_t address)
{
	for (auto& [clusterAddress, cluster] : _getInodeClusters(parentAddress))
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

uint32_t FSManager::GetInodeFromPath(std::string path)
{
	if (path == "")
	{
		return _actualNodeAddress;
	}

	std::vector<std::string> pathParts;

	for (auto&& part : path | std::views::transform([] (char c)
		{
			return (c == '/' || c == '\\') ? '/' : c;
		}) | std::views::split('/'))
	{
		pathParts.emplace_back(part.begin(), part.end());
	}

	uint32_t address;

	if (pathParts[0] == "")
	{
		address = _inodeStart;
		pathParts.erase(pathParts.begin());
	}
	else
	{
		address = _actualNodeAddress;
	}

	for (std::string part : pathParts)
	{
		address = _getSubInodeAddress(address, part);
		if (address == 0)
		{
			return 0;
		}
	}

	return address;
}

uint32_t FSManager::_getSubInodeAddress(uint32_t parentAddress, std::string subName)
{

	if (GetInodeType(parentAddress) == INODE_TYPE::FILE)
	{
		return 0;
	}

	if (subName == ".")
	{
		return parentAddress;
	}

	if (subName == "..")
	{
		uint32_t address;
		std::streampos originalPos = _file.tellg();
		_file.seekg(parentAddress + sizeof(uint64_t) + sizeof(uint8_t), std::ios::beg);
		_file.read(reinterpret_cast<char*>(&address), sizeof(address));
		_file.seekg(originalPos);
		return address;
	}

	subName.resize(MAX_FILENAME_LENGTH, '\0');
	subName[MAX_FILENAME_LENGTH - 1] = '\0';

	for (auto& [clusterAddress, cluster] : _getInodeClusters(parentAddress))
	{
		for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
		{
			std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
			if (filename[0] == '\0')
			{
				continue;
			}

			if (filename == subName)
			{
				uint32_t address;
				std::memcpy(&address, &cluster[i + MAX_FILENAME_LENGTH], sizeof(uint32_t));
				return address;
			}
		}
	}

	return 0;
}

void FSManager::_deleteINode(uint32_t inodeAddress, std::string name)
{
	std::streampos originalPos = _file.tellg();

	_file.seekg(inodeAddress + sizeof(uint64_t) + sizeof(uint8_t));

	uint32_t parentAddress;

	_file.read(reinterpret_cast<char*>(&parentAddress), sizeof(parentAddress));

	name.resize(MAX_FILENAME_LENGTH, '\0');
	name[MAX_FILENAME_LENGTH - 1] = '\0';

	for (auto& [clusterAddress, cluster] : _getInodeClusters(parentAddress))
	{
		bool rewrited = false;

		for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
		{
			std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
			if (filename[0] == '\0')
			{
				continue;
			}

			if (filename == name)
			{
				std::array<uint8_t, CLUSTER_SIZE> newCluster = cluster;

				std::fill(newCluster.begin() + i, newCluster.begin() + i + MAX_FILENAME_LENGTH + sizeof(uint32_t), 0);

				_rewriteCluster(clusterAddress, newCluster);
				rewrited = true;
				break;
			}
		}

		if (rewrited)
		{
			break;
		}
	}

	_file.seekg(inodeAddress + INODE_LINK_OFFSET, std::ios::beg);

	uint32_t address;

	for (uint32_t i = 0; i < DIRECT_LINK_COUNT; ++i)
	{
		_file.read(reinterpret_cast<char*>(&address), sizeof(address));
		SetDMapAddress(address, MAP_CELL_STATE::FREE);
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_1_COUNT; ++i)
	{
		_file.read(reinterpret_cast<char*>(&address), sizeof(address));

		_deleteIndirectLink(1, address);

		SetDMapAddress(address, MAP_CELL_STATE::FREE);
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_2_COUNT; ++i)
	{
		_file.read(reinterpret_cast<char*>(&address), sizeof(address));

		_deleteIndirectLink(2, address);

		SetDMapAddress(address, MAP_CELL_STATE::FREE);
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_3_COUNT; ++i)
	{
		_file.read(reinterpret_cast<char*>(&address), sizeof(address));

		_deleteIndirectLink(3, address);

		SetDMapAddress(address, MAP_CELL_STATE::FREE);
	}

	SetIMapAddress(inodeAddress, MAP_CELL_STATE::FREE);
	_file.seekg(originalPos);
}

void FSManager::_deleteIndirectLink(uint8_t rank, uint32_t address)
{
	if (address == 0)
	{
		return;
	}

	std::streampos originalPos = _file.tellg();
	uint32_t subAdress;
	if (rank == 1)
	{
		_file.seekg(address);

		for (int i = 0; i < CLUSTER_SIZE / sizeof(uint32_t); ++i)
		{
			_file.read(reinterpret_cast<char*>(&subAdress), sizeof(subAdress));

			if (subAdress == 0)
			{
				continue;
			}

			SetDMapAddress(subAdress, MAP_CELL_STATE::FREE);
		}

		_file.seekg(originalPos);
		return;
	}

	for (int i = 0; i < CLUSTER_SIZE / sizeof(uint32_t); ++i)
	{
		_file.read(reinterpret_cast<char*>(&subAdress), sizeof(subAdress));

		if (subAdress == 0)
		{
			continue;
		}
		_deleteIndirectLink(rank - 1, subAdress);
		SetDMapAddress(subAdress, MAP_CELL_STATE::FREE);
	}

	_file.seekg(originalPos);
}

void FSManager::SetIMapAddress(uint32_t address, MAP_CELL_STATE value)
{
	_setMapIndex(_iMapStart, (address - _inodeStart) / INODE_SIZE, value);
}

void FSManager::SetDMapAddress(uint32_t address, MAP_CELL_STATE value)
{
	_setMapIndex(_dMapStart, (address - _dataStart) / CLUSTER_SIZE, value);
}

CoroutineGenerator<std::pair<uint32_t, std::array<uint8_t, FSManager::CLUSTER_SIZE>>> FSManager::_getInodeClusters(uint32_t inodeAddress)
{
	std::streampos originalPos = _file.tellg();


	uint32_t linkAddress;

	for (uint32_t i = 0; i < DIRECT_LINK_COUNT; ++i)
	{
		_file.seekg(inodeAddress + INODE_LINK_OFFSET + (i*sizeof(uint32_t)), std::ios::beg);

		_file.read(reinterpret_cast<char*>(&linkAddress), sizeof(linkAddress));

		if (linkAddress == 0)
		{
			_file.seekg(originalPos);
			co_return;
		}

		co_yield {linkAddress,_readCluster(linkAddress)};
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_1_COUNT; ++i)
	{
		_file.seekg(inodeAddress + INODE_LINK_OFFSET + (DIRECT_LINK_COUNT * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.read(reinterpret_cast<char*>(&linkAddress), sizeof(linkAddress));

		if (linkAddress == 0)
		{
			_file.seekg(originalPos);
			co_return;
		}

		for (auto [clAddr, cluster] : _getInodeIndirectClusters(1, linkAddress))
		{
			co_yield {clAddr, cluster};
		}
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_2_COUNT; ++i)
	{
		_file.seekg(inodeAddress + INODE_LINK_OFFSET + ((DIRECT_LINK_COUNT + INDIRECT_LINK_1_COUNT) * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.read(reinterpret_cast<char*>(&linkAddress), sizeof(linkAddress));

		if (linkAddress == 0)
		{
			_file.seekg(originalPos);
			co_return;
		}

		for (auto [clAddr, cluster] : _getInodeIndirectClusters(2, linkAddress))
		{
			co_yield {clAddr, cluster};
		}
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_3_COUNT; ++i)
	{
		_file.seekg(inodeAddress + INODE_LINK_OFFSET + ((DIRECT_LINK_COUNT + INDIRECT_LINK_1_COUNT + INDIRECT_LINK_3_COUNT) * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.read(reinterpret_cast<char*>(&linkAddress), sizeof(linkAddress));

		if (linkAddress == 0)
		{
			_file.seekg(originalPos);
			co_return;
		}

		for (auto [clAddr, cluster] : _getInodeIndirectClusters(3, linkAddress))
		{
			co_yield {clAddr, cluster};
		}
	}

	_file.seekg(originalPos);
}

CoroutineGenerator<std::pair<uint32_t, std::array<uint8_t, FSManager::CLUSTER_SIZE>>> FSManager::_getInodeIndirectClusters(uint8_t rank, uint32_t clusterAddress)
{
	std::streampos originalPos = _file.tellg();

	if (rank == 1)
	{
		for (int i = 0; i < CLUSTER_SIZE / sizeof(uint32_t); ++i)
		{
			_file.seekg(clusterAddress + (i * sizeof(uint32_t)), std::ios::beg);
			uint32_t address;
			_file.read(reinterpret_cast<char*>(&address), sizeof(address));

			if (address == 0)
			{
				co_return;
			}

			co_yield {address,_readCluster(address)};
		}

		_file.seekg(originalPos);
		co_return;
	}

	for (int i = 0; i < CLUSTER_SIZE / sizeof(uint32_t); ++i)
	{
		_file.seekg(clusterAddress + (i * sizeof(uint32_t)), std::ios::beg);
		uint32_t address;
		_file.read(reinterpret_cast<char*>(&address), sizeof(address));

		if (address == 0)
		{
			_file.seekg(originalPos);
			co_return;
		}

		for (auto [subAddress, cluster] : _getInodeIndirectClusters(rank - 1, address))
		{
			co_yield {subAddress, cluster};
		}
	}

	_file.seekg(originalPos);
}

std::vector<std::string> FSManager::GetCurrentPath()
{
	uint32_t address = _actualNodeAddress;

	std::vector<std::string> folders;
	std::streampos originalPos = _file.tellg();

	while (address != _inodeStart)
	{
		_file.seekg(address + sizeof(uint64_t) + sizeof(uint8_t), std::ios::beg);

		uint32_t parentAddress;

		_file.read(reinterpret_cast<char*>(&parentAddress), sizeof(parentAddress));

		for (auto& [_, cluster] : _getInodeClusters(parentAddress))
		{
			bool found = false;

			for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
			{
				std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
				if (filename[0] == '\0')
				{
					continue;
				}

				uint32_t tempAddress = std::bit_cast<uint32_t>(*(reinterpret_cast<const uint8_t(*)[ADDRESS_LENGTH]>(&cluster[i + MAX_FILENAME_LENGTH])));

				if (tempAddress == address)
				{
					_file.seekg(originalPos);
					folders.emplace(folders.begin(), filename);
					found = true;
					break;
				}
			}

			if (found)
			{
				break;
			}
		}

		address = parentAddress;
	}

	return folders;
}

RETURN_CODES FSManager::OpenFolder(std::string folderPath)
{
	std::filesystem::path realFolderPath = folderPath;

	std::streampos originalPos = _file.tellg();
	uint32_t targetAddress = _actualNodeAddress;

	for (const auto& folder : realFolderPath)
	{
		if (folder.string() == "/")
		{
			targetAddress = _inodeStart;
			continue;
		}

		if (folder.string() == ".")
		{
			continue;
		}

		if (folder.string() == "..")
		{
			uint32_t address;
			_file.seekg(targetAddress + sizeof(uint64_t) + sizeof(uint8_t), std::ios::beg);
			_file.read(reinterpret_cast<char*>(&targetAddress), sizeof(targetAddress));
			continue;
		}

		std::string folderName = folder.string();
		folderName.resize(MAX_FILENAME_LENGTH, '\0');
		folderName[MAX_FILENAME_LENGTH - 1] = '\0';

		bool found = false;
		for (auto& [_, cluster] : _getInodeClusters(targetAddress))
		{
			for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
			{
				std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
				if (filename[0] == '\0')
				{
					continue;
				}

				if (filename == folderName)
				{
					targetAddress = std::bit_cast<uint32_t>(*(reinterpret_cast<const uint8_t(*)[ADDRESS_LENGTH]>(&cluster[i + MAX_FILENAME_LENGTH])));
					if (GetInodeType(targetAddress) == INODE_TYPE::FILE)
					{
						_file.seekg(originalPos);
						return RETURN_CODES::NOT_FOLDER;
					}

					found = true;
					break;
				}
			}

			if (found)
			{
				break;
			}
		}

		if (!found)
		{
			_file.seekg(originalPos);
			return RETURN_CODES::PATH_NOT_FOUND;
		}
	}
	_actualNodeAddress = targetAddress;
	_file.seekg(originalPos);

	return RETURN_CODES::SUCCESS;
}

uint64_t FSManager::GetINodeSize(uint32_t address)
{
	std::streampos originalPos = _file.tellg();
	uint64_t size;
	_file.seekg(address, std::ios::beg);
	_file.read(reinterpret_cast<char*>(&size), sizeof(size));
	_file.seekg(originalPos);
	return size;
}

uint32_t FSManager::GetINodeIndex(uint32_t address)
{
	return (address - _inodeStart) / INODE_SIZE;
}

std::string FSManager::GetINodeName(uint32_t address)
{
	if (address == _inodeStart)
	{
		return "/";
	}

	std::streampos originalPos = _file.tellg();

	_file.seekg(address + sizeof(uint64_t) + sizeof(uint8_t), std::ios::beg);

	uint32_t parentAddress;

	_file.read(reinterpret_cast<char*>(&parentAddress), sizeof(parentAddress));

	for (auto& [_, cluster] : _getInodeClusters(parentAddress))
	{
		for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
		{
			std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
			if (filename[0] == '\0')
			{
				continue;
			}

			uint32_t tempAddress = std::bit_cast<uint32_t>(*(reinterpret_cast<const uint8_t(*)[ADDRESS_LENGTH]>(&cluster[i + MAX_FILENAME_LENGTH])));

			if (tempAddress == address)
			{
				_file.seekg(originalPos);
				return filename;
			}
		}
	}
}

std::vector<uint64_t> FSManager::GetStats()
{
	uint64_t usedInodeCount = 0;
	uint64_t folderCount = 0;
	uint64_t usedClusterCount = 0;

	for (uint32_t i = 0; i < _inodeCount; ++i)
	{
		if (GetIMapValue(i) == MAP_CELL_STATE::FREE)
		{
			continue;
		}

		++usedInodeCount;

		if (GetInodeType(GetIMapAddress(i)) == INODE_TYPE::FOLDER)
		{
			++folderCount;
		}
	}

	for (uint32_t i = 0; i < _dataCount; ++i)
	{
		if (GetDMapValue(i) == MAP_CELL_STATE::FREE)
		{
			continue;
		}

		++usedClusterCount;
	}

	return {_size, usedInodeCount, _inodeCount - usedInodeCount, folderCount, usedClusterCount, _dataCount - usedClusterCount};
}

uint32_t FSManager::GetIMapAddress(uint32_t index)
{
	return _getMapAddress(_inodeStart, index, INODE_SIZE);
}

uint32_t FSManager::GetDMapAddress(uint32_t index)
{
	return _getMapAddress(_dataStart, index, CLUSTER_SIZE);
}

uint32_t FSManager::_getMapAddress(uint32_t mapStart, uint32_t index, uint32_t structureSize)
{
	return mapStart + (index * structureSize);
}

RETURN_CODES FSManager::CreateFolder(std::string path, std::string name)
{
	auto emptyGenerator = [] () -> CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>
		{
			std::array<uint8_t, CLUSTER_SIZE> emptyCluster {0};
			co_yield emptyCluster;
		}();

	uint32_t testAddress = GetInodeFromPath((path == "" ? "" : path + "/") + name);

	if (testAddress != 0)
	{
		return RETURN_CODES::EXIST;
	}

	uint32_t parentAddress = GetInodeFromPath(path);

	if (parentAddress == 0)
	{
		return RETURN_CODES::PATH_NOT_FOUND;
	}

	uint32_t newAddress = CreateINode(CLUSTER_SIZE, INODE_TYPE::FOLDER, parentAddress, emptyGenerator);
	_addToFolder(parentAddress, name, newAddress);

	return RETURN_CODES::SUCCESS;
}

void FSManager::FileFromPC(std::filesystem::path sourcePath, std::filesystem::path targetPath)
{
	uint64_t size = std::filesystem::file_size(sourcePath);
	std::ifstream sourceFile(sourcePath, std::ios::binary);

	auto fileGenerator = [&sourceFile] () -> CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>
		{
			sourceFile.seekg(0, std::ios::beg);
			std::array<uint8_t, CLUSTER_SIZE> cluster;
			while (true)
			{
				sourceFile.read(reinterpret_cast<char*>(cluster.data()), CLUSTER_SIZE);

				if (sourceFile.gcount() == 0)
				{
					co_return;
				}

				if (sourceFile.gcount() < CLUSTER_SIZE)
				{
					std::fill(cluster.begin() + sourceFile.gcount(), cluster.end(), 0);
				}

				co_yield cluster;
			}
		}();

	uint32_t parentAddress = GetInodeFromPath(targetPath.parent_path().string());
	uint32_t newAddress = CreateINode(size, INODE_TYPE::FILE, parentAddress, fileGenerator);
	_addToFolder(parentAddress, targetPath.filename().string(), newAddress);
}

void FSManager::PrintFile(uint32_t address)
{
	uint64_t size = GetINodeSize(address);

	for (auto& [_, cluster] : _getInodeClusters(address))
	{
		if (size < CLUSTER_SIZE)
		{
			std::string_view sv(reinterpret_cast<const char*>(cluster.data()), size);
			std::cout << sv << std::endl;
			return;
		}

		size -= CLUSTER_SIZE;
		std::string_view sv(reinterpret_cast<const char*>(cluster.data()), CLUSTER_SIZE);
		std::cout << sv;
	}
}

RETURN_CODES FSManager::DeleteFolder(std::string path)
{
	uint32_t address = GetInodeFromPath(path);

	if (GetInodeType(address) == INODE_TYPE::FILE)
	{
		return RETURN_CODES::NOT_FOLDER;
	}

	for (auto& [clusterAddress, cluster] : _getInodeClusters(address))
	{
		for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
		{
			std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
			if (filename[0] == '\0')
			{
				continue;
			}

			return RETURN_CODES::NOT_EMPTY;
		}
	}

	std::filesystem::path dirPath = path;

	std::string name = dirPath.filename().string();

	_deleteINode(address, name);

	return RETURN_CODES::SUCCESS;
}

void FSManager::DeleteFile(uint32_t address, std::string name)
{
	_deleteINode(address, name);
}

void FSManager::MoveFile(uint32_t address, std::filesystem::path targetPath)
{
	_file.seekg(address + sizeof(uint64_t) + sizeof(uint8_t));

	uint32_t parentAddress;

	_file.read(reinterpret_cast<char*>(&parentAddress), sizeof(parentAddress));

	for (auto& [clusterAddress, cluster] : _getInodeClusters(parentAddress))
	{
		bool rewrited = false;

		for (uint32_t i = 0; i < CLUSTER_SIZE; i += (MAX_FILENAME_LENGTH + ADDRESS_LENGTH))
		{
			std::string filename(reinterpret_cast<const char*>(&cluster[i]), MAX_FILENAME_LENGTH);
			if (filename[0] == '\0')
			{
				continue;
			}

			uint32_t tempAddress = std::bit_cast<uint32_t>(*(reinterpret_cast<const uint8_t(*)[ADDRESS_LENGTH]>(&cluster[i + MAX_FILENAME_LENGTH])));

			if (tempAddress == address)
			{
				std::array<uint8_t, CLUSTER_SIZE> newCluster = cluster;
				std::fill(newCluster.begin() + i, newCluster.begin() + i + MAX_FILENAME_LENGTH + sizeof(uint32_t), 0);
				_rewriteCluster(clusterAddress, newCluster);
				rewrited = true;
				break;
			}

		}

		if (rewrited)
		{
			break;
		}
	}

	_addToFolder(GetInodeFromPath(targetPath.parent_path().string()), targetPath.filename().string(), address);
}

void FSManager::CopyFile(uint32_t address, std::filesystem::path targetPath)
{
	uint64_t size = GetINodeSize(address);

	auto clusters = _getInodeClusters(address);

	auto adapter = [] (auto& source) -> CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>
		{
			while (source.next())
			{
				co_yield source.value().second;
			}
		}(clusters);

	uint32_t parentAddress = GetInodeFromPath(targetPath.parent_path().string());
	uint32_t newAddress = CreateINode(size, INODE_TYPE::FILE, parentAddress, adapter);
	_addToFolder(parentAddress, targetPath.filename().string(), newAddress);
}

void FSManager::FileToPC(uint32_t address, std::ofstream& oFile)
{
	uint64_t size = GetINodeSize(address);

	for (auto& [_, cluster] : _getInodeClusters(address))
	{
		if (size < CLUSTER_SIZE)
		{
			oFile.write(reinterpret_cast<const char*>(cluster.data()), size);
			return;
		}

		size -= CLUSTER_SIZE;
		oFile.write(reinterpret_cast<const char*>(cluster.data()), CLUSTER_SIZE);
	}
}

uint32_t FSManager::_createIndirectLink(uint8_t rank, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator)
{
	std::array<uint32_t, CLUSTER_SIZE / sizeof(uint32_t)> childClusters = {};

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

void FSManager::_extendINode(uint32_t address, uint64_t bonusSize, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusters)
{
	uint64_t oldSize;
	uint32_t freeAddress = 0;

	std::streampos originalPos = _file.tellg();
	_file.seekg(address);
	_file.read(reinterpret_cast<char*>(&oldSize), sizeof(oldSize));

	uint64_t remainingSize = oldSize;
	uint64_t newSize = oldSize + bonusSize;

	_file.seekg(address);
	_file.write(reinterpret_cast<char*>(&newSize), sizeof(newSize));


	uint32_t clusterToSkip = oldSize / CLUSTER_SIZE;
	uint32_t newAddress;
	uint32_t oldAddress;

	for (uint32_t i = clusterToSkip; i < DIRECT_LINK_COUNT; ++i)
	{
		if (!clusters.next())
		{
			_file.seekg(address + INODE_LINK_OFFSET + (i * sizeof(uint32_t)), std::ios::beg);
			_file.write(reinterpret_cast<char*>(&freeAddress), sizeof(freeAddress));
			continue;
		}

		newAddress = _writeNewCluster(clusters.value());
		_file.seekg(address + INODE_LINK_OFFSET + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	if (clusterToSkip >= DIRECT_LINK_COUNT)
	{
		clusterToSkip -= DIRECT_LINK_COUNT;
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_1_COUNT; ++i)
	{
		_file.seekg(address + INODE_LINK_OFFSET + (DIRECT_LINK_COUNT * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&oldAddress), sizeof(oldAddress));

		newAddress = _extendIndirectLink(1, oldAddress, clusterToSkip, clusters);

		if (newAddress == oldAddress)
		{
			continue;
		}

		_file.seekg(address + INODE_LINK_OFFSET + (DIRECT_LINK_COUNT * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_2_COUNT; ++i)
	{
		_file.seekg(address + INODE_LINK_OFFSET + (DIRECT_LINK_COUNT * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&oldAddress), sizeof(oldAddress));

		newAddress = _extendIndirectLink(2, oldAddress, clusterToSkip, clusters);

		if (newAddress == oldAddress)
		{
			continue;
		}

		_file.seekg(address + INODE_LINK_OFFSET + ((DIRECT_LINK_COUNT + INDIRECT_LINK_1_COUNT) * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	for (uint32_t i = 0; i < INDIRECT_LINK_3_COUNT; ++i)
	{
		_file.seekg(address + INODE_LINK_OFFSET + (DIRECT_LINK_COUNT * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&oldAddress), sizeof(oldAddress));

		newAddress = _extendIndirectLink(3, oldAddress, clusterToSkip, clusters);

		if (newAddress == oldAddress)
		{
			continue;
		}

		_file.seekg(address + INODE_LINK_OFFSET + ((DIRECT_LINK_COUNT + INDIRECT_LINK_1_COUNT + INDIRECT_LINK_2_COUNT) * sizeof(uint32_t)) + (i * sizeof(uint32_t)), std::ios::beg);
		_file.write(reinterpret_cast<char*>(&newAddress), sizeof(newAddress));
	}

	_file.seekg(originalPos);
}

uint32_t FSManager::_extendIndirectLink(uint8_t rank, uint32_t oldAddress, uint32_t& clusterToSkip, CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>& clusterGenerator)
{
	std::array<uint32_t, CLUSTER_SIZE / sizeof(uint32_t)> childClusters = {};
	if (rank == 1)
	{
		if (clusterToSkip >= CLUSTER_SIZE / sizeof(uint32_t))
		{
			clusterToSkip -= CLUSTER_SIZE / sizeof(uint32_t);
			return oldAddress;
		}

		if (clusterToSkip > 0)
		{
			for (int i = 0; i < clusterToSkip; ++i)
			{
				_file.seekg(oldAddress + (i * sizeof(uint32_t)), std::ios::beg);
				uint32_t address;
				_file.read(reinterpret_cast<char*>(&address), sizeof(address));
				childClusters[i] = address;
			}

			clusterToSkip = 0;

			for (uint32_t i = clusterToSkip; i < childClusters.size(); ++i)
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

			_rewriteCluster(oldAddress, std::span<const uint8_t, CLUSTER_SIZE>(
				reinterpret_cast<const uint8_t*>(childClusters.data()),
				CLUSTER_SIZE));
			return oldAddress;
		}

		return _createIndirectLink(1, clusterGenerator);
	}

	for (int i = 0; i < childClusters.size(); ++i)
	{
		if (clusterToSkip > 0)
		{
			_file.seekg(oldAddress + (i * sizeof(uint32_t)), std::ios::beg);
			uint32_t address;
			_file.read(reinterpret_cast<char*>(&address), sizeof(address));

			uint32_t clusterAddress = _extendIndirectLink(rank - 1, address, clusterToSkip, clusterGenerator);
			childClusters[i] = clusterAddress;
			continue;
		}

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

	if (oldAddress == 0)
	{
		return _writeNewCluster(std::span<const uint8_t, CLUSTER_SIZE>(
			reinterpret_cast<const uint8_t*>(childClusters.data()),
			CLUSTER_SIZE));
	}
	else
	{
		_rewriteCluster(oldAddress, std::span<const uint8_t, CLUSTER_SIZE>(
			reinterpret_cast<const uint8_t*>(childClusters.data()),
			CLUSTER_SIZE));
		return oldAddress;
	}

}


void FSManager::ExtendFile(uint32_t sourceAddress, uint32_t targetAddress)
{
	uint64_t oldSize = GetINodeSize(sourceAddress);
	uint64_t targetSize = GetINodeSize(targetAddress);

	auto adapter = [this, oldSize, targetSize, sourceAddress, targetAddress] () -> CoroutineGenerator<std::array<uint8_t, CLUSTER_SIZE>>
		{
			// 1. Získání posledního clusteru ze SOURCE
			auto genS = _getInodeClusters(sourceAddress);
			std::array<uint8_t, CLUSTER_SIZE> lastSourceCluster = {0};
			bool hasSource = false;
			while (genS.next())
			{
				lastSourceCluster = genS.value().second;
				hasSource = true;
			}

			// 2. Pøíprava dat z TARGET
			auto genT = _getInodeClusters(targetAddress);
			uint64_t offsetInLastCluster = oldSize % CLUSTER_SIZE;

			if (offsetInLastCluster == 0)
			{
				// Pokud je source zarovnaný, jen pøeposíláme target
				while (genT.next())
				{
					co_yield genT.value().second;
				}
				co_return;
			}

			// 3. Zpracování prvního clusteru z TARGET
			if (genT.next())
			{
				auto firstTargetCluster = genT.value().second;

				// Doplníme poslední cluster source daty ze zaèátku targetu
				for (size_t i = 0; i < (CLUSTER_SIZE - offsetInLastCluster); ++i)
				{
					lastSourceCluster[offsetInLastCluster + i] = firstTargetCluster[i];
				}
				co_yield lastSourceCluster;

				// Posun (shift) pro všechny další clustery
				std::array<uint8_t, CLUSTER_SIZE> prevCluster = firstTargetCluster;
				size_t shiftSize = CLUSTER_SIZE - offsetInLastCluster;

				while (genT.next())
				{
					auto currentTarget = genT.value().second;
					std::array<uint8_t, CLUSTER_SIZE> combined;

					// Skládáme z konce pøedchozího a zaèátku aktuálního
					for (size_t i = 0; i < offsetInLastCluster; ++i)
						combined[i] = prevCluster[shiftSize + i];
					for (size_t i = 0; i < shiftSize; ++i)
						combined[offsetInLastCluster + i] = currentTarget[i];

					co_yield combined;
					prevCluster = currentTarget;
				}

				if (targetSize % CLUSTER_SIZE <= CLUSTER_SIZE - (oldSize % CLUSTER_SIZE))
				{
					co_return;
				}

				// 4. Poslední zbytek ("ocas") doplnìný nulami
				std::array<uint8_t, CLUSTER_SIZE> finalCluster;
				finalCluster.fill(0);
				for (size_t i = 0; i < offsetInLastCluster; ++i)
				{
					finalCluster[i] = prevCluster[shiftSize + i];
				}
				co_yield finalCluster;
			}
		}();

	_extendINode(sourceAddress, targetSize, adapter);
}