#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <map>
#include <vector>

#ifdef BAZEL_BUILD
#include "examples/protos/alimama.grpc.pb.h"
#else
#include "internal.grpc.pb.h"
#endif

#include "model_hander.h"
#include "model_manger.h"
#include "memory_allocator.h"

using grpc::ServerContext;
using grpc::Status;

using internal::proto::InternalModelService;
using internal::proto::InternalSliceRequest;
using internal::proto::InternalRequest;
using internal::proto::InternalResponse;

class InternalModelServiceImpl final : public InternalModelService::Service {    
public:
    InternalModelServiceImpl(Options & options, std::string server_address, std::shared_ptr<ModelManger> model_manger);

    ~InternalModelServiceImpl();

    Status Get(ServerContext *context, const InternalRequest *req, InternalResponse *res) override;

private:
    Options options_;
    std::string server_address_;

    std::shared_ptr<ModelManger> model_mangerPtr;
    std::atomic<int> req_num = 0;
    MemoryAllocator allocator_;
    
};