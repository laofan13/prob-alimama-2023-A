#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include <bits/stdc++.h>

#include <cmath>
#include <etcd/Client.hpp>
#include <boost/lockfree/queue.hpp>
#include <grpcpp/grpcpp.h>
#include "alimama.grpc.pb.h"

#include "grpc_benchmark.h"

#define BOOST_LOG_DYN_LINK 1
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
namespace logging = boost::log;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using RequestPtr = std::shared_ptr<alimama::proto::Request>;
using ResponsePtr = std::shared_ptr<alimama::proto::Response>;
using alimama::proto::ModelService;
using alimama::proto::Request;
using alimama::proto::Response;

struct CustomSummary{
  int32_t total_num;
};
struct TestCasePair{
    bool repeat;
    RequestPtr req;
    ResponsePtr response;
};

using StubsVector=std::vector<std::unique_ptr<ModelService::Stub>>;
using ModelServiceGprcBenchmark = GrpcBenchmark<RequestPtr, ResponsePtr, CustomSummary, RequestPtr>;
using GrpcClientPtr = shared_ptr<GrpcClient<RequestPtr, ResponsePtr>>;

struct RequestItem {
  std::shared_ptr<ClientContext> ctx;
  void* obj;
  RequestPtr req;
  ResponsePtr resp;
  std::shared_ptr<Status> status;
};

class ModelServiceGprcClient : public GrpcClient<RequestPtr, ResponsePtr> {
private:
  StubsVector stubs_;
  std::vector<std::string> services_;
  shared_ptr<grpc::CompletionQueue> cq_;
  std::atomic<uint64_t> req_idx_;

  std::mutex mtx_;
  bool enable_;
public:
  static std::vector<shared_ptr<GrpcClient<RequestPtr, ResponsePtr>>> CreateClients(const std::vector<std::string>& services, uint32_t threads) {
    std::vector<shared_ptr<GrpcClient<RequestPtr, ResponsePtr>>> clients{};
    for(size_t i=0; i<threads; i++) {
      auto cli = std::make_shared<ModelServiceGprcClient>(services);
      if (!cli->Init()) {
        BOOST_LOG_TRIVIAL(error)  << "init clients failed";
        return std::vector<shared_ptr<GrpcClient<RequestPtr, ResponsePtr>>>{};
      }
      clients.push_back(cli);
    }
    return clients;
  };

  ModelServiceGprcClient(const std::vector<std::string> services):
    services_(services), req_idx_{0},enable_{false},cq_() {};
  ~ModelServiceGprcClient() {
  };

  bool Init() {
    for (const auto& svc : services_) {
      std::unique_ptr<ModelService::Stub> stub(ModelService::NewStub(grpc::CreateChannel(svc, grpc::InsecureChannelCredentials())));
      if (!stub) {
        BOOST_LOG_TRIVIAL(error)  << "failed to setup serach service , got nullptr ";
        return false;
      }
      stubs_.push_back(std::move(stub));
      auto q = std::make_shared<grpc::CompletionQueue>();
      cq_.swap(q);
    }
    enable_ = true;
    return true;
  }

  bool Request(std::shared_ptr<ClientContext> ctx, RequestPtr& req, void* obj) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!enable_) return false;
    auto idx = req_idx_.fetch_add(1);
    idx = idx % stubs_.size();
    std::unique_ptr<grpc::ClientAsyncResponseReader<Response> > rpc(
      stubs_[idx]->AsyncGet(ctx.get(), *req, cq_.get()));
    ResponsePtr resp = std::make_shared<Response>();
    auto status = std::make_shared<Status>();
    auto* item = new RequestItem{ctx, obj, req, resp, status};
    rpc->Finish(resp.get(), status.get(), (void*)item);
    return true;
  }

  bool WaitResponse(ResponsePtr& resp, std::shared_ptr<Status>& status, void** obj) {
    void * got;
    bool ok = false;
    bool success = cq_->Next(&got, &ok);
    if (!success) {
      return false;
    }
    if (!ok) {
      BOOST_LOG_TRIVIAL(warning) << "request !OK";
      // TODO
      return false;
    }
    RequestItem* got_item = (RequestItem*) got;
    if (!got_item) {
      BOOST_LOG_TRIVIAL(warning) << "request !got_item";
      return false;
    }
    if (obj) {
      *obj = got_item->obj;
    }
    resp.swap(got_item->resp);
    status.swap(got_item->status);
    delete got_item;
    return true;
  }
  bool Close() {
    std::lock_guard<std::mutex> lock(mtx_);
    cq_->Shutdown();
    enable_ = false;
    return true;
  }

  ModelServiceGprcClient(const ModelServiceGprcClient&) = delete;
  ModelServiceGprcClient(ModelServiceGprcClient&&) = delete;
  ModelServiceGprcClient& operator=(const ModelServiceGprcClient&) = delete;
  ModelServiceGprcClient& operator=(ModelServiceGprcClient&&) = delete;
};

void dump_summary(const ModelServiceGprcBenchmark::SummaryType& summary, double qps) {
  BOOST_LOG_TRIVIAL(info)  << "summary completed_requests " << summary.completed_requests;
  BOOST_LOG_TRIVIAL(info)  << "summary avg_latency_ms " << summary.avg_latency_ms;
  BOOST_LOG_TRIVIAL(info)  << "summary qps " << qps;
  BOOST_LOG_TRIVIAL(info)  << "summary error_request_count " << summary.error_request_count;
  BOOST_LOG_TRIVIAL(info)  << "summary success_request_count " << summary.success_request_count;
  BOOST_LOG_TRIVIAL(info)  << "summary timeout_request_count " << summary.timeout_request_count;
}

bool compare_result_dummy(const ResponsePtr& resp, const RequestPtr& ref, CustomSummary& result) {
  return true;
}

bool compare_result(const RequestPtr& req, const ResponsePtr& resp, const RequestPtr& ref, CustomSummary& result) {
  if (!resp) {
    BOOST_LOG_TRIVIAL(warning)  << "resp is null ";
    return true;
  }
  if (!ref) {
    BOOST_LOG_TRIVIAL(warning)  << "ref is null ";
    return true;
  }
  result.total_num ++;
  return true;
}
struct TestMaxQpsConfig {
  int32_t qps_baseline = 2000;
  int32_t thread_num = 6;
  int32_t timeout_ms = 10;
  int32_t request_duration_each_iter_sec = 10;
  int32_t max_iter_times = 50;
  double success_percent_th = 0.99;
  double qps_step_size_percent = 0.1;
}; 

ModelServiceGprcBenchmark::SummaryType TestMaxQps(std::vector<std::string> services, const TestMaxQpsConfig& cfg, double& max_qps) {
  double qps_limit = cfg.qps_baseline;
  double last_qps = 0;
  max_qps = 0;

  ModelServiceGprcBenchmark::SummaryType last_summary;
  for (size_t i = 0; i<cfg.max_iter_times; ++i) {
    auto clis = ModelServiceGprcClient::CreateClients(services, cfg.thread_num);
    if (clis.size() == 0) return last_summary;

    ModelServiceGprcBenchmark bench(clis, compare_result_dummy, cfg.timeout_ms, int32_t(qps_limit));
    int64_t request_times = qps_limit * cfg.request_duration_each_iter_sec;
    auto start = std::chrono::steady_clock::now();
    double elapsedtime_popdata_ms_all = 0;
    for (size_t j = 0; j < request_times; ++j) {
      auto popdata = std::chrono::steady_clock::now();
      auto req = std::make_shared<alimama::proto::Request>();
      // pair.response = std::make_shared<alimama::proto::Response>();
      req->mutable_slice_request()->Reserve(1000);
      for (int i = 0; i < 1000; ++i) {
        alimama::proto::SliceRequest* slice = req->add_slice_request();
        slice->set_slice_partition(i);
        slice->set_data_start(i);
        slice->set_data_len(16);
      }
      // pair.response = std::make_shared<alimama::proto::Response>();
      auto popdataend = std::chrono::steady_clock::now();
      elapsedtime_popdata_ms_all += std::chrono::duration<double, std::milli>(popdataend - popdata).count();
      if (!bench.Request(i, req, req)) {
        BOOST_LOG_TRIVIAL(error)  << "request data failed";
        break;
      }
    }
    auto not_timeout = bench.WaitAllUntilTimeout(cfg.request_duration_each_iter_sec * 1000);
    auto end = std::chrono::steady_clock::now();
    auto elapsedtime_ms = std::chrono::duration<double, std::milli>(end - start).count();

    auto summary = bench.Summary();
    double QPS = summary.success_request_count / (elapsedtime_ms / 1000);
    BOOST_LOG_TRIVIAL(info)  << "timeout: " << !not_timeout << " elapsedtime_ms: " << elapsedtime_ms << " elapsedtime_popdata_ms " << elapsedtime_popdata_ms_all << " QPS " << QPS ;

    last_qps = QPS;
    max_qps = last_qps > max_qps ? last_qps : max_qps;
    last_summary = summary;

    if (summary.success_request_percent < cfg.success_percent_th) {
      qps_limit = qps_limit - qps_limit * cfg.qps_step_size_percent;
    } else {
      qps_limit = qps_limit + qps_limit * cfg.qps_step_size_percent;
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(60*1000));
    dump_summary(last_summary, last_qps);
  }

  dump_summary(last_summary, last_qps);
  return last_summary;
}

std::vector<std::string> setupModelService() {
  std::vector<std::string> services{};
  etcd::Client etcd("http://etcd:2379");
  std::string prefix = "/services/modelservice/";
	etcd::Response response = etcd.keys(prefix).get();
  if (response.is_ok()) {
      BOOST_LOG_TRIVIAL(info) << "etcd connected successful.";
  } else {
      BOOST_LOG_TRIVIAL(info) <<  "etcd connected failed: " << response.error_message();
      return services;
  }
  for (size_t i = 0; i < response.keys().size(); i++) {
    std::string server_address = std::string(response.key(i)).substr(prefix.size());
    BOOST_LOG_TRIVIAL(info)  << "found server_address " << server_address;
    services.push_back(server_address);
  }
  return services;
}
int main() {
  logging::core::get()->set_filter(logging::trivial::severity >= logging::trivial::info);
  BOOST_LOG_TRIVIAL(info)  << "TestMaxQps ";
  auto services = setupModelService();
  double max_qps = 0;
  GrpcClientPtr cli = std::make_shared<ModelServiceGprcClient>(services);
  TestMaxQpsConfig cfg;
  auto summary = TestMaxQps(services, cfg, max_qps);
  BOOST_LOG_TRIVIAL(info)  << "max_qps " << max_qps;
}
