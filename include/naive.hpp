#ifndef __NAIVE_HPP__
#define __NAIVE_HPP__

#include <vector>
#include <map>

#include "table.hpp"

class Naive {
private:
	const vector<map<uint32_t, uint32_t>>& entries;
	uint32_t masks[33];

public:
	Naive(Table& table);

	uint32_t route(uint32_t);
	int route_challenge(list<pair<uint32_t, uint32_t>>& challenge);
};

#endif /*__NAIVE_HPP__*/
