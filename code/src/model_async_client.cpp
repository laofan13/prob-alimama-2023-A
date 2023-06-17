#include "model_async_client.h"

#include <iostream>

ModelClient::ModelClient(int node_id, int node_num, std::shared_ptr<Channel> channel_)
    : node_id_(node_id),
    node_num_(node_num),
    stub_(InternalModelService::NewStub(channel_))
{

}

ModelClient::~ModelClient() {
    if(bg_thread != nullptr) {
        if(bg_thread->joinable())
            bg_thread->join();
        delete bg_thread;
    }
}

void ModelClient::init() {
    bg_thread = new std::thread([this]  {
        AsyncCompleteRpc();
    });
}

void ModelClient::Push(RequestTask & tasks){
    if(tasks.num <= 0)
        return;
    // std::cout << "node_" << node_id_ << "push num: " << tasks.num << std::endl;
    InternalRequest request;
    request.set_model_name(tasks.model_name);
    request.mutable_slice_request()->Reserve(tasks.num);
    for (int i = 0; i < tasks.num; ++i) {
        SliceData& sd = tasks.req_slice_list[i];
        auto slice = request.add_slice_request();
        slice->set_slice_partition(sd.slice_partition);
        slice->set_data_start(sd.start);
        slice->set_data_len(sd.len);
    }

    AsyncClientCall* call = new AsyncClientCall;
    call->tasks = std::move(tasks);

    call->response_reader = stub_->PrepareAsyncGet(&call->context, request, &cq_);
    call->response_reader->StartCall();
    call->response_reader->Finish(&call->response, &call->status, (void*)call);
}

void ModelClient::AsyncCompleteRpc() {
    void* got_tag;
    bool ok = false;
    // Block until the next result is available in the completion queue "cq".
    while (cq_.Next(&got_tag, &ok)) {
        // The tag in this example is the memory location of the call object
        AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);

        // Verify that the request was completed successfully. Note that "ok"
        // corresponds solely to the request for updates introduced by Finish().
        GPR_ASSERT(ok);

        HandleResponse(call->tasks, &call->response, call->status);

        // Once we're complete, deallocate the call object.
        delete call;
    }
}

void ModelClient::HandleResponse(RequestTask & tasks, InternalResponse* response, Status status) {
    // std::cout << "node_" << node_id_ << " HandleResponse num: " << tasks.num << std::endl;
    if (!status.ok()) {
        std::cout << status.error_code() << ": " << status.error_message() << std::endl;
        {
            std::unique_lock<std::mutex> lock(tasks.result->mu);
            tasks.result->faild = true;
        }   
        tasks.result->cv.notify_one();
        return;
    }

    if(response->slice_data_size() < tasks.num) {
        {
            std::unique_lock<std::mutex> lock(tasks.result->mu);
            tasks.result->faild = true;
        }   
        tasks.result->cv.notify_one();
        return;
    }

    for(int i = 0; i < tasks.num; ++i) {
        SliceData& sd = tasks.req_slice_list[i];
        memcpy(tasks.result->data + sd.pos , response->slice_data(i).c_str(),  sd.len);
    }

    {
        std::unique_lock<std::mutex> lock(tasks.result->mu);
        tasks.result->finish_num +=  tasks.num;
        if(tasks.result->finish_num >= tasks.result->req_num)
            tasks.result->finish = true;
    }   
    tasks.result->cv.notify_one();
}