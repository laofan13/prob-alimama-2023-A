#include "model_internal_server.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>

#include "model_hander.h"

InternalModelServiceImpl::InternalModelServiceImpl(
        Options & options, 
        std::string server_address, 
        std::shared_ptr<ModelManger> model_manger)
    : options_(options),
    server_address_(server_address),
    model_mangerPtr(std::move(model_manger))
    
{
    req_num = 0;
}

InternalModelServiceImpl::~InternalModelServiceImpl() {
    
}

Status InternalModelServiceImpl::Get(ServerContext *context, const InternalRequest *req, InternalResponse *res) {
    auto model_name = req->model_name();
    auto mode_hander = model_mangerPtr->getModelHander(model_name);

    // slice request
    int req_n = req->slice_request_size();
    RequestTask task;
    int size = 0;
    for (int i = 0; i < req_n; i++) {
        auto & sr = req->slice_request(i);
        SliceData rd{sr.slice_partition(), sr.data_start(), sr.data_len(), size};
        size +=  sr.data_len();
        task.req_slice_list.emplace_back(rd);
        task.num++;
    }
    auto result = std::make_shared<RequestResult>(req_num++, req_n, size);
    result->data = allocator_.allocate(size);
    task.result = result;

    mode_hander->Push(task);
        
    while(1) {
        std::unique_lock<std::mutex> lock(result->mu);
        result->cv.wait(lock, [&] {
            return result->finish || result->faild ;
        });
        if(result->finish || result->faild)
            break;
    }
    if(result->faild) {
        res->set_status(-1);
        return Status::OK;
    }
    // return result
    int pos = 0;
    res->mutable_slice_data()->Reserve(req_n);
    for(int i = 0; i < req_n; i++) {
        auto & sr = req->slice_request(i);
        int len = sr.data_len();
        res->add_slice_data(result->data + pos, len);
        pos += len;
    }
    allocator_.deallocate(result->data);

    // std::cout << "process a request successfully: " << result->req_no 
        // << "finish: " << result->finish_num << std::endl;

    res->set_status(0);
    return Status::OK;
}