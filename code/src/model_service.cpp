#include "model_service.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <thread>
#include <type_traits>

#include "config.h"
#include "model_hander.h"
#include "utils.h"

ModelServiceImpl::ModelServiceImpl(
        Options & options, 
        std::string server_address, 
        etcd::Client* etcd,
        std::shared_ptr<ModelManger> model_manger)
    : options_(options),
    server_address_(server_address),
    etcd_(etcd),
    model_mangerPtr(std::move(model_manger))
    
{
    req_num = 0;
}


ModelServiceImpl::~ModelServiceImpl () {
    std::string key = modelservice_dir + server_address_;
    etcd_->rm(key);
}

int ModelServiceImpl::init() {
    std::cout << "ModelService, Node_"<< options_.node_id << " start init ......." << std::endl;

    std::thread([this] {
        std::chrono::seconds retry_delay(1);
        std::string key = internalservice_dir + server_address_;
        while(1) {
            pplx::task<etcd::Response> response_task = etcd_->set(key, std::to_string(options_.node_id));
            etcd::Response response = response_task.get();
            if (response.is_ok()){
                break;
            }
            std::this_thread::sleep_for(retry_delay);
        }   

        std::cout << "node_"<< options_.node_id 
            << " try found other service" << std::endl;
        while(1) {
            etcd::Response resp = etcd_->ls(internalservice_dir).get();
            if (!resp.is_ok() || resp.keys().size() != options_.node_num) {
                continue;
            }
            for (size_t i = 0; i < resp.keys().size(); i++) {
                int node = std::stoi(resp.value(i).as_string());
                std::string server_address = std::string(resp.key(i)).substr(internalservice_dir.size());
                if(node != options_.node_id && model_service_stubs.find(node) == model_service_stubs.end()) {
                    std::cout << "try connect grpc service, node: " << node 
                        << " server_address" << server_address << std::endl;
                    std::shared_ptr<Channel> channel = grpc::CreateChannel(
                        server_address, grpc::InsecureChannelCredentials());
                    gpr_timespec tm_out{ 3, 0, GPR_TIMESPAN };
                    if(!channel->WaitForConnected(gpr_time_add(
                            gpr_now(GPR_CLOCK_REALTIME),
                            gpr_time_from_seconds(60, GPR_TIMESPAN)))) {
                        std::cout << "faild to connect grpc server: " << server_address << std::endl;
                        continue;
                    }
                    auto stub = std::make_shared<ModelClient>(node, options_.node_num, channel);
                    stub->init();
                    model_service_stubs[node] = stub;
                    std::cout << "rigister service node_" << node  << " successfully ...." << std::endl;
                }
            }
            if(model_service_stubs.size() == options_.node_num - 1) {
                std::cout << "connect to all gprc server sucessfully..." << std::endl;
                break;
            }
        }   

        std::cout << "node_"<< options_.node_id 
            << " start try register service in etcd, server_address: " 
            << server_address_ << std::endl;
        key = modelservice_dir + server_address_;
        while(1) {
            pplx::task<etcd::Response> response_task = etcd_->set(key, std::to_string(options_.node_id));
            etcd::Response response = response_task.get();
            if (response.is_ok()){
                break;
            }
        }
        std::cout << "node_" << options_.node_id  
            << " register service sucessfully in etcd" << std::endl;
    }).detach();
       
    return 0;
}

Status ModelServiceImpl::Get(ServerContext *context, const Request *req, Response *res) {
    auto mode_hander = model_mangerPtr->getModelHander();
    std::string model_name = mode_hander->model_name();

    // slice request
    int req_n = req->slice_request_size();
    std::vector<RequestTask> requestList(options_.node_num);
    int size = 0;
    for (int i = 0; i < req_n; i++) {
        auto & sr = req->slice_request(i);
        SliceData rd{sr.slice_partition(), sr.data_start(), sr.data_len(), size};
        size +=  sr.data_len();
        
        int node_idx = model_mangerPtr->getNodeForSlice(sr.slice_partition());
        requestList[node_idx].req_slice_list.emplace_back(rd);
        requestList[node_idx].num++;
    }
    auto result = std::make_shared<RequestResult>(req_num++, req_n, size);
    result->data = allocator_.allocate(size);

    for(auto & task: requestList) {
        task.result = result;
        task.model_name = model_name;
    }
        

    for(int node_idx = 0; node_idx < options_.node_num; ++node_idx){
        // std::cout << "RequestTask size:" << requestList[node_idx].num << std::endl;
        if(node_idx == options_.node_id) {
            mode_hander->Push(requestList[node_idx]);
        }else{
            model_service_stubs[node_idx]->Push(requestList[node_idx]);
        }
    }

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