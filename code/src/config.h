#pragma once

#include <string>

// hdfs 
const std::string HDFS_ADDRES = "hdfs://namenode:9000";
const std::string MODEL_ROOT_DIR = "/";
const std::string MODEL_PREFIX = "model_";
const std::string LOCAL_ROOT_DIR = "/tmp/";

// ETCD 
const std::string ECTD_URL = "http://etcd:2379";
const std::string modelservice_dir = "/services/modelservice/";
const std::string internalservice_dir = "/services/tmpservice/";
const std::string version_dir = "/services/version/";

const uint32_t MODEL_NAME_LEN = 25;
const uint32_t BUFFER_SIZE = 65536;

const uint64_t MEMORY = 16777216000;
const uint32_t SWITCH_TIME_LIMIT = 300;

