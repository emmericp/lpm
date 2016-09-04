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
			if(new_int->parent && (new_int->splitPos <= new_int->parent->splitPos)){
				cerr << "PCTrie::buildTrie() parent splitPos is >= new splitPos" << endl;
				__builtin_trap();
			}

			// Add snapshot for qtree history
			add_qtree_snapshot();
		}
	}
};

PCTrie::PCTrie(Table& table) : root(NULL), table(table) {
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

	// Check for a match, XXX same as below (lambda function maybe?)
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
};

string PCTrie::get_qtree_snapshot(){
	string output = "";
	output += "\
%\n\
	\\hspace{10pt}\n\
	\\begin{tikzpicture}\n\
		\\tikzset{every tree node/.style={align=center,anchor=north}}\n";

	// example: \Tree [ [ .0 0\\A [ .1\\B 0 [ .1 0 1\\F ] ] ] [ .1 0\\C [ .1\\D 0\\E 1 ]]]
	output += "\\Tree ";
	string references = "";

	function<void (PCTrie::Internal*)> recursive_helper =
		[&output,&references,&recursive_helper](Internal* node){

		output += " [ ";

		// Helper function for later
		auto leaf_printer = [&output](Leaf* leaf){
			output += "\\node[draw]("
				+ to_string((uint64_t) leaf)
				+ "){" + ip_to_str(leaf->base);
			for(auto& e : leaf->entries){
				output += "\\\\";
				output += ip_to_str(e.next_hop) + ":" + to_string(e.prefix_length);
			}
			output += "}; ";
		};

		// node itself
		output += ".\\node(" + to_string((uint64_t) node) + "){"
			+ ip_to_str(node->base) + "-" + to_string(node->splitPos) + "}; ";

		if(node->leaf){
		// \draw[dashed,->] (root)..controls +(west:1.5) and +(north west:1.2) .. (A);
			references += "\\draw[semithick,dashed,->] ("
				+ to_string((uint64_t) node)
				+ ")..controls +(west:1.5) and +(north west:1.5) .. ("
				+ to_string((uint64_t) node->leaf)
				+ ");\n";
		}

		// left child
		if(node->left->type == INTERNAL){
			recursive_helper(static_cast<Internal*>(node->left));
		} else {
			leaf_printer(static_cast<Leaf*>(node->left));
		}

		// Insert line break - otherwise pdflatex breaks
		output += "\n";

		// right child
		if(node->right->type == INTERNAL){
			recursive_helper(static_cast<Internal*>(node->right));
		} else {
			leaf_printer(static_cast<Leaf*>(node->right));
		}

		output += " ] ";
	};

	// Let's roll
	if(root->type == INTERNAL){
		recursive_helper(static_cast<Internal*>(root));
	} else {
		cerr << "PCTrie::get_qtree() root is just a leaf..." << endl;
		__builtin_trap();
	}

	output += references;

	output +="\
	\\end{tikzpicture}\n\
	\\vspace{10pt}\n\
%\n";
	return output;
};

void PCTrie::add_qtree_snapshot(){
	qtree_prev += get_qtree_snapshot();
};

string PCTrie::finalize_qtree(string tree){
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

string PCTrie::get_qtree(){
	return finalize_qtree(get_qtree_snapshot());
};

string PCTrie::get_qtree_history(){
	return finalize_qtree(qtree_prev);
}

