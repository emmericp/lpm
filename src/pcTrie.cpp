#include "pcTrie.hpp"

using namespace std;

PCTrie::Node::Node(
		Internal* parent,
		enum node_type type,
		uint32_t base) :
	parent(parent), type(type), base(base) {};

PCTrie::Internal::Internal(
		Internal* parent,
		uint32_t base,
		Internal* left,
       	Internal* right,
       	Leaf* leaf,
       	int splitPos) :
	Node(parent, INTERNAL, base), left(left), right(right), leaf(leaf), splitPos(splitPos) {};

PCTrie::Leaf::Leaf(Internal* parent, uint32_t base) :
	Node(parent, LEAF, base) {};

void PCTrie::Leaf::push_route(uint32_t next_hop, uint32_t prefix_length) {
	struct leaf_entry entry;
	entry.next_hop = next_hop;
	entry.prefix_length = prefix_length;
	entries.push_back(entry);
};

void PCTrie::buildTrie() {
	// Build trie
	vector<map<uint32_t,uint32_t>> tbl = table.get_sorted_entries();
	for(int len=0; len<=32; len++){
		for(auto& e : tbl[len]){
			// Is the trie empty?
			if(root == NULL){
				// Place leaf as root
				//root = new Leaf(NULL, len, e.first, e.second);
				root = new Leaf(NULL, e.first);

				// Push route into leaf
				static_cast<Leaf*>(root)->push_route(e.second, len);
				continue;
			}

			// Traverse to the first prefix conflict, or until a leaf is found
			Node* cur = root;
			while(cur->type == INTERNAL){
				Internal* cur_int = static_cast<Internal*>(cur);
				uint32_t mask = PREFIX_MASK(cur_int->splitPos);

				// Does the prefix still match?
				if(cur_int->base ^ (e.first & mask)){
					// We found a mismatch
					break;
				} else {
					// Traverse further down
					if(extractBit(e.first, cur_int->splitPos)){
						cur = cur_int->right;
					} else {
						cur = cur_int->left;
					}
				}
			}

			if(cur->type == LEAF){
				// Check if the base matches
				if(cur->base == e.first){
					//Push route to leaf
					static_cast<Leaf*>(cur)->push_route(e.second, len);
					continue;
				}
			}

			uint32_t diff; // Significant difference between bases

			// Are we "above" the root
			if(cur){
				if(cur->type == INTERNAL){
					Internal* cur_int = static_cast<Internal*>(cur);
					diff = cur->base ^ (e.first & PREFIX_MASK(cur_int->splitPos));
				} else {
					diff = cur->base ^ e.first;
				}
			} else {
				// root is a leaf, the new addr is a second one
				diff = root->base ^ e.first;
			}

			// Where is the new split position?
			int pos = mostSigOne(diff);
			// Sanity check
			if((pos < 0) || (pos > 32)){
				cerr << "PCTrie::buildTrie() something went wrong" << endl;
				cerr << "\tThe first bit conflict is invalid" << endl;
				__builtin_trap();
			}
			uint32_t mask = PREFIX_MASK(pos);

			uint32_t new_base = e.first & mask; // This prefix is still shared
			Internal* new_int;

			// Are we "above" the root
			if(cur){
				new_int = new Internal(cur->parent, new_base, NULL, NULL, NULL, pos);
			} else {
				new_int = new Internal(NULL, new_base, NULL, NULL, NULL, pos);
			}
			Leaf* new_leaf = new Leaf(new_int, e.first);

			// Push route to new leaf
			new_leaf->push_route(e.second, len);

			// Which child is left, which is right
			if(extractBit(e.first, pos)){
				// New leaf is right
				new_int->right = new_leaf;
				new_int->left = (cur) ? cur : root;
			} else {
				// New leaf is left
				new_int->left = new_leaf;
				new_int->right = (cur) ? cur : root;
			}

			// Do the bases of the Internal node and the possible left leaf match?
			if((new_int->left->type == LEAF) && (new_int->base == new_int->left->base)){
				new_int->leaf = static_cast<Leaf*>(new_int->left);
			}

			// Are we "above" the root
			if(cur){
				cur->parent = new_int;
			} else {
				root = new_int;
			}

			// Are we dealing with the root node?
			if(new_int->parent){
				Internal* parent = new_int->parent;
				if(parent->left == cur){
					parent->left = new_int;
				} else {
					parent->right = new_int;
				}
			} else {
				root = new_int;
			}

			// Sanity check
			if(new_int->left == NULL){
				cerr << "PCTrie::buildTrie() new_int->left is NULL" << endl;
				__builtin_trap();
			}
			if(new_int->right == NULL){
				cerr << "PCTrie::buildTrie() new_int->right is NULL" << endl;
				__builtin_trap();
			}
		}
	}
};

PCTrie::PCTrie(Table& table) : table(table) {
	buildTrie();
};

uint32_t PCTrie::route(uint32_t addr){
	Node* cur = root;

	// Is the trie populated at all?
	if(!root){
		return 0xffffffff;
	}

	// Traverse down
	while(cur->type == INTERNAL){
		Internal* cur_int = static_cast<Internal*>(cur);
		if(extractBit(addr, cur_int->splitPos)){
			cur = cur_int->right;
		} else {
			cur = cur_int->left;
		}
	}

	// cur is a leaf at this position
	Leaf* cur_leaf = static_cast<Leaf*>(cur);

	// Check for a match, XXX same as below (inline function maybe?)
	for(auto& entry : cur_leaf->entries){
		uint32_t mask = PREFIX_MASK(entry.prefix_length);
		if((addr & mask) == cur_leaf->base){
			return entry.next_hop;
		}
	}
	cur = cur->parent;

	// Backtrack upwards
	while(cur){
		Internal* cur_int = static_cast<Internal*>(cur);
		if(cur_int->leaf){
			cur_leaf = static_cast<Leaf*>(cur_int->leaf);

			// Check for a match, XXX same as above
			for(auto& entry : cur_leaf->entries){
				uint32_t mask = PREFIX_MASK(entry.prefix_length);
				if((addr & mask) == cur_leaf->base){
					return entry.next_hop;
				}
			}
		}
		cur = cur->parent;
	}

	return 0xffffffff;

#if 0 // Still from BasicTrie
	// Bootstrap first iteration
	Internal* cur = root;
	int pos = 0;
	uint32_t bit = extractBit(addr, pos);
	Internal* next = (bit) ? cur->right : cur->left;

	// Traverse downwards
	while(next){
		cur = next;
		bit = extractBit(addr, ++pos);
		next = (bit) ? cur->right : cur->left;
	}

	// Traverse upwards, until a matching prefix is found
	while(cur){
		if(cur->leaf && ((cur->leaf->mask & addr) == cur->leaf->base)){
			return cur->leaf->next_hop;
		}
		cur = cur->parent;
	}

	return 0xffffffff;
#endif
};

#if 0
void BasicTrie::routeBatch(uint32_t* in, uint32_t* out, int count){
	// Mask - which addresses are finished
	uint64_t cur_mask = 0;
	uint64_t finished_mask = (((uint64_t) 1) << count) -1;

	// Mask - which traversals go upwards again
	uint64_t dir_mask = 0;

	// Bootstrap first iteration
	Internal* cur[64];
	int pos[64] = {0};
	uint32_t bit[64] = {0};
	Internal* next[64];

	for(int i=0; i<64; i++){
		cur[i] = root;
	}

	for(int i=0; i<count; i++){
		bit[i] = extractBit(in[i], pos[i]);
		next[i] = (bit[i]) ? cur[i]->right : cur[i]->left;
	}

	// Set all results to not-found
	for(int i=0; i<count; i++){
		out[i] = 0xffffffff;
	}

	// Until all addresses are flagged finished
	while(cur_mask != finished_mask){
		// Iterate over the complete batch
		for(int i=0; i<count; i++){
			// Filter/Mask for not finished addresses
			if(!(cur_mask & (((uint64_t) 1) << i))){
				// Decide if traversal is downwards or upwards
				if(!(dir_mask & (((uint64_t) 1) << i))){
					// Traverse downwards
					cur[i] = next[i];
					bit[i] = extractBit(in[i], ++pos[i]);
					next[i] = (bit[i]) ? cur[i]->right : cur[i]->left;
					if(!next[i]){
						dir_mask |= ((uint64_t) 1) << i;
					}
					__builtin_prefetch(next[i]);
				} else {
					// Traverse upwards, until a matching prefix is found
					if(cur[i]->leaf &&
							((cur[i]->leaf->mask & in[i])
							 == cur[i]->leaf->base)){
						out[i] = cur[i]->leaf->next_hop;
						cur_mask |= ((uint64_t) 1) << i;
					}
					cur[i] = cur[i]->parent;
					__builtin_prefetch(cur[i]);
				}
			}
		}
	}
}
#endif

