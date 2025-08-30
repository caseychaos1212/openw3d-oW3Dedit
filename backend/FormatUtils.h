#pragma once

namespace FormatUtils {

	inline std::string FormatVersion(uint32_t ver) {
		uint16_t major = uint16_t(ver >> 16);
		uint16_t minor = uint16_t(ver & 0xFFFF);
		return std::to_string(major) + "." + std::to_string(minor);
	}

	inline std::string FormatPrimVersion(uint32_t major, uint32_t minor) {

		return std::to_string(major) + "." + std::to_string(minor);
	}

	inline std::string FormatName(const char* name, size_t len) {

		return std::string(name, strnlen(name, len));
	}


	inline std::string FormatFloat(float f) {
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(6) << f;
		return oss.str();
	}

	inline std::string FormatUInt32(uint32_t data) {

		return std::to_string(data);
	}

	inline std::string FormatUInt16(uint16_t data) {

		return std::to_string(data);
	}

	inline std::string FormatVec2(float u, float v) {
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(6) << u << ' ' << v;
		return oss.str();
	}

	inline std::string FormatVec3(float x, float y, float z) {
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(6) << x << ' ' << y << ' ' << z;
		return oss.str();
	}

	inline std::string FormatString(const char* raw, size_t maxLen) {
		size_t len = ::strnlen(raw, maxLen);
		return std::string(raw, len);
	}

	inline std::string FormatRGB(int r, int g, int b) {
		return "(" + std::to_string(r) + " " + std::to_string(g) + " " + std::to_string(b) + ")";
	}

	inline std::string FormatRGBA(int r, int g, int b, int a) {
		return "(" + std::to_string(r) + " " + std::to_string(g) + " "
			+ std::to_string(b) + " " + std::to_string(a) + ")";
	}


	inline std::string FormatTexCoord(float u, float v) {
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(6) << u << ' ' << v;
		return oss.str();
	}

	inline std::string FormatVec3i(int i, int k, int j) {
		std::ostringstream oss;
		oss << i << ' ' << k << ' ' << j;
		return oss.str();
	}

	inline std::string FormatQuat(float x, float y, float z, float w) {
		std::ostringstream oss;
		oss << std::fixed << std::setprecision(6) << x << ' ' << y << ' ' << z << ' ' << w;
		return oss.str();
	}

}