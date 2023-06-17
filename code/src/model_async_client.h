#pragma once

#include <string>
#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "examples/protos/alimama.grpc.pb.h"
#else
#include "internal.grpc.pb.h"
#endif

#include "model_hander.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;

using internal::proto::InternalModelService;
using internal::proto::InternalSliceRequest;
using internal::proto::InternalRequest;
using internal::proto::InternalResponse;

class ModelClient {
public:
    ModelClient(int node_id, int node_num, std::shared_ptr<Channel> channel_);

    ~ModelClient();

    void init();
    void Push(RequestTask & tasks);

private:
    void AsyncCompleteRpc();
    void HandleResponse(RequestTask & tasks, InternalResponse* response, Status status);

private:
    int node_id_;
    int node_num_;
    std::string server_address_;

    struct AsyncClientCall {
        RequestTask tasks;
        InternalResponse response;
        ClientContext context;
        Status status;
        std::unique_ptr<ClientAsyncResponseReader<InternalResponse>> response_reader;
    };
    CompletionQueue cq_;


    std::thread* bg_thread;
    std::unique_ptr<InternalModelService::Stub> stub_;
};