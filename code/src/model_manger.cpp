#include "model_manger.h"

#include "hdfs_hander.h"
#include "model_hander.h"
#include "utils.h"

#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

ModelManger::ModelManger(Options & options, etcd::Client* etcd)
    : options_(options),
    etcd_(etcd),
    hdfsHander_(HDFS_ADDRES, MODEL_ROOT_DIR,LOCAL_ROOT_DIR) 
{
    slice_num_per_node_ = 0;
    version_ = 0;
}

ModelManger::~ModelManger() {
    // if(model_)
    //     hdfsHander_.clearLocalModel(*model_);
    if(bg_thread != nullptr) {
        if(bg_thread->joinable())
            bg_thread->join();
        delete bg_thread;
    }
}

int ModelManger::init() { 
    if(hdfsHander_.init() != 0) {
        std::cout << "HDFSHander faild to init ...." << std::endl;
        return -1;
    }
    while (1) { 
        auto model = hdfsHander_.getLastestModel();
        if(model) { 
            if(loadNewModel(model)) {
                break;
            }
            std::cout << "Faild to load a Model ...." << std::endl;
            std::cout << *model << std::endl;
        }
        sleep(2);
    }
    run();
    return 0;
}

void ModelManger::run() {
    bg_thread = new std::thread([this](){
        while (1) { 
            auto model = hdfsHander_.getLastestModel();
            if(model && model->model_name.compare(model_->model_name) != 0) {
                if(loadNewModel(model)) {
                    sleep(30);
                }
            }
            sleep(2);
        }
    });
}

bool ModelManger::loadNewModel(ModelMeta * model) {
    std::vector<SliceMeta*> slices;
    int slice_num_per_node = model->slice_num / options_.node_num;
    
    // continue shard
    int start = slice_num_per_node * options_.node_id;
    int end = start + slice_num_per_node;
    if(end + slice_num_per_node > model->slice_num) {
        end = model->slice_num;
    }
    for(; start < end; ++start) {
        auto slice = model->slices[start];
        if(!hdfsHander_.loadSliceToLocal(*slice)) {
            return false;
        }
        slices.push_back(slice);
    }
    auto modelHander = std::make_shared<ModelHander>(options_.cpu, model, slices);
    if(modelHander == nullptr || modelHander->init() != 0) {
        return false;
    }
    // modelHander->LoadAll(slices.size());
    if(version_ == 0) {
        modelHander->LoadAll(slices.size());
    }else{
        modelHander->LoadAll(slices.size() / 0.75);
    }

    std::cout << "Start Switch a New Version Model to " <<  model->model_name << std::endl
            << *model << *modelHander;

    // sync all node
    std::string target_dir = version_dir + std::to_string(version_) + "/";
    std::string key = target_dir + std::to_string(options_.node_id);
    while(1) {
        pplx::task<etcd::Response> response_task = etcd_->set(key, std::to_string(options_.node_id));
        etcd::Response response = response_task.get();
        if (response.is_ok()){
            break;
        }
    }   

    while(1) {
        etcd::Response resp = etcd_->ls(target_dir).get();
        if (resp.is_ok() && resp.keys().size() == options_.node_num) {
            break;
        }
    }

    // update node
    version_++;
    slice_num_per_node_.store(slice_num_per_node);
    model_ = model;
    {
        std::unique_lock<std::mutex> lock(mu_);
        modelHanderPtr = modelHander;
        model_tables[model->model_name] = modelHander;
    }
    cv_.notify_all();

    //clear old model
    std::thread([this] () {
        sleep(10);
        std::unique_lock<std::mutex> lock(mu_);
        for(auto &it: model_tables) {
            if(it.second.use_count() == 1 && it.second.get() != modelHanderPtr.get()) {
                it.second->UnLoadAll();
                std::cout << it.second->model_name() << " is being unload....." << std::endl;
            }
        }
    }).detach();

    return true;
}

int ModelManger::getNodeForSlice(uint32_t silce_partition) {
    int node = silce_partition / slice_num_per_node_;
    if(node >= options_.node_num){
        node = options_.node_num - 1;
    }
    return node;
}

std::shared_ptr<ModelHander> ModelManger::getModelHander() {
    std::unique_lock<std::mutex> lock(mu_);
    return modelHanderPtr;
}

std::shared_ptr<ModelHander> ModelManger::getModelHander(std::string& model_name) {
    {
        std::unique_lock<std::mutex> lock(mu_);
        if(model_tables.find(model_name) != model_tables.end()) {
            return model_tables[model_name];
        }
    }
    
    while (1) {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [this, &model_name] { 
            return model_tables.find(model_name) != model_tables.end(); 
        });
        if(model_tables.find(model_name) != model_tables.end())
            break;
    }
    return model_tables[model_name];
}