#pragma once

#include "FSManager.h"

class CommandManager
{
public:
	CommandManager(FSManager& fsmanager);

	void MainLoop();
private:
	FSManager& _fsmanager;

	void _copy(std::string s1, std::string s2);
	void _move(std::string s1, std::string s2);
	void _removeFile(std::string s);
	void _makeDir(std::string s);
	void _removeDir(std::string s);
	void _list(std::string s);
	void _cat(std::string s);
	void _cd(std::string s);
	void _path();
	void _info(std::string s);
	void _copyFromPC(std::string s1, std::string s2);
	void _copyToPC(std::string s1, std::string s2);
	void _runScript(std::string s);
	void _format(std::string s);
	void _fSStats();
	void _joinFiles(std::string s1, std::string s2, std::string s3);
	void _appendFile(std::string s1, std::string s2);
};