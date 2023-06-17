#include <string>
#include <iostream>

#include <etcd/Client.hpp>
#include <etcd/Response.hpp>


std::string str_url = "http://etcd:2379";
std::string str_key = "a";

std::string modelservice_dir = "/services/modelservice/";

int main() {
    etcd::Client etcd(str_url);
    etcd::Response resp = etcd.ls(modelservice_dir).get();
    if(!resp.is_ok()) {
        std::cout << "faild to get etcd service" << std::endl;
        return 0;
    }
    for (size_t i = 0; i < resp.keys().size(); i++) {
        std::cout << "key: " << std::string(resp.key(i)) 
            << " value: " << resp.value(i).as_string() << std::endl;
    }
    etcd.rmdir(modelservice_dir, true);
    return 0;
}
