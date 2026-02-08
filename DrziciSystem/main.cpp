#include "FSManager.h"

#include <iostream>
#include <fstream>

#include <string>

int main(int argc, char *argv[])
{
	std::cout << "xddd" << std::endl;
	FSManager fsm("xd.txt");
	fsm.Format(6000000);
	fsm.CreateFolder("xd");
	fsm.CreateFolder("nnnn");
	for (auto a : fsm.GetContent())
	{
		std::cout << a.first << "\t" << std::to_string(static_cast<uint8_t>(a.second)) << std::endl;
	}
	return 5;
}