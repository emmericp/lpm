#include "naive.hpp"

Naive::Naive(Table& table) : entries(table.get_sorted_entries()) {
	uint32_t mask = 0;
	for(int i=0; i<33; i++){
		masks[i] = mask;
		mask = mask >> 1;
		mask |= 1 << 31;
	}
};

uint32_t Naive::route(uint32_t addr) {
	for(int len=32; len>=0; len--){
		for(auto& e : entries[len]){
			if(e.first == (addr & masks[len])){
				return e.second;
			}
		}
	};
	return 0xffffffff;
};

int Naive::route_challenge(list<pair<uint32_t, uint32_t>>& challenge){

	int failed = 0;
	int success = 0;
	clock_t start = clock();
	for(auto& a : challenge){
		uint32_t res = route(a.first);
		if(res != a.second){
			cout << "Failed IP: " << ip_to_str(a.first) << endl;
			cout << "Expected : " << ip_to_str(a.second) << endl;
			cout << "Got      : " << ip_to_str(res) << endl << endl;
			failed++;
		} else {
			success++;
		}
	}
	clock_t end = clock();

	cerr << "Lookups took " << (1.0*(end-start)) / CLOCKS_PER_SEC << " seconds" << endl;

	cout << "Successful lookups: " << success << endl;
	cout << "Failed lookups: " << failed << endl;

	return failed == 0 ? 0 : 1;
}
