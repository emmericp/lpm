#include "pcTrieFast.hpp"

using namespace std;

#define TRIE_NULL 0xffffffff

/*
void PCTrieFast::Leaf::pushRoute(
		uint32_t pos,
		struct NextHop e,
		struct PCTrieFast::Leaf* leafs,
		struct NextHop* nextHops,
		Allocator<struct NextHop>& nextHops_alloc){ */
void PCTrieFast::Leaf::pushRoute(
		struct NextHop e,
		struct NextHop* nextHops,
		Allocator<struct NextHop>& nextHops_alloc){
	// Copy nextHops, erase, sort, insert
	struct NextHop* nextHop =
		(struct NextHop*) malloc((this->number +1) * sizeof(struct NextHop));
	memcpy(nextHop, &nextHops[this->nextHops], this->number);
	nextHop[this->number].nextHop = e.nextHop;
	nextHop[this->number].prefixLength = e.prefixLength;
	sort(&nextHop[0], &nextHop[this->number]);
	this->nextHops = nextHops_alloc.insert(nextHop, this->number+1);
	this->number++;
	free(nextHop);
};

void PCTrieFast::buildTrie() {
	// Build trie
	vector<map<uint32_t,uint32_t>> tbl = table.get_sorted_entries();
	for(unsigned int len=0; len<=32; len++){
		for(auto& e : tbl[len]){
			// Is the trie empty?
			if(buildState == EMPTY){
				// Create structs and insert into arrays
				/*
				struct Internal new_int;
				new_int.base = 0;
				new_int.parent = TRIE_NULL;
				new_int.splitPos = 0;
				new_int.childTypes = 0;
				new_int.leaf = TRIE_NULL;
				new_int.left = TRIE_NULL;
				new_int.right = TRIE_NULL;
				root = internals_alloc.insert(new_int);
				*/

				struct Leaf new_leaf;
				new_leaf.base = e.first;
				new_leaf.parent = root;
				new_leaf.nextHops = TRIE_NULL;
				new_leaf.number = 1;
				root = leafs_alloc.insert(new_leaf);

				struct NextHop new_nextHop;
				new_nextHop.nextHop = e.second;
				new_nextHop.prefixLength = len;
				uint32_t nextHop = nextHops_alloc.insert(new_nextHop);

				// Set correct positions
				leafs[root].nextHops = nextHop;
				/*
				internals[root].left = leaf;
				internals[root].leaf = leaf;
				*/

				buildState = ONE_LEAF;
				continue;
			}

			if((buildState == ONE_LEAF) && (leafs[root].base == e.first)){
				// Just add it to the existing next hop entry
				struct Leaf* leaf = &leafs[root];

				leaf->pushRoute({.nextHop = e.second, .prefixLength = len},
						nextHops, nextHops_alloc);

				continue;
			}

			// Traverse to the first prefix conflict, or until a leaf is found
			Node cur(root, buildState-1, internals, leafs);
			while(cur.is_internal){
				uint32_t mask = PREFIX_MASK(internals[cur.pos].splitPos);

				// Does the prefix still match?
				if(cur.base() ^ (e.first & mask)){
					// We found a mismatch
					break;
				} else {
					// Traverse further down
					if(extractBit(e.first, internals[cur.pos].splitPos)){
						cur.goRight();
					} else {
						cur.goLeft();
					}
				}
			}

			if(!cur.is_internal){
				// Check if the base matches
				if(cur.base() == e.first){
					//Push route to leaf
					struct Leaf* leaf = &leafs[cur.pos];

					leaf->pushRoute({.nextHop = e.second, .prefixLength = len},
						nextHops, nextHops_alloc);

					continue;
				}
			}

			uint32_t diff; // Significant difference between bases

			if(cur.is_internal){
				diff = cur.base() ^ (e.first & PREFIX_MASK(internals[cur.pos].splitPos));
			} else {
				diff = cur.base() ^ e.first;
			}

			// Where is the new split position?
			int pos = mostSigOne(diff);
			// Sanity check
			if((pos < 0) || (pos > 32)){
				cerr << "PCTrieFast::buildTrie() something went wrong" << endl;
				cerr << "\tThe first bit conflict is invalid" << endl;
				abort();
			}
			uint32_t mask = PREFIX_MASK(pos);

			// Allocate new data structures
			uint32_t new_base = e.first & mask; // This prefix is still shared
			struct Internal new_int;
			new_int.base = new_base;
			new_int.splitPos = pos;
			new_int.parent = cur.parent();
			new_int.childTypes = 0;
			new_int.leaf = TRIE_NULL;
			new_int.left = TRIE_NULL;
			new_int.right = TRIE_NULL;
			uint32_t new_int_pos = internals_alloc.insert(new_int);
			Internal* new_int_p = &internals[new_int_pos];

			//Leaf* new_leaf = new Leaf(new_int, e.first);
			struct Leaf new_leaf;
			new_leaf.parent = new_int_pos;
			new_leaf.base = e.first;
			new_leaf.number = 1;
			uint32_t new_leaf_pos = leafs_alloc.insert(new_leaf);

			struct NextHop nextHop;
			nextHop.nextHop = e.second;
			nextHop.prefixLength = len;
			uint32_t nextHop_pos = nextHops_alloc.insert(nextHop);

			leafs[new_leaf_pos].nextHops = nextHop_pos;

			// Which child is left, which is right
			if(extractBit(e.first, pos)){
				// New leaf is right
				new_int_p->right = new_leaf_pos;
				new_int_p->left = cur.pos;
				if(cur.is_internal){
					SET_LEFT_INTERNAL(new_int_p);
				}
			} else {
				// New leaf is left
				new_int_p->left = new_leaf_pos;
				new_int_p->right = cur.pos;
				if(cur.is_internal){
					SET_LEFT_INTERNAL(new_int_p);
				}
			}

			// Do the bases of the Internal node and the possible left leaf match?
			if(!IS_LEFT_INTERNAL(new_int_p) &&
					(new_int_p->base == (internals[new_int_p->left].base & PREFIX_MASK(pos)))){
				new_int_p->leaf = new_int_p->left;
			}

			// Do we need to pass the leaf node of the left child up?
			if(IS_LEFT_INTERNAL(new_int_p) && internals[new_int_p->left].leaf != TRIE_NULL &&
					leafs[internals[new_int_p->left].leaf].hasMoreGeneralRoute(len)){
				new_int_p->leaf = internals[new_int_p->left].leaf;
				internals[new_int_p->left].leaf = TRIE_NULL;
			}

			// Are we dealing with the root node?
			if(new_int_p->parent != TRIE_NULL){
				struct Internal* parent = &internals[new_int_p->parent];
				if(parent->left == cur.pos){
					parent->left = new_int_pos;
				} else {
					parent->right = new_int_pos;
				}
			} else {
				root = new_int_pos;
				buildState = NORMAL; // if it wasn't already
			}

			// Sanity check
			if(new_int_p->left == TRIE_NULL){
				cerr << "PCTrieFast::buildTrie() new_int->left is NULL" << endl;
				abort();
			}
			if(new_int_p->right == TRIE_NULL){
				cerr << "PCTrieFast::buildTrie() new_int->right is NULL" << endl;
				abort();
			}
			if(new_int_p->parent &&
					(new_int_p->splitPos <= internals[new_int_p->parent].splitPos)){
				cerr << "PCTrieFast::buildTrie() parent splitPos is >= new splitPos" << endl;
				abort();
			}

			// Add snapshot for qtree history
#if PCTRIE_QTREE == 1
			addQtreeSnapshot();
#endif
		}
	}
};

#define S_32MiB (32 * (1024*1024))

PCTrieFast::PCTrieFast(Table& table) :
	internals_alloc(S_32MiB),
	leafs_alloc(S_32MiB),
	nextHops_alloc(S_32MiB),
	root(0xffffffff),
	table(table),
	buildState(EMPTY) {

	internals = internals_alloc.getPointer();
	leafs = leafs_alloc.getPointer();
	nextHops = nextHops_alloc.getPointer();

	buildTrie();
};

#if 0
unsigned int PCTrieFast::getSize(){

	unsigned int num_int = 0;
	unsigned int num_leaf = 0;
	unsigned int num_entries = 0;

	function<void(Internal*)> summer =
		[&num_int, &num_leaf, &num_entries, &summer](Internal* cur) {

		num_int++;

		if(cur->left->type == INTERNAL){
			summer(static_cast<Internal*>(cur->left));
		} else {
			num_leaf++;
			num_entries += static_cast<Leaf*>(cur->left)->entries.max_size();
		}

		if(cur->right->type == INTERNAL){
			summer(static_cast<Internal*>(cur->right));
		} else {
			num_leaf++;
			num_entries += static_cast<Leaf*>(cur->right)->entries.max_size();
		}
	};

	if(root && root->type == INTERNAL){
		summer(static_cast<Internal*>(root));
	}

	unsigned int size_int = sizeof(Internal);
	unsigned int size_leaf = sizeof(Leaf);
	unsigned int size_leaf_entry = sizeof(Leaf::leaf_entry);

#if 0
	cerr << "Number of internal nodes: " << num_int
		<< " size each: " << size_int
		<< " size all: " << (size_int * num_int) / 1024 << " KiB" << endl;
	cerr << "Number of leaf nodes: " << num_leaf
		<< " size each: " << size_leaf
		<< " size all: " << (size_leaf * num_leaf) / 1024 << " KiB" << endl;
	cerr << "Number of leaf entries: " << num_entries
		<< " size each: " << size_leaf_entry
		<< " size all: " << (size_leaf_entry * num_entries) / 1024 << " KiB" << endl;
#endif

	return size_int * num_int + size_leaf * num_leaf + size_leaf_entry  * num_entries;
};
#endif

#if 0
uint32_t PCTrieFast::route(uint32_t addr){
	Node* cur = root;

	// Is the trie populated at all?
	if(!root){
		return 0xffffffff;
	}

	// Traverse down
	while(likely(cur->type == INTERNAL)){
		Internal* cur_int = static_cast<Internal*>(cur);
		if(extractBit(addr, cur_int->splitPos)){
			cur = cur_int->right;
		} else {
			cur = cur_int->left;
		}
	}

	// cur is a leaf at this position
	Leaf* cur_leaf = static_cast<Leaf*>(cur);

	// Check for a match, XXX same as below (lambda function maybe?)
	for(auto& entry : cur_leaf->entries){
		uint32_t mask = PREFIX_MASK(entry.prefix_length);
		if((addr & mask) == cur_leaf->base){
			return entry.next_hop;
		}
	}
	cur = cur->parent;

	// Backtrack upwards
	while(likely(cur)){
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
};
#endif

#if PCTRIE_QTREE == 1

string PCTrieFast::getQtreeSnapshot(){
	stringstream output;
	output << "\
%\n\
	\\hspace{10pt}\n\
	\\begin{tikzpicture}\n\
		\\tikzset{every tree node/.style={align=center,anchor=north}}\n";

	// example: \Tree [ [ .0 0\\A [ .1\\B 0 [ .1 0 1\\F ] ] ] [ .1 0\\C [ .1\\D 0\\E 1 ]]]
	output << "\\Tree ";
	stringstream references;

	function<void (PCTrieFast::Internal*)> recursive_helper =
		[&output,&references,&recursive_helper](Internal* node){

		output << " [ ";

		// Helper function for later
		auto leaf_printer = [&output](Leaf* leaf){
			output << "\\node[draw](" << leaf << "){" << ip_to_str(leaf->base);
			for(auto& e : leaf->entries){
				output << "\\\\" << ip_to_str(e.next_hop) << ":" << e.prefix_length;
			}
			output << "};";
		};

		// node itself
		output << ".\\node(" << node << "){" << ip_to_str(node->base)
			<< "-" << static_cast<int>(node->splitPos) << "};";

		if(node->leaf){
			references << "\\draw[semithick,dashed,->] (" << node
				<< ") .. controls +(west:1.8) and +(north west:1.8) .. (" << node->leaf << ");\n";
		}

		// left child
		if(node->left->type == INTERNAL){
			recursive_helper(static_cast<Internal*>(node->left));
		} else {
			leaf_printer(static_cast<Leaf*>(node->left));
		}

		// Insert line break - otherwise pdflatex breaks
		output << "\n";

		// right child
		if(node->right->type == INTERNAL){
			recursive_helper(static_cast<Internal*>(node->right));
		} else {
			leaf_printer(static_cast<Leaf*>(node->right));
		}

		output << " ] ";
	};

	// Let's roll
	if(root->type == INTERNAL){
		recursive_helper(static_cast<Internal*>(root));
	} else {
		cerr << "PCTrieFast::get_qtree() root is just a leaf..." << endl;
		abort();
	}

	output << references.str();

	output <<"\
	\\end{tikzpicture}\n\
	\\vspace{10pt}\n\
%\n";
	return output.str();
};

void PCTrieFast::addQtreeSnapshot(){
	qtree_prev << getQtreeSnapshot();
};

string PCTrieFast::finalizeQtree(string tree){
	string output = "\
\\documentclass[preview,multi={tikzpicture},border={5pt 5pt 5pt 5pt}]{standalone} \n \
%\n\
\\usepackage{tikz}\n\
\\usepackage{tikz-qtree}\n\
%\n\
\\begin{document}\n\
%\n";

	output += tree;

	output += "\
%\n\
\\end{document}\n";

	return output;
};

string PCTrieFast::getQtree(){
	return finalizeQtree(getQtreeSnapshot());
};

string PCTrieFast::getQtreeHistory(){
	return finalizeQtree(qtree_prev.str());
}

#endif

