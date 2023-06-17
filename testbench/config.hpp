#pragma once
#include<bits/stdc++.h>
#include"json.hpp"
using json = nlohmann::json;
using namespace std;
struct Slice {
	int seed_a;
	Slice() = default;
	Slice(const Slice &) = default;
	Slice& operator=(const Slice& t) = default;
	Slice(Slice &&) = default;
	Slice& operator=(Slice&& ) = default;
	~Slice() = default;
};
struct Version {
	string name;
	vector<Slice> slices;

	Version() = default;
	Version(const Version &) = default;
	Version& operator=(const Version&) = default;
	Version(Version &&) = default;
	Version& operator=(Version&&) = default;
	~Version() = default;
};
struct Config {
	bool benchmark_no_check_data;
	int benchmark_thread_num;
	int start_load_duration_limit_ms;
	int switch_load_duration_limit_ms;
	int qps_limit;
	int qps_step;
	int latency_limit;
	int failure_limit; //每千个运行多少失败

	int request_slice_num;
	int request_data_len;
	double switch_load_qps_ratio;

	int slice_size;
	int slice_cnt;
	vector<Version> versions;
	void debug() {
		cout << "slice_size: " << slice_size << endl;
		cout << "slice_cnt: " << slice_cnt << endl;
		cout << "versions.num: " << versions.size() << endl;
		cout << "benchmark_thread_num:" << benchmark_thread_num << endl;
	// 	for (auto &v: versions) {
	// 		cout << "version: " << v.name << "\tgen_seed: ";
	// 		string sep = "";
	// 		for (auto &s: v.slices) {
	// 			cout << sep << s.seed_a;
	// 			sep = ",\t";
	// 		}
	// 		cout << endl;
	// 	}
	}

	Config() = default;
	Config(const Config &) = default;
	Config& operator=(const Config&) = default;
	Config(Config &&) = default;
	Config& operator=(Config&&) = default;
	~Config() = default;
};
Config LoadConfig(const char* file) {
	Config config;
	ifstream t(file);
	json j = json::parse(string(istreambuf_iterator<char>(t), std::istreambuf_iterator<char>()));
	config.start_load_duration_limit_ms = j["start_load_duration_limit_ms"];
	config.switch_load_duration_limit_ms = j["switch_load_duration_limit_ms"];
	config.switch_load_qps_ratio  = j["switch_load_qps_ratio"];
	config.benchmark_thread_num = j["benchmark_thread_num"];
	config.benchmark_no_check_data = j["benchmark_no_check_data"];
	config.qps_limit = j["qps_limit"];
	config.qps_step  = j["qps_step"];
	config.latency_limit  = j["latency_limit"];
	config.failure_limit  = j["failure_limit"];
	config.request_slice_num  = j["request_slice_num"];
	config.request_data_len  = j["request_data_len"];

	config.slice_size = j["slice_size"];
	config.slice_cnt = j["slice_cnt"];
	for (auto v: j["versions"]) {
		Version version;
		version.name = v["name"];
		assert(v["slices"].size() >= config.slice_cnt);
		for (auto s: v["slices"]) {
			Slice slice;
			slice.seed_a = s["seed_a"];
			version.slices.push_back(slice);
		}
		config.versions.push_back(version);
	}
	config.debug();
	return config;
}