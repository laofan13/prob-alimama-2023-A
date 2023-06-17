#include <string>
#include <memory>

#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "examples/protos/alimama.grpc.pb.h"
#else
#include "alimama.grpc.pb.h"
#endif

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using alimama::proto::ModelService;
using alimama::proto::SliceRequest;
using alimama::proto::Request;
using alimama::proto::Response;

int main() {
    std::string server_address_ = "172.19.0.7:50051";
        std::unique_ptr<ModelService::Stub> stub_ = 
            ModelService::NewStub(grpc::CreateChannel(server_address_, grpc::InsecureChannelCredentials()));
    Request request;
    Response resp;

    ClientContext context;

    int n = 1000;
    request.mutable_slice_request()->Reserve(n);
    for (int i = 0; i < n; ++i) {
        alimama::proto::SliceRequest* slice = request.add_slice_request();
        slice->set_slice_partition(i % 20);
        slice->set_data_start(i);
        slice->set_data_len(16);
    }

    // The actual RPC.
    Status status = stub_->Get(&context, request, &resp);

    if(resp.status() == -1) {
        std::cout <<"faild to request....." << std::endl;
        return 0;
    }

    for(int i = 0; i < resp.slice_data_size(); ++i) {
        std::cout << "slice request: " << i <<" slice_data: " << resp.slice_data(i) << std::endl;
    }
        
    // Act upon its status.
    if(!status.ok()) {
      std::cout << status.error_code() << " : " << status.error_message()
                << std::endl;
      return 0;
    }
}