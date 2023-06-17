/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#ifdef BAZEL_BUILD
#include "examples/protos/alimama.grpc.pb.h"
#else
#include "alimama.grpc.pb.h"
#endif

#include "config.h"
#include "utils.h"
#include "model_manger.h"
#include "model_service.h"
#include "model_internal_server.h"

using grpc::Server;
using grpc::ServerBuilder;

void RunServer() {
    // Options options = {
    //     .node_id = 0,
    //     .node_num = 1,
    //     .cpu = 2,
    //     .mempry = 4294967296
    // };
    // config information
    Options options = loadENV();
    std::string server_address = getLocalIP() + ":50051";
    std::cout << options << std::endl;

    etcd::Client etcd(ECTD_URL);

    auto ModelMangerPtr = std::make_shared<ModelManger>(options, &etcd);
    if(ModelMangerPtr->init() != 0) {
        std::cout << "ModelManger faild to init ...." << std::endl;
        return ;
    }
    InternalModelServiceImpl internalService(options, server_address, ModelMangerPtr);

    ModelServiceImpl service(options, server_address, &etcd, ModelMangerPtr);
    if(service.init() != 0) {
        std::cout << "ModelService faild to init ...." << std::endl;
        return ;
    }

    
    // grpc service
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;

    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    builder.RegisterService(&internalService);
    
    // Finally assemble the server.
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}