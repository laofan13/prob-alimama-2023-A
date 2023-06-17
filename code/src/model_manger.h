#pragma once

#include "config.h"

#include "hdfs_hander.h"
#include "utils.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>
#include <thread>
#include <map>

#include <etcd/Client.hpp>
#include <etcd/Response.hpp>

class ModelHander;

class ModelManger
{
public:
    ModelManger();
    ModelManger(Options& options, etcd::Client* etcd_);

    ModelManger(ModelManger &) = delete;
    ModelManger& operator=(ModelManger &) = delete;

    ~ModelManger();

    int init();
    std::shared_ptr<ModelHander> getModelHander();
    std::shared_ptr<ModelHander> getModelHander(std::string& model_name);
    int getNodeForSlice(uint32_t silce_partition);

private:
    void run();
    bool loadNewModel(ModelMeta * model);

private:
    Options options_;
    HDFSHander hdfsHander_;
    etcd::Client* etcd_;

    int version_ = 0;
    std::atomic<int> slice_num_per_node_;

    ModelMeta* model_;

    std::map<std::string, std::shared_ptr<ModelHander>> model_tables;
    std::shared_ptr<ModelHander> modelHanderPtr;

    std::mutex mu_;
    std::condition_variable cv_;
    
    std::thread* bg_thread;
};
