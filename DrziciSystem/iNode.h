#pragma once

#include <cstdint>


class iNode
{
public:
	iNode();

	static uint32_t SizeOf();

private:
	uint64_t _size;

	uint8_t _type;

	uint32_t _directLinks[12];
	uint32_t _indirectLinks1;
	uint32_t _indirectLinks2;
	uint32_t _indirectLinks3;
};