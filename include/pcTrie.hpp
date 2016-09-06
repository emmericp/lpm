#ifndef PCTRIE_HPP
#define PCTRIE_HPP

#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>

#include "table.hpp"
#include "allocator.hpp"

#define PCTRIE_QTREE 0

#define IS_LEFT_INTERNAL(internal) (internal->childTypes & 1)
#define SET_LEFT_INTERNAL(internal) (internal->childTypes |= 1)
#define IS_RIGHT_INTERNAL(internal) ((internal->childTypes & 2) >> 1)
#define SET_RIGHT_INTERNAL(internal) (internal->childTypes |= 2)

class PCTrie {
private:

	struct Internal;
	struct Leaf;
	struct NextHop;

	struct Internal {
		uint32_t base;
		uint32_t parent;
		uint16_t splitPos;
		uint16_t childTypes;
		uint32_t leaf;
		uint32_t left;
		uint32_t right;
	};

	struct Leaf {
		uint32_t base;
		uint32_t parent;
		uint32_t nextHops;
		uint32_t number;

		void pushRoute(
				struct PCTrie::NextHop e,
				struct PCTrie::NextHop* nextHops,
				Allocator<struct NextHop>& nextHops_alloc);

		bool hasMoreGeneralRoute(int len);
	};

	struct NextHop {
		uint32_t nextHop;
		uint32_t prefixLength;

		bool operator < (const struct NextHop& e) const {
			return prefixLength > e.prefixLength;
		}
	};

	// Useful traversal helper class
	class Node {
	public:
		bool is_internal;
		uint32_t pos;
		struct Internal* internals;
		struct Leaf* leafs;

		Node(uint32_t pos, bool is_internal, struct Internal* internals, struct Leaf* leafs) :
			is_internal(is_internal), pos(pos), internals(internals), leafs(leafs) {};

		uint32_t base(){
			return is_internal ? internals[pos].base : leafs[pos].base;
		};
		uint32_t parent(){
			return is_internal ? internals[pos].base : leafs[pos].base;
		};
		void goUp(){
			pos = is_internal ? internals[pos].parent : leafs[pos].parent;
			is_internal = true;
		}
		void goLeft(){
			if(!IS_LEFT_INTERNAL((&internals[pos])))
					is_internal = false;
			pos = internals[pos].left;
		};
		void goRight(){
			if(!IS_RIGHT_INTERNAL((&internals[pos])))
					is_internal = false;
			pos = internals[pos].right;
		};
	};

	struct Internal* internals;
	struct Leaf* leafs;
	struct NextHop* nextHops;

	Allocator<struct Internal> internals_alloc;
	Allocator<struct Leaf> leafs_alloc;
	Allocator<struct NextHop> nextHops_alloc;

	uint32_t root;

	Table& table;

	void buildTrie();
	enum buildState {
		EMPTY = 0,
		ONE_LEAF = 1,
		NORMAL = 2
	} buildState;

#if PCTRIE_QTREE == 1
	std::stringstream qtree_prev;
	void addQtreeSnapshot();
	std::string getQtreeSnapshot();
	std::string finalizeQtree(std::string tree);
#endif

public:
	PCTrie(Table& table);

#if PCTRIE_QTREE == 1
	std::string getQtree();
	std::string getQtreeHistory();
#endif

	unsigned int getSize();

	uint32_t route(uint32_t);
};

#endif /* PCTRIE_HPP */
