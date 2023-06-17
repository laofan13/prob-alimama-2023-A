#pragma once

#include "config.h"

#include <algorithm>
#include <cstddef>
#include <ostream>
#include <queue>
#include <utility>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <condition_variable>

#ifdef BAZEL_BUILD
#include "examples/protos/alimama.grpc.pb.h"
#else
#include "alimama.grpc.pb.h"
#endif

#include "hdfs_hander.h"
#include "ModelSliceReader.h"

using alimama::proto::ModelService;
using alimama::proto::SliceRequest;
using alimama::proto::Request;
using alimama::proto::Response;

struct ModelMeta;
struct SliceRequestTask;
class SliceReader;
class SlicesHander;
class ModelSliceReader;

struct RequestResult {
    int req_no;
    int req_num;
    int finish_num;
    bool faild;
    bool finish;
    std::mutex mu;
    std::condition_variable cv;

    // result
    int size;
    char* data;

    ~RequestResult() {
        // if(data)
        //     delete data;
    }

    RequestResult(int i, int req)
        : req_no(i), req_num(req) {
        finish_num = 0;
        faild = false;
        finish = false;
    };

    RequestResult(int i, int req, int s)
        : req_no(i), req_num(req),size(s) {
            finish_num = 0;
            faild = false;
            finish = false;
    };
};

struct SliceData {
    uint64_t slice_partition;
    uint64_t start;
    uint64_t len;

    int pos;

    SliceData(int i, int s, int l, int p)
        : slice_partition(i), start(s), len(l), pos(p) {};
};

struct RequestTask {
    int num;
    std::string model_name;
    std::shared_ptr<RequestResult> result;
    std::vector<SliceData> req_slice_list;

    RequestTask() {
        num = 0;
    }

    RequestTask(int n, std::string model_name_,  std::shared_ptr<RequestResult> res)
        : num(n), model_name(model_name_) ,result(res) 
    {

    }

    RequestTask(RequestTask& other) {
        // std::cout << "RequestTask copy constructor..." << std::endl;
        num = other.num;
        model_name = other.model_name;
        result = other.result;
        req_slice_list = other.req_slice_list;
    }

    RequestTask(RequestTask&& other) {
        // std::cout << "RequestTask move constructor..." << std::endl;
        num = other.num;
        model_name = std::move(other.model_name);
        result = std::move(other.result);
        req_slice_list = std::move(other.req_slice_list);
    }

    RequestTask& operator=(RequestTask& other) {
        // std::cout << "RequestTask copy asgin..." << std::endl;
        if (this != &other) {
            num = other.num;
            model_name = other.model_name;
            result = other.result;
            req_slice_list = other.req_slice_list;
        }
        return *this;
    }

    RequestTask& operator=(RequestTask&& other){
        // std::cout << "RequestTask move asgin..." << std::endl;
        if (this != &other) {
            num = other.num;
            model_name = std::move(other.model_name);
            result = std::move(other.result);
            req_slice_list = std::move(other.req_slice_list);
        }
        return *this;
    }
};

// std::ostream& operator<<(std::ostream& os, RequestResult& requestResult);
// std::ostream& operator<<(std::ostream& os, SliceRequestTask& sliceRequestTask);

class ModelHander {
public:
    ModelHander(int n_thread, ModelMeta*  model, std::vector<SliceMeta*>& slices);

    ~ModelHander();

    void LoadAll(int num) {
        for(int i = 0; i < num && i < slices_.size(); ++i) {
            auto slice = slices_[i];
            slice_readers.emplace(slice->slice_no, ModelSliceReader());
            slice_readers[slice->slice_no].Load(slice->local_slice_path);
        }
    }

    int init();
    
    void UnLoadAll();
    std::string model_name() {
        return model_->model_name;
    }
    void Push(RequestTask & req_task);

    friend std::ostream& operator<<(std::ostream& os, const ModelHander& modelHander);
private:
    void start();
    void processTask(RequestTask & req_task);
    void processBatchTask(std::vector<RequestTask> & batchs);
    

private:
    int n_thread_;
    
    ModelMeta* model_;
    std::vector<SliceMeta*> slices_;
    std::map<int, SliceMeta*> slice_tables;
    std::map<int, ModelSliceReader> slice_readers;

    bool is_stop_ {false};
    std::queue<RequestTask> request_queues;
    std::vector<std::thread> reader_threads;
    std::mutex mu_;
    std::condition_variable cv_;
};