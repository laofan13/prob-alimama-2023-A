#pragma once

#include <string>
#include <cstdlib> 

// network 
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

struct Options {
    int node_id;
    int node_num;
    int cpu;
    uint64_t mempry;
};

std::ostream& operator<<(std::ostream& os, Options& options);

std::string getLocalIP();
Options loadENV();

