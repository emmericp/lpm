#ifndef PCTRIE_HPP
#define PCTRIE_HPP

#include <vector>

#include "table.hpp"

class PCTrie {
private:

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
		};

		std::vector<struct leaf_entry> entries;

		Leaf(Internal* parent, uint32_t base);
		void push_route(uint32_t next_hop, uint32_t prefix_length);
	};

	Node* root;

	Table& table;

	void buildTrie();

public:
	PCTrie(Table& table);

	uint32_t route(uint32_t);
	//void routeBatch(uint32_t* in, uint32_t* out, int count);
};

#endif /* PCTRIE_HPP */
