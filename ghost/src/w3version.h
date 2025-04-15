#pragma once


#include "stdint.h"
#include <string>

class W3Version {
public:
	uint16_t m_major;
	uint16_t m_minor;
	uint16_t m_patch;
	uint16_t m_build;

	W3Version(uint16_t major, uint16_t minor, uint16_t patch, uint16_t build);
	W3Version();
	~W3Version();

	std::string toString();
};