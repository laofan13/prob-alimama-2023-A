#include<bits/stdc++.h>
#include"json.hpp"
#include"config.hpp"
#include"hash.hpp"
using json = nlohmann::json;
using namespace std;

Config config;

bool mkdir(const std::string& path) {
    std::filesystem::path dir(path);
    if (std::filesystem::create_directory(dir)) {
        return true;
    }
    return false;
}


void gen_data(string file_name, int file_size, int seeda) {
	ofstream outfile(file_name, ios::binary);
	char* buf = new char[file_size];
	for (int i = 0; i < file_size/4; i++) {
	    *((int*)&buf[i*4]) = myhash(i, seeda);
	}
	outfile.write(buf, file_size);
	delete buf;
	outfile.close();	
}

int main(int argc, char** argv){
	config = LoadConfig(argv[1]);
	for (int i = 0; i < config.slice_cnt; i++) {
		int sa = config.versions[0].slices[i].seed_a;
		int sb = config.versions[1].slices[i].seed_a;
		int sc = config.versions[2].slices[i].seed_a;
		for (int j = 0; j < config.slice_size/4; j++) {
			int a = myhash(j, sa);
			int b = myhash(j, sb);
			int c = myhash(j, sc);
			if (a == b || b == c || a == c) {
				cout << i << endl;
			}
		}
	}
	cout << "no confilt" << endl;
	// return 0;
	for (auto &v: config.versions) {
		mkdir(v.name);

//version:2023_03_01_12_30_00
//slice:model_slice.001,size:512000000
//slice:model_slice.002,size:512000000
//slice:model_slice.003,size:512000000
//...

		ofstream meta(v.name+"/model.meta");
		meta << "version:" << v.name << endl << endl;
		for (int i = 0; i < config.slice_cnt; i++) {
			string filename = "model_slice."+string(to_string(config.slice_cnt).size() - to_string(i).size(), '0') + to_string(i);
			gen_data(v.name+"/" + filename, config.slice_size, v.slices[i].seed_a);
			meta << "slice:" << filename << "," << "size:" << config.slice_size << endl;
		}
		meta.close();
	}
}
