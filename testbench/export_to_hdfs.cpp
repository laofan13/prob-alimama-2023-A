#include<bits/stdc++.h>
#include"json.hpp"
#include"config.hpp"
using json = nlohmann::json;
using namespace std;

void Command(string command) {
	cout << "command: " << command << endl;
	system(command.c_str());
}

Config config;
int main(int argc, char** argv) {
	config = LoadConfig(argv[1]);

	vector<thread> threads;
	Command("hdfs dfs -fs hdfs://namenode:9000/  -rm -r -f /backup");
	Command("hdfs dfs -fs hdfs://namenode:9000/  -mkdir /backup");
	for (int i = 0; i < 3; i++) {
		threads.push_back(thread([i]{
			auto &v = config.versions[i];
			Command("hdfs dfs -fs hdfs://namenode:9000/  -rm -r -f /"+v.name);
			Command("hdfs dfs -fs hdfs://namenode:9000/  -put -f "+v.name+"  /backup/");
			Command("hdfs dfs -fs hdfs://namenode:9000/  -touch /backup/"+v.name+"/model.done");
		}));
	}
	for (auto &t: threads) t.join();
	Command("hdfs dfs  -fs hdfs://namenode:9000/  -rm -f /rollback.version");
}
