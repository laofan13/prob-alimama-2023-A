#include "utils.h"
#include <cstddef>
#include <iostream>
#include <ostream>
#include <string>

std::ostream& operator<<(std::ostream& os, Options& options) {
    os << "node Option information: {"  <<  std::endl;
    os << '\t' << "node_id: " << options.node_id << std::endl
        << '\t' << "node_num: " << options.node_num << std::endl
        << '\t' << "mempry: " << options.mempry << std::endl
        << '\t' << "cpu: " << options.cpu << std::endl;
    os << '\t' << "}"<< std::endl;
    return os;
}

std::string getLocalIP() {
    struct ifaddrs *ifAddrStruct = NULL;
    void *tmpAddrPtr = NULL;
    std::string localIP;
    getifaddrs(&ifAddrStruct);
    while (ifAddrStruct != NULL) {
        if (ifAddrStruct->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            std::string interfaceName(ifAddrStruct->ifa_name);
            if (interfaceName == "en0" || interfaceName == "eth0") {
                return addressBuffer;
            }
        }
        ifAddrStruct = ifAddrStruct->ifa_next;
    }
    return "";
}

Options loadENV() {
    Options options;
    const char* env_p = std::getenv("NODE_ID");
    if(env_p)
        options.node_id = std::stoi(env_p) - 1;
    else{
        options.node_id = 0;
    }
    // std::cout<< "NODE_ID: " << options.node_id << std::endl;
    env_p = std::getenv("NODE_NUM");
    if(env_p)
        options.node_num = std::stoi(env_p);
    else{
        options.node_num = 1;
    }
    // std::cout<< "NODE_NUM: " << options.node_num << std::endl;
    env_p = std::getenv("MEMORY");
    if(env_p)
        options.mempry = std::stoull(env_p) << 30; //4G
    else{
        options.mempry = 2 << 30; //2G
    }
    // std::cout<< "MEMORY: " << options.mempry << std::endl;
    env_p = std::getenv("CPU");
    if(env_p != nullptr)
        options.cpu = std::stoi(env_p);
    else{
        options.cpu = 2;
    }
    // std::cout<< "CPU: " << options.cpu << std::endl;
    return options;
}

