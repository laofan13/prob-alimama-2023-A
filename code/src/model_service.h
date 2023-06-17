#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <map>

#include <etcd/Client.hpp>
#include <etcd/Response.hpp>
#include <vector>

#ifdef BAZEL_BUILD
#include "examples/protos/alimama.grpc.pb.h"
#else
#include "alimama.grpc.pb.h"
#endif

#include "config.h"
#include "utils.h"

#include "model_hander.h"
#include "model_manger.h"
#include "model_async_client.h"
#include "memory_allocator.h"

using grpc::ServerContext;
using grpc::Status;

using alimama::proto::ModelService;
using alimama::proto::SliceRequest;
using alimama::proto::Request;
using alimama::proto::Response;


class ModelServiceImpl final : public ModelService::Service {    
public:
    ModelServiceImpl(Options & options, std::string server_address, etcd::Client* etcd, std::shared_ptr<ModelManger> model_manger);

    ~ModelServiceImpl();

    int init();
    Status Get(ServerContext *context, const Request *req, Response *res) override;

private:
    Options options_;
    std::string server_address_;
    etcd::Client* etcd_;

    std::shared_ptr<ModelManger> model_mangerPtr;
    std::atomic<int> req_num = 0;
    MemoryAllocator allocator_;
    
    std::map<int, std::shared_ptr<ModelClient>> model_service_stubs;
};