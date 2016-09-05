#ifndef PCTRIE_HPP
#define PCTRIE_HPP

#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>

#include "table.hpp"

#define PCTRIE_QTREE 0

class PCTrie {
private:

	struct Internal {
		uint32_t base;
		uint32_t parent;
		uint16_t splitPos;
		uint16_t childTypes; // If most sig bit set: only one child (root)
		uint32_t leaf;
		uint32_t left;
		uint32_t right;
	} __attribute__ ((aligned (8)));

	struct Leaf {
		uint32_t base;
		uint32_t parent;
		uint32_t nextHops;
		uint32_t number;
	} __attribute__ ((aligned (8)));

	struct NextHop {
		uint32_t nextHop;
		uint32_t prefixLength;
	} __attribute__ ((aligned (8)));

	struct Internal* internals;
	struct Leaf* leafs;
	struct NextHop* nextHops;

	uint32_t root;

/*
	class Internal;
	class Leaf;
	class Node;

	enum node_type {
		INTERNAL,
		LEAF
	};

	class Node {
	public:
		Internal* parent;
		enum node_type type;
		uint32_t base;

		Node(
			Internal* parent,
			enum node_type type,
			uint32_t base);
	};

	class Internal : public Node {
	public:
		Node* left;
		Node* right;
		Leaf* leaf;
		uint8_t splitPos;

		Internal(
			Internal* parent,
			uint32_t base,
			Internal* left,
	       	Internal* right,
	       	Leaf* leaf,
	       	int splitPos);
	};

	class Leaf : public Node {
	public:
		struct leaf_entry {
			uint32_t next_hop;
			uint32_t prefix_length;

			bool operator < (const leaf_entry& e) const {
				return prefix_length > e.prefix_length;
			}
		};

		std::vector<struct leaf_entry> entries;

		Leaf(Internal* parent, uint32_t base);
		void pushRoute(uint32_t next_hop, uint32_t prefix_length);
		bool hasMoreGeneralRoute(uint32_t prefix_length);
	};

	Node* root;
*/
	Table& table;

	void buildTrie();

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
