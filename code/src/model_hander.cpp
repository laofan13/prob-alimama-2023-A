#include "model_hander.h"
#include "hdfs_hander.h"
#include "utils.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

using alimama::proto::SliceRequest;
using alimama::proto::Request;
using alimama::proto::Response;

ModelHander::ModelHander(int n_thread, ModelMeta*  model, std::vector<SliceMeta*>& slices)
    : n_thread_(n_thread),
    model_(model),
    slices_(std::move(slices))
{   
        is_stop_ = false;
}

ModelHander::~ModelHander() {
    {
        std::unique_lock<std::mutex> lock(mu_);
        is_stop_ = true;
    }
    cv_.notify_all();
    for(auto & thread: reader_threads) {
        thread.join();
    }
}

int ModelHander::init() { 
    // hash allocation slice for per node
    for(auto slice: slices_){
        slice_readers.emplace(slice->slice_no, ModelSliceReader());
        slice_readers[slice->slice_no].Load(slice->local_slice_path);
    }
    start();
    return 0;
}

void ModelHander::UnLoadAll() {
    for(auto &it: slice_readers) {
        it.second.Unload();
    }
}

void ModelHander::start() {
    for(int i = 0; i < n_thread_; ++i) {
        reader_threads.emplace_back([this]() {
            std::vector<RequestTask> tasks;
            RequestTask task;
            while (1) {
                {
                    std::unique_lock<std::mutex> lock(mu_);
                    cv_.wait(lock, [this] { 
                        return is_stop_ || !request_queues.empty(); 
                    });
                    if (is_stop_)
                        break;
                    // while(!request_queues.empty()) {
                    //     tasks.emplace_back(std::move(request_queues.front()));
                    //     request_queues.pop();
                    // }
                    task = std::move(request_queues.front());
                    request_queues.pop();
                }
                // std::cout << "process task number: " << batchs.size() << std::endl;
                processTask(task);
                // for(auto &task: tasks)
                //     processTask(task);
                // tasks.clear();
            }
        });
    }
} 

void ModelHander::processTask(RequestTask & tasks) {
    char buf[128];
    for(int i = 0; i < tasks.num; ++i) {
        SliceData& sd = tasks.req_slice_list[i];
        slice_readers[sd.slice_partition].Read(sd.start, sd.len, buf);
        memcpy(tasks.result->data + sd.pos , buf,  sd.len);
    }

    {
        std::unique_lock<std::mutex> lock(tasks.result->mu);
        tasks.result->finish_num +=  tasks.num;
        // std::cout << "ModelHander::processTask process: " << tasks.result->finish_num << std::endl;
        if(tasks.result->finish_num >= tasks.result->req_num){
            tasks.result->finish = true;
        }
    }   
    tasks.result->cv.notify_one();
}


void ModelHander::Push(RequestTask & req_task) {
    {
        std::unique_lock<std::mutex> lock(mu_);
        request_queues.push(std::move(req_task));
    }
    cv_.notify_all();
}

std::ostream& operator<<(std::ostream& os, const ModelHander& modelHander) {
    os << "ModelHander : {" <<  std::endl;
    os << '\t' <<" n_thread: " << modelHander.n_thread_ << std::endl;
    std::cout << '\t' << "slice: [";
    for(auto slice: modelHander.slices_) {
        std::cout << slice->slice_no <<", ";
    }
    os << "]"  << std::endl;
    os << "}"  << std::endl;
    return os;
}