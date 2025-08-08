#pragma once
inline std::string FormatVersion(uint32_t ver) {
	uint16_t major = uint16_t(ver >> 16);
	uint16_t minor = uint16_t(ver & 0xFFFF);
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

inline std::string FormatVec3(const W3dVectorStruct& v) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6)
		<< v.X << " " << v.Y << " " << v.Z;
	return oss.str();
}

inline std::string FormatString(const char* raw, size_t maxLen) {
	size_t len = ::strnlen(raw, maxLen);
	return std::string(raw, len);
}

inline std::string FormatRGBA(const W3dRGBAStruct& c) {
	return "("
		+ std::to_string(c.R) + " "
		+ std::to_string(c.G) + " "
		+ std::to_string(c.B) + " "
		+ std::to_string(c.A) + ")";
}

inline std::string FormatTexCoord(const W3dTexCoordStruct& t) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6) << t.U << ' ' << t.V;
	return oss.str();
}

inline std::string FormatVec3i(const Vector3i& v) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6)
		<< v.I << " " << v.K << " " << v.J;
	return oss.str();
}