#include "w3version.h"
#include "util.h"

W3Version::W3Version(uint16_t major, uint16_t minor, uint16_t patch, uint16_t build) {
	m_major = major;
	m_minor = minor;
	m_patch = patch;
	m_build = build;
}

W3Version::W3Version() {

}

W3Version::~W3Version() {

}

std::string W3Version::toString() {
	return UTIL_ToString(m_major) + "." + UTIL_ToString(m_minor) + "." + UTIL_ToString(m_patch) + "." + UTIL_ToString(m_build);
}