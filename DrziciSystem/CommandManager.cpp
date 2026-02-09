#include "CommandManager.h"

#include <iostream>
#include <ranges>
#include <string_view>
#include <filesystem>

CommandManager::CommandManager(FSManager& fsmanager) : _fsmanager(fsmanager)
{

}

void CommandManager::MainLoop()
{
	while (true)
	{
		std::string command;

		std::getline(std::cin, command);

		std::vector<std::string> commandParts;

		for (auto&& part : command | std::views::split(' '))
		{
			commandParts.emplace_back(part.begin(), part.end());
		}

		if (!_fsmanager.Inited)
		{
			if (commandParts.size() == 0)
			{
				continue;
			}
			else if (commandParts[0] == "exit")
			{
				return;
			}
			else if (commandParts[0] == "format")
			{
				_format(commandParts[1]);
			}
		}

		if (commandParts.size() == 0)
		{
			continue;
		}
		else if (commandParts[0] == "exit")
		{
			return;
		}
		else if (commandParts[0] == "mkdir")
		{
			_makeDir(commandParts[1]);
		}
		else if (commandParts[0] == "ls")
		{
			_list(commandParts.size() == 1 ? "" : commandParts[1]);
		}
		else if (commandParts[0] == "rmdir")
		{
			_removeDir(commandParts[1]);
		}
		else if (commandParts[0] == "pwd")
		{
			_path();
		}
		else if (commandParts[0] == "cd")
		{
			_cd(commandParts[1]);
		}
		else if (commandParts[0] == "info")
		{
			_info(commandParts[1]);
		}
		else if (commandParts[0] == "statfs")
		{
			_fSStats();
		}
		else if (commandParts[0] == "format")
		{
			_format(commandParts[1]);
		}
		else if (commandParts[0] == "incp")
		{
			_copyFromPC(commandParts[1], commandParts[2]);
		}
		else if (commandParts[0] == "cat")
		{
			_cat(commandParts[1]);
		}
		else if (commandParts[0] == "rm")
		{
			_removeFile(commandParts[1]);
		}
		else if (commandParts[0] == "mv")
		{
			_move(commandParts[1], commandParts[2]);
		}
		else if (commandParts[0] == "cp")
		{
			_copy(commandParts[1], commandParts[2]);
		}
		else if (commandParts[0] == "outcp")
		{
			_copyToPC(commandParts[1], commandParts[2]);
		}
		else if (commandParts[0] == "load")
		{
			_runScript(commandParts[1]);
		}
		else if (commandParts[0] == "add")
		{
			_appendFile(commandParts[1], commandParts[2]);
		}
	}
}
	
void CommandManager::_makeDir(std::string newDirPath)
{
	std::filesystem::path dirPath = newDirPath;

	RETURN_CODES rc = _fsmanager.CreateFolder(dirPath.parent_path().string(), dirPath.filename().string());

	switch (rc)
	{
		case RETURN_CODES::SUCCESS:
			std::cout << "OK" << std::endl;
			return;
		case RETURN_CODES::PATH_NOT_FOUND:
			std::cout << "PATH NOT FOUND (neexistuje zadana cesta)" << std::endl;
			return;
		case RETURN_CODES::EXIST:
			std::cout << "EXIST (nelze zalozit, jiz existuje)" << std::endl;
			return;
		default:
			std::cout << "Bad return code" << std::endl;
			return;
	}
}

void CommandManager::_list(std::string path)
{
	uint32_t address = _fsmanager.GetInodeFromPath(path);

	if (address == 0)
	{
		std::cout << "PATH NOT FOUND (neexistuje zadana cesta)" << std::endl;
		return;
	}

	if (_fsmanager.GetInodeType(address) == INODE_TYPE::FILE)
	{
		std::cout << "PATH NOT FOUND (zadana cesta neni slozka)" << std::endl;
		return;
	}

	for (auto [name, type]: _fsmanager.GetContent(address))
	{
		std::cout << name << "\t" << (type == INODE_TYPE::FOLDER ? "Folder" : "File") << std::endl;
	}
}

void CommandManager::_removeDir(std::string path)
{
	RETURN_CODES rc = _fsmanager.DeleteFolder(path);

	switch (rc)
	{
		case RETURN_CODES::SUCCESS:
			std::cout << "OK" << std::endl;
			break;
		case RETURN_CODES::PATH_NOT_FOUND:
			std::cout << "PATH NOT FOUND (neexistuje zadana cesta)" << std::endl;
			break;
		case RETURN_CODES::NOT_FOLDER:
			std::cout << "NOT FOLDER (zadana cesta neni slozka)" << std::endl;
			break;
		case RETURN_CODES::NOT_EMPTY:
			std::cout << "NOT EMPTY (adresar obsahuje podadresare, nebo soubory)" << std::endl;
			break;
		default:
			std::cout << "Bad return code" << std::endl;
			break;
	}
}

void CommandManager::_path()
{
	std::vector<std::string> curPath = _fsmanager.GetCurrentPath();

	for (std::string folder : curPath)
	{
		std::cout << "/" << folder;
	}

	std::cout << std::endl;
}

void CommandManager::_cd(std::string path)
{
	RETURN_CODES rc = _fsmanager.OpenFolder(path);

	switch (rc)
	{
		case RETURN_CODES::SUCCESS:
			std::cout << "OK" << std::endl;
			break;
		case RETURN_CODES::PATH_NOT_FOUND:
			std::cout << "PATH NOT FOUND (neexistuje zadana cesta)" << std::endl;
			break;
		case RETURN_CODES::NOT_FOLDER:
			std::cout << "NOT FOLDER (zadana cesta neni slozka)" << std::endl;
			break;
		default:
			std::cout << "Bad return code" << std::endl;
			break;
	}
}

void CommandManager::_info(std::string path)
{
	uint32_t address = _fsmanager.GetInodeFromPath(path);

	if (address == 0)
	{
		std::cout << "PATH NOT FOUND (neexistuje zadana cesta)" << std::endl;
		return;
	}

	std::cout << _fsmanager.GetINodeName(address) << "\t-\t" << _fsmanager.GetINodeSize(address) << " B\t-\ti-uzel " << _fsmanager.GetINodeIndex(address) << std::endl;
}

void CommandManager::_fSStats()
{
	std::vector<uint64_t> info = _fsmanager.GetStats();

	std::cout << "T.Space:\t" << std::to_string(info[0]) << std::endl;
	std::cout << "Used IN:\t" << std::to_string(info[1]) << std::endl;
	std::cout << "Free IN:\t" << std::to_string(info[2]) << std::endl;
	std::cout << "Folders:\t" << std::to_string(info[3]) << std::endl;
	std::cout << "Used Cl:\t" << std::to_string(info[4]) << std::endl;
	std::cout << "Free Cl:\t" << std::to_string(info[5]) << std::endl;
}

void CommandManager::_format(std::string size)
{
	size_t i = 0;
	while (i < size.length() && std::isdigit(size[i]))
	{
		i++;
	}

	uint64_t value = std::stoull(size.substr(0, i));
	std::string unit = size.substr(i);

	for (char& c : unit)
	{
		c = std::tolower(static_cast<unsigned char>(c));
	}

	if (unit == "kb")
	{
		value *= 1000;
	}
	else if (unit == "mb")
	{
		value *= 1000 * 1000;
	}
	else if (unit == "gb")
	{
		value *= 1000 * 1000 * 1000;
	}

	_fsmanager.Format(value);
	std::cout << "OK" << std::endl;
}

void CommandManager::_copyFromPC(std::string sourcePath, std::string targetPath)
{
	std::filesystem::path realSourcePath = sourcePath;
	std::filesystem::path realTargetPath = targetPath;

	if (!std::filesystem::is_regular_file(realSourcePath))
	{
		std::cout << "FILE NOT FOUND (neni zdroj)" << std::endl;
		return;
	}

	uint32_t parentAddress = _fsmanager.GetInodeFromPath(realTargetPath.parent_path().string());
	uint32_t potentialAddress = _fsmanager.GetInodeFromPath(targetPath);

	if (parentAddress == 0 || _fsmanager.GetInodeType(parentAddress) == INODE_TYPE::FILE || potentialAddress != 0)
	{
		std::cout << "PATH NOT FOUND (neexistuje cilova cesta)" << std::endl;
		return;
	}

	_fsmanager.FileFromPC(realSourcePath, realTargetPath);
	std::cout << "OK" << std::endl;
}

void CommandManager::_cat(std::string path)
{
	uint32_t address = _fsmanager.GetInodeFromPath(path);
	if (address == 0 || _fsmanager.GetInodeType(address) == INODE_TYPE::FOLDER)
	{
		std::cout << "PATH NOT FOUND (neexistuje cilova cesta)" << std::endl;
		return;
	}

	_fsmanager.PrintFile(address);
}

void CommandManager::_removeFile(std::string path)
{
	uint32_t address = _fsmanager.GetInodeFromPath(path);
	if (address == 0 || _fsmanager.GetInodeType(address) == INODE_TYPE::FOLDER)
	{
		std::cout << "PATH NOT FOUND (neexistuje cilova cesta)" << std::endl;
		return;
	}

	std::filesystem::path realPath = path;

	_fsmanager.DeleteFile(address, realPath.filename().string());
	std::cout << "OK" << std::endl;
}

void CommandManager::_move(std::string sourcePath, std::string targetPath)
{
	uint32_t sourceAddress = _fsmanager.GetInodeFromPath(sourcePath);

	if (sourceAddress == 0 || _fsmanager.GetInodeType(sourceAddress) == INODE_TYPE::FOLDER)
	{
		std::cout << "FILE NOT FOUND (neni zdroj)" << std::endl;
		return;
	}

	std::filesystem::path trueTargetPath = targetPath;
	uint32_t targetFolderAddress = _fsmanager.GetInodeFromPath(trueTargetPath.parent_path().string());
	uint32_t targetAddress = _fsmanager.GetInodeFromPath(targetPath);

	if (targetFolderAddress == 0 || _fsmanager.GetInodeType(targetFolderAddress) == INODE_TYPE::FILE || targetAddress != 0)
	{
		std::cout << "PATH NOT FOUND (neexistuje cilova cesta)" << std::endl;
		return;
	}

	_fsmanager.MoveFile(sourceAddress, trueTargetPath);
	std::cout << "OK" << std::endl;
}

void CommandManager::_copy(std::string sourcePath, std::string targetPath)
{
	uint32_t sourceAddress = _fsmanager.GetInodeFromPath(sourcePath);

	if (sourceAddress == 0 || _fsmanager.GetInodeType(sourceAddress) == INODE_TYPE::FOLDER)
	{
		std::cout << "FILE NOT FOUND (neni zdroj)" << std::endl;
		return;
	}

	std::filesystem::path trueTargetPath = targetPath;
	uint32_t targetFolderAddress = _fsmanager.GetInodeFromPath(trueTargetPath.parent_path().string());
	uint32_t targetAddress = _fsmanager.GetInodeFromPath(targetPath);

	if (targetFolderAddress == 0 || _fsmanager.GetInodeType(targetFolderAddress) == INODE_TYPE::FILE || targetAddress != 0)
	{
		std::cout << "PATH NOT FOUND (neexistuje cilova cesta)" << std::endl;
		return;
	}

	_fsmanager.CopyFile(sourceAddress, trueTargetPath);
	std::cout << "OK" << std::endl;
}


void CommandManager::_copyToPC(std::string sourcePath, std::string targetPath)
{
	uint32_t address = _fsmanager.GetInodeFromPath(sourcePath);

	if (address == 0 || _fsmanager.GetInodeType(address) == INODE_TYPE::FOLDER)
	{
		std::cout << "FILE NOT FOUND (neni zdroj)" << std::endl;
		return;
	}

	std::ofstream file(targetPath, std::ios::binary);
	if (!file.is_open())
	{
		std::cout << "PATH NOT FOUND (neexistuje cilova cesta)" << std::endl;
		return;
	}


	_fsmanager.FileToPC(address, file);
	std::cout << "OK" << std::endl;
}

void CommandManager::_appendFile(std::string sourceFile, std::string targetFile)
{
	uint32_t sourceAddress = _fsmanager.GetInodeFromPath(sourceFile);

	if (sourceAddress == 0 || _fsmanager.GetInodeType(sourceAddress) == INODE_TYPE::FOLDER)
	{
		std::cout << "FILE NOT FOUND (neni zdroj)" << std::endl;
		return;
	}

	uint32_t targetAddress = _fsmanager.GetInodeFromPath(targetFile);

	if (targetAddress == 0 || _fsmanager.GetInodeType(targetAddress) == INODE_TYPE::FOLDER)
	{
		std::cout << "FILE NOT FOUND (neni zdroj)" << std::endl;
		return;
	}

	_fsmanager.ExtendFile(sourceAddress, targetAddress);
	std::cout << "OK" << std::endl;
}

void CommandManager::_runScript(std::string path)
{
	std::string command;

	std::ifstream script(path);

	if (!script.is_open())
	{
		std::cerr << "FILE NOT FOUND (neni zdroj)" << std::endl;
		return;
	}

	while (std::getline(script, command))
	{
		std::cout << command << std::endl;
		std::vector<std::string> commandParts;

		for (auto&& part : command | std::views::split(' '))
		{
			commandParts.emplace_back(part.begin(), part.end());
		}

		if (!_fsmanager.Inited)
		{
			if (commandParts.size() == 0)
			{
				continue;
			}
			else if (commandParts[0] == "exit")
			{
				return;
			}
			else if (commandParts[0] == "format")
			{
				_format(commandParts[1]);
			}
		}

		if (commandParts.size() == 0)
		{
			continue;
		}
		else if (commandParts[0] == "exit")
		{
			return;
		}
		else if (commandParts[0] == "mkdir")
		{
			_makeDir(commandParts[1]);
		}
		else if (commandParts[0] == "ls")
		{
			_list(commandParts.size() == 1 ? "" : commandParts[1]);
		}
		else if (commandParts[0] == "rmdir")
		{
			_removeDir(commandParts[1]);
		}
		else if (commandParts[0] == "pwd")
		{
			_path();
		}
		else if (commandParts[0] == "cd")
		{
			_cd(commandParts[1]);
		}
		else if (commandParts[0] == "info")
		{
			_info(commandParts[1]);
		}
		else if (commandParts[0] == "statfs")
		{
			_fSStats();
		}
		else if (commandParts[0] == "format")
		{
			_format(commandParts[1]);
		}
		else if (commandParts[0] == "incp")
		{
			_copyFromPC(commandParts[1], commandParts[2]);
		}
		else if (commandParts[0] == "cat")
		{
			_cat(commandParts[1]);
		}
		else if (commandParts[0] == "rm")
		{
			_removeFile(commandParts[1]);
		}
		else if (commandParts[0] == "mv")
		{
			_move(commandParts[1], commandParts[2]);
		}
		else if (commandParts[0] == "cp")
		{
			_copy(commandParts[1], commandParts[2]);
		}
		else if (commandParts[0] == "outcp")
		{
			_copyToPC(commandParts[1], commandParts[2]);
		}
		else if (commandParts[0] == "load")
		{
			_runScript(commandParts[1]);
		}
	}
}