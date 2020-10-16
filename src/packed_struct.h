#ifndef _QUADRA_PACKED_STRUCT_H
#define _QUADRA_PACKED_STRUCT_H

#include <iostream>

#ifdef _MSC_VER
	#define packed_struct(name, ...) \
		__pragma(pack(push, 1)) struct name { __VA_ARGS__ } __pragma(pack(pop));
#else
	#define packed_struct(name, ...) \
		struct __attribute__((__packed__)) name { __VA_ARGS__ };
#endif

template <typename T>
T read_packed(std::ifstream& file) {
	T packed;
	file.read((char*) &packed, sizeof(T));
	return packed;
}

template <typename T>
T read_packed(std::ifstream& file, size_t offset) {
	file.seekg(offset);
	return read_packed<T>(file);
}

template <typename T>
std::vector<T> read_multiple_packed(std::ifstream& file, size_t count) {
	std::vector<T> packed(count);
	file.read((char*) packed.data(), sizeof(T) * count);
	return packed;
}

template <typename T>
std::vector<T> read_multiple_packed(std::ifstream& file, size_t offset, size_t count) {
	file.seekg(offset);
	return read_multiple_packed<T>(file, count);
}

#endif
