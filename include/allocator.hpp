#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include <cstddef>
#include <iostream>

#include <stdint.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "util.hpp"

template<typename T>
class Allocator {
private:
	T* array;
	size_t size;
	uint64_t* maps;

public:
	Allocator(size_t size_wish) {
		size = size_wish | 0x3f;
		array = static_cast<T*>(
				mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
		if(array == MAP_FAILED){
			std::cerr << "Allocator::Allocator map failed" << std::endl;
			std::cerr << "errno: " << strerror(errno) << std::endl;
			std::abort();
		}

		maps = static_cast<uint64_t*>(malloc((size/64) * sizeof(uint64_t)));
		if(maps == MAP_FAILED){
			std::cerr << "Allocator::Allocator map failed" << std::endl;
			std::cerr << "errno: " << strerror(errno) << std::endl;
			std::abort();
		}
	};

	~Allocator() {
		munmap(array, size);
		free(maps);
	};

	T* getPointer() {
		return array;
	};

	size_t insert(T& obj) {
		size_t cur = 0;
		while(likely(cur < (size/64) && maps[cur] == 0xffffffffffffffff)) {
			cur++;
		}

		if(unlikely(cur >= (size/64))){
			std::cerr << "Allocator::insert size is not sufficient" << std::endl;
			std::abort();
		}

		int pos = mostSigOne(~maps[cur]);
		memcpy(array + (cur*64) + pos, &obj, sizeof(T));
		maps[cur] |= 1 << (63-pos);

		return (cur*64) + pos;
	};

	size_t insert(T* obj, int num) {
		size_t cur = 0;
		int pos = 0;
		uint64_t mask = 0;

		while(likely(cur < (size/64))) {
			mask = PREFIX_MASK_64(num);
			pos = 0;
			while(__builtin_popcountll(mask) == num){
				if(__builtin_popcountll(~maps[cur] & mask) == num){
					goto found_map;
				}
				mask = mask >> 1;
				pos++;
			}
			cur++;
		}

found_map:

		if(unlikely(cur >= (size/64))){
			std::cerr << "Allocator::insert size is not sufficient"
				<< ", or the memory is too framented" << std::endl;
			std::abort();
		}

		memcpy(array + (cur*64) + pos, obj, sizeof(T) * num);
		maps[cur] |= mask;

		return (cur*64) + pos;

	};

	void erase(size_t idx, int num = 1) {
		int cur = idx >> idx;
		int pos = idx & 0x3f;

		uint64_t mask = ~(PREFIX_MASK_64(num) >> (63 - pos));
		maps[cur] &= mask;
	};
};

#endif /* ALLOCATOR_HPP */
