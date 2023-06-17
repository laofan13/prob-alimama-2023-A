#include <cassert>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <iterator>
#include <ostream>
#include <sstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <list>
#include <filesystem>
#include <iomanip>
#include <chrono>

#include "hdfs.h" 
#include "ModelSliceReader.h"

const std::string HDFS_ADDRES = "hdfs://namenode:9000";
const std::string MODEL_ROOT_DIR = "/";
const std::string MODEL_PREFIX = "model_";
const uint32_t MODEL_NAME_LEN = 25;
const int BUFFER_SIZE = 65536;
const u_int64_t MEMORY = 16777216000;
const u_int64_t switch_time_limit = 300;
// const std::string model_file = "model.meta";

struct SliceMeta {
    std::string slice_name;
    std::string local_slice_path;
    int slice_no;
    u_int64_t size;
    bool is_load;
};

std::ostream& operator<<(std::ostream& os, SliceMeta& slice) {
    os << "slice_name: " << slice.slice_name << " | "
        << "slice_no: " << slice.slice_no << " | "
        << "local_slice_path: " << slice.local_slice_path << " | "
        << "size: " << slice.size << " | "
        << "is load: " << slice.is_load;
    return os;
}

struct ModelMeta {
    std::string model_name;
    std::string model_path;
    int slice_num;
    int local_slice_num;
    int load_num; 
    bool load_;
    std::vector<SliceMeta*> slices;

    std::mutex mu_;
    std::condition_variable cv_;
};

std::ostream& operator<<(std::ostream& os, ModelMeta& model) {
    os << "model_name: " << model.model_name << " | "
        << "model_path: " << model.model_path << " | "
        << "slice_num: " << model.slice_num<< " | "
        << "local_slice_num: " << model.local_slice_num<< " | "
        << "load_num: " << model.load_num << std::endl;
    os << "model meta:" << std::endl;
    for(auto slice: model.slices)
        os << *slice << std::endl;
    return os;
}


class HDFSHander {
public:
    HDFSHander(int node_id, int node_num, std::string hdfs_addres, std::string model_root_dir, std::string local_root_dir = "/tmp/")
        : node_id_(node_id),
         node_num_(node_num),
         is_init_(false),
         hdfs_addres_(hdfs_addres),
         model_root_dir_(model_root_dir),
         local_root_dir_(local_root_dir),
         is_stop_(false) {

    }

    HDFSHander(HDFSHander &) = delete;
    HDFSHander& operator=(HDFSHander &) = delete;

    int init() {
        fs_ = hdfsConnect(hdfs_addres_.c_str(), 0);
        if(nullptr == fs_)
            return -1;
        if(hdfsExists(fs_, model_root_dir_.c_str()) != 0) 
            return -1;
        is_init_ = true;
        run();
        return 0;
    }

    // download hdfs slice to local /tmp/model_...
    void loadSliceData(ModelMeta * modelMteta, SliceMeta *slice) {
        // std::cout << *slice << std::endl;
        // std::cout << "node_id: " << node_id_ << " | "<< "node_num:"<< node_num_ << std::endl;
        auto hdf_slice_path = modelMteta->model_path + "/" + slice->slice_name;
        // open hdfs slice file : model_.../model_slice.0;
        hdfsFile sliceFile = hdfsOpenFile(fs_, hdf_slice_path.c_str(), O_RDONLY, 0, 0, 0);
        if(!sliceFile) {
            fprintf(stdout, "Failed to open %s for slice \n", hdf_slice_path.c_str());
            hdfsCloseFile(fs_, sliceFile);
            return ;
        }
        // open local slice file : model_.../model_slice.0;
        std::ofstream local_slice_file(slice->local_slice_path, std::ios::binary);
        if (!local_slice_file.is_open()) {
            fprintf(stdout, "Failed to open %s for local slice file\n", slice->local_slice_path.c_str());
            hdfsCloseFile(fs_, sliceFile);
            local_slice_file.close();
            return;
        }

        // download file content to local
        char * buf = new char[BUFFER_SIZE];
        tSize num_read_bytes = 0;
        while((num_read_bytes = hdfsRead(fs_, sliceFile, buf, BUFFER_SIZE)) != 0) {
            local_slice_file.write(buf, num_read_bytes);
        }
        delete[] buf;

        {
            std::unique_lock<std::mutex> lock(modelMteta->mu_);
            slice->is_load = true;
            modelMteta->load_num++;
            modelMteta->cv_.notify_all();
        }
        
        hdfsCloseFile(fs_, sliceFile);
        local_slice_file.close();
    }

    void loadModelData(ModelMeta * modelMteta) {
        // const auto& model_path = modelMteta->model_path;
        std::string local_model_path = local_root_dir_ + modelMteta->model_name;

        // if local model dir exsit 
        if (!std::filesystem::exists(local_model_path)) {
            if(!std::filesystem::create_directory(local_model_path)) {
                std::cout << "faild to create local directory :" << local_model_path << std::endl;
                return ;
            }
        }
        // load slice data from hdfs
        for(auto slice: modelMteta->slices) {
            if(slice->slice_no % node_num_ != node_id_)
                continue;
            loadSliceData(modelMteta, slice);
        }
        std::unique_lock<std::mutex> lock(modelMteta->mu_);
        modelMteta->load_ = true;
        modelMteta->cv_.notify_all();
    }

    void run() {
        io_thread = new std::thread([this]  {
            while (1) {
                ModelMeta * modelMteta = nullptr;
                {
                    std::unique_lock<std::mutex> lock(this->io_mu);
                    this->io_cv.wait(lock,
                            [this] { return this->is_stop_ || !this->io_tasks.empty(); });
                    if (this->is_stop_ && this->io_tasks.empty())
                        break;
                    modelMteta = std::move(io_tasks.front());
                    io_tasks.pop();
                }
                loadModelData(modelMteta);
            }
        });
    }

    bool modelAvailable(std::string model_name) {
        // if model_ exsit
        std::string model_path = model_root_dir_ + model_name;
        if(hdfsExists(fs_, model_path.c_str()) != 0) {
            return false;
        }
        // std::cout << model_path << " exsit" <<std::endl;
        // if model.done exsit
        std::string model_done_path = model_path + "/model.done";
        if(hdfsExists(fs_, model_done_path.c_str()) == 0) {
            std::cout << model_done_path << " exsit: model invalid...." << std::endl;
            return false;
        }
        
        return true;
    }

    std::string getMainModel() {
        if(!is_init_)
            return "";
        char buf[128];
        // if rollback.version exsit
        std::string rollback_version = model_root_dir_ + "rollback.version";
        if(hdfsExists(fs_, rollback_version.c_str()) == 0) {
            hdfsFile rollbackFile = hdfsOpenFile(fs_, rollback_version.c_str(), O_RDONLY, 0, 0, 0);
            if(rollbackFile) {
                tSize num_read_bytes = hdfsRead(fs_, rollbackFile, &buf, MODEL_NAME_LEN);
                buf[MODEL_NAME_LEN] = '\0';
                std::cout << "rollback.version: " << buf << std::endl; 
                if(modelAvailable(buf)) {
                    std::cout << "start rollback version:" << buf << std::endl; 
                    hdfsCloseFile(fs_, rollbackFile);
                    return buf;
                }
            }
            hdfsCloseFile(fs_, rollbackFile);
        }

        // list of files/directories
        int numEntries = 0;
        hdfsFileInfo* hdfsFileInfolist = hdfsListDirectory(fs_, model_root_dir_.c_str(), &numEntries);
        for(int i = numEntries-1; i >= 0; --i) {
            std::string model_path(hdfsFileInfolist[i].mName);
            u_int32_t last_slash_pos = model_path.find_last_of('/');
            std::string model_name = model_path.substr(last_slash_pos + 1);
            // if file name prefix == 'model_'
            if(model_name.compare(0, MODEL_PREFIX.size(), MODEL_PREFIX) == 0) {
                // std::cout << "found a model: " << model_name <<std::endl;
                if(modelAvailable(model_name)) {
                    // std::cout << "found available model:" << model_name << std::endl; 
                    return model_name;
                }
            }
            
        }
        std::cout << "not found available model" << std::endl;
        return "";
    }

    ModelMeta* loadModelMeta(const std::string & model_name) {
        // if model exsit
        std::string model_path = model_root_dir_ + model_name;
        if(hdfsExists(fs_, model_path.c_str()) != 0) {
            std::cout << "model " << model_name << "not exsit" << std::endl; 
            return nullptr;
        }

        ModelMeta* modelMeta = new ModelMeta();
        modelMeta->model_name = model_name;
        modelMeta->model_path = model_path;
        modelMeta->load_num = 0;
        modelMeta->local_slice_num = 0;
        modelMeta->load_ = false;

        // read model.meta
        std::ostringstream oss;
        {
            std::string meta_path = model_path + "/model.meta";
            hdfsFile metaFile = hdfsOpenFile(fs_, meta_path.c_str(), O_RDONLY, 0, 0, 0);
            if(!metaFile) {
                std::cout << "Failed to open model.meta: " << meta_path << std::endl; 
                hdfsCloseFile(fs_, metaFile);
                return nullptr;
            }
            char buf[512];
            tSize num_read_bytes = 0;
            while((num_read_bytes = hdfsRead(fs_, metaFile, &buf, 256)) != 0)  {
                buf[num_read_bytes] = '\0';
                oss << buf;       
            }
            hdfsCloseFile(fs_, metaFile);
        }
        
        // std::cout << "metaFile content: " << std::endl; 

        int slice_num = 0;
        std::stringstream iss(oss.str());
        std::string line;
        while (std::getline(iss, line)) {
            if(line.compare(0, 5, "slice") == 0) {
                // std::cout << line << std::endl;
                int first_comma_pos = line.find_first_of(',');
                SliceMeta* sliceMeta = new SliceMeta();
                sliceMeta->slice_name = line.substr(6, first_comma_pos - 6);
                sliceMeta->slice_no = slice_num++;
                sliceMeta->size = std::stoi(line.substr(first_comma_pos + 6));
                sliceMeta->local_slice_path = local_root_dir_ + model_name + "/" + sliceMeta->slice_name;
                sliceMeta->is_load = false;

                modelMeta->slices.push_back(sliceMeta);
                // std::cout << *sliceMeta;
            }
        }
        modelMeta->slice_num = slice_num;
        modelMeta->local_slice_num = slice_num / node_num_ + ( slice_num % node_num_ <= node_id_ ? 0 : 1);
        model_tables[model_name] = modelMeta;

        // TODO
        // ayasc dowload slice file
        {
            std::unique_lock<std::mutex> lock(io_mu);
            io_tasks.push(modelMeta);
            io_cv.notify_one();
        }

        return modelMeta;
    }

    ModelMeta* getLastestModel() {
        std::string model_name = getMainModel();
        if(model_name.empty() || model_name == "")
            return nullptr;
        if(model_tables.find(model_name) != model_tables.end()) {
            // std::cout << "model " << model_name << " is alreadly exist" << std::endl; 
            return model_tables[model_name];
        }
        return loadModelMeta(model_name);
    }

    ~HDFSHander() {
        {
            std::unique_lock<std::mutex> lock(io_mu);
            is_stop_ = true;
        }
        io_cv.notify_all();
        if(io_thread && io_thread->joinable()) {
            io_thread->join();
        }
        delete io_thread;
        for(auto ele: model_tables) {
            auto metaModel = ele.second;
            for(auto slice: metaModel->slices)
                delete slice;
            delete metaModel;
        }
    }

private:
    // node information
    int node_id_;
    int node_num_;

    // metadata
    bool is_init_;
    std::string hdfs_addres_;
    std::string model_root_dir_;
    std::string local_root_dir_;
    hdfsFS  fs_;

    // hdfs io thread;
    bool is_stop_;
    std::thread* io_thread;
    std::queue<ModelMeta *> io_tasks;
    std::mutex io_mu;
    std::condition_variable io_cv;

    std::map<std::string, ModelMeta*> model_tables;
};

class ModelSliceHander {
public:
    ModelSliceHander(std::string& name, ModelMeta * model, SliceMeta * slice)
        : name_(name),
        model_(model),
        slice_(slice) {
        is_load_ = false;
        ref_ = 0;
    }

    bool Load() {
        // wait local slice file load complete
        if(!is_load_)
            return sliceReader_.Load(slice_->local_slice_path);
        is_load_ = true;
        return true;
    }

    bool Unload() {
        if(is_load_)
            return sliceReader_.Unload();
        is_load_ = false;
        return true;
    }

    bool Read(size_t data_start, size_t data_len, char * data_buffer) {
        return sliceReader_.Read(data_start, data_len, data_buffer);
    }

    const std::string & getName() const{
        return name_;
    }

    const ModelMeta * getModelMeta() const{
        return model_ ;
    }

    const SliceMeta * getModelSlice() const{
        return slice_;
    }

    void setModelSlice(std::string& name) {
        name_ = name;
    }

    void setModelMeta(ModelMeta * model) {
        model_ = model;
    }

    void setModelSlice(SliceMeta * slice) {
        slice_ = slice;
    }

    int getRef() {
        return ref_.load(std::memory_order_acquire);
    }

    void incrRef() {
        ref_.fetch_add(1, std::memory_order_release);
    }

    void decrRef() {
        ref_.fetch_add(-1, std::memory_order_release);
    }

    void clear() {
        name_ = "",
        model_ = nullptr;
        slice_ = nullptr;
        ref_ = 0;
        Unload();
    }
    
private:
    std::string name_;
    bool is_load_;
    ModelMeta * model_;
    SliceMeta * slice_;

    std::atomic<int> ref_;
    ModelSliceReader sliceReader_;
};

class ModelHander {
public:
    ModelHander(int node_id, int node_num)
        : node_id_(node_id),
          node_num_(node_num),
          hdfsHanderPtr(std::make_unique<HDFSHander>(node_id, node_num, HDFS_ADDRES, MODEL_ROOT_DIR)) 
    {
        is_init_ = false;
        is_stop_ = false;
        use_memory_ = 0;
    } 

    ModelHander(ModelHander &) = delete;
    ModelHander& operator=(ModelHander &) = delete;

    void init() {
        hdfsHanderPtr->init();
        is_init_ = true;
        run();
    }

    void run() {
        th_ = new std::thread([this]  {
            while (1) {
                std::unique_lock<std::mutex> lock(mu_);
                if (this->is_stop_)
                    break;
                lock.unlock();

                // load model 
                auto model = hdfsHanderPtr->getLastestModel();

                if(model) {
                    if(version_lists.empty() || version_lists.back()->model_name.compare(model->model_name) != 0) {
                        auto load_time = std::chrono::system_clock::now();
                        // TODO 
                        // wait background io thread download slice file from hdfs
                        while(1) {
                            std::unique_lock<std::mutex> load_lock(model->mu_);

                            if(model->load_ || (double)model->load_num / model->local_slice_num > 0.75) {
                                std::cout << "model:" << model->model_name << " slice file alreadly load more then 3/4" << std::endl;
                                break;
                            }
                            
                            model->cv_.wait(load_lock,[&] {
                                return (double)model->load_num / model->local_slice_num > 0.5;
                            });

                            // model->cv_.wait_for(load_lock,std::chrono::seconds(switch_time_limit) ,[&] {
                            //     return (double)model->load_num / model->local_slice_num > 0.5;
                            // });

                            auto end = std::chrono::system_clock::now();
                            auto duration_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - load_time);
                            auto seconds = duration_in_ms.count() / 1000.0;
                            if(seconds > switch_time_limit) {
                                std::cout << "more then max switch_time_limit: " << std::endl;
                                break;
                            }
                        }

                        ModelMeta * target = nullptr;
                        for(auto ele: version_lists) {
                            if(ele->model_name.compare(model->model_name) == 0) {
                                target = ele;
                                break;
                            }
                        }
                        lock.lock();
                        if(target) 
                            version_lists.remove(target);
                        version_lists.push_back(model);
                        lock.unlock();
                    }
                }
                sleep(5);
            }
        });
    }

    ModelMeta* getMainModelMeta() {
        std::unique_lock<std::mutex> lock(mu_);
        return version_lists.empty() ? nullptr : version_lists.back();
    }

    ModelSliceHander* getSliceReader(ModelMeta* model, int slice_partition) {
        if(slice_partition >= model->slice_num || (slice_partition % node_num_ != node_id_))
            return nullptr;
        std::string slice_name = model->model_name + "_slice_" + std::to_string(slice_partition);

        // if slice_tables exist
        std::unique_lock<std::mutex> lock(slice_mu_);
        if(slice_tables.find(slice_name) != slice_tables.end()) {
            std::cout << "found a slice reader { " << slice_name << " } in cache" << std::endl;
            auto sliceHander = slice_tables[slice_name];
            // {
            //     slice_caches.remove(sliceHander);
            //     slice_caches.push_back(sliceHander);
            // }
            // sliceHander->Load();
            return sliceHander;
        }
        lock.unlock();

        // TODO
        auto slice = model->slices[slice_partition];

        // TODO if slice is alreadly load
        // {
        //     while(1) {
        //         std::unique_lock<std::mutex> load_lock(model->mu_);
        //         if(slice->is_load)
        //             break;
        //         std::cout << "slice { " << slice_name << " } don't be load" << std::endl;
        //         model->cv_.wait(load_lock, [&] {
        //             return slice->is_load;
        //         });
        //     }
        // }

        //memory usage:
        // std::cout << "memory usage :" << use_memory_ << std::endl;
        // std::cout << "memory usage :" << std::setprecision(2) << (use_memory_ / MEMORY) * 100 << "%" << std::endl;


        // remove a ModelSliceHander from caches
        // TODO posible other client use the ModelSliceHander;
        ModelSliceHander* sliceHander = nullptr;
        if(use_memory_ + slice->size > MEMORY) {
            std::cout << "memory usage more then max memory limit: " << use_memory_ << std::endl;
            // if being used.
            lock.lock();
            for(auto it: slice_caches) {
                if(it->getRef() == 0) 
                    sliceHander = it;
            }
            // memory 100% , not find available
            if(sliceHander == nullptr) {
                std::cout << "not foun a available slice Hander" << std::endl;
                return nullptr;
            }
            
            std::cout << "evict a available slice Hander {" << sliceHander->getName() <<"} from cache" << std::endl;
                
            slice_caches.remove(sliceHander);
            slice_tables.erase(sliceHander->getName());
            use_memory_ -= sliceHander->getModelSlice()->size;
            lock.unlock();
        }
        
        // sliceHander
        if(sliceHander) {
            sliceHander->clear();
        }else{
            sliceHander = new ModelSliceHander(slice_name, model, slice);
        }
        sliceHander->Load();

        lock.lock();
        use_memory_ += slice->size;
        slice_caches.push_back(sliceHander);
        slice_tables[slice_name] = sliceHander;
    
        return sliceHander;
    }

    ~ModelHander() {
        {
            std::unique_lock<std::mutex> lock(mu_);
            is_stop_ = true;
        }
        if(th_ && th_->joinable()) {
            th_->join();
        }
        delete th_;
        for(auto slice: slice_caches)
            delete slice;
        
    }

private:
    // node information
    int node_id_;
    int node_num_;
    std::unique_ptr<HDFSHander> hdfsHanderPtr;

    //
    bool is_init_;
    bool is_stop_;
    std::thread* th_;
    std::mutex mu_;
    std::list<ModelMeta *> version_lists;

    // ModelSliceReader list
    u_int64_t use_memory_;
    std::list<ModelSliceHander *> slice_caches;
    std::map<std::string, ModelSliceHander*> slice_tables;
    std::mutex slice_mu_;
    // std::condition_variable slice_cv_;
};

void test_HdfsHander() {
    // HDFSHander hdfsHander(0, 2, HDFS_ADDRES, MODEL_ROOT_DIR);
    // hdfsHander.init();

    // auto model = hdfsHander.getLastestModel();
    // if(!model) {
    //     return 0;
    // }
    // std::cout << "load before: "<<*model << std::endl;
    // int load_num = model->load_num.load();
    // while((load_num = model->load_num.load()) < model->local_slice_num) {
    //     std::cout << "wait slice download ...." << std::endl;
    //     std::cout << "hdfs alreadly download slice: " << load_num << std::endl;
    //     sleep(1);
    // }
    // std::cout << "load after: "<<*model << std::endl;
        // std::cout << model;
        // std::string main_model = getMainModel(fs);

    // while(1) {
    //     auto model = hdfsHander.getLastestModel();
    //     if(main_model && model->model_name.compare(main_model->model_name) == 0) {
    //         continue;
    //     }else{
    //         main_model = model;
    //         std::cout << "switch a new model "<< main_model->model_name << std::endl;
    //     }
    //     std::cout << "load before: "<<*model << std::endl;
    //     int load_num = model->load_num.load();
    //     while((load_num = model->load_num.load()) < model->local_slice_num) {
    //         std::cout << "wait slice download ...." << std::endl;
    //         std::cout << "hdfs alreadly download slice: " << load_num << std::endl;
    //         sleep(1);
    //     }
    //     std::cout << "load after: "<<*model << std::endl;
    //     // std::cout << model;
    //     // std::string main_model = getMainModel(fs);
    // }
}

void test_ModelHander() {
    int node_id = 1 , node_num = 4;
    int n_thread = 4;
    ModelHander modelHander(node_id, node_num);
    modelHander.init();

    std::vector<std::thread> threads;
    for(int i = 0; i < n_thread; ++i) {
        threads.emplace_back([&]{
            int n = 10;
            while(n--) {
                ModelMeta *model = nullptr;    
                while((model = modelHander.getMainModelMeta()) == nullptr) {
                    // std::cout << "faild to gain model...." << std::endl;
                    sleep(2);
                }
                std::cout << "gain a model:" << std::endl
                    << *model << std::endl;

                char buf[1024];
                for(int i = 0; i < 100; ++i) { 
                    int slice_partition = i % model->slice_num;
                    auto sliceReader = modelHander.getSliceReader(model, slice_partition);
                    
                    if(slice_partition % node_num == node_id) {
                        assert(sliceReader != nullptr);
                    }else{
                        assert(sliceReader == nullptr);
                    }
                    
                    if(!sliceReader) {
                        std::cout << "faild to gain slice: "<< slice_partition << std::endl;
                        continue;
                    }else{
                        sliceReader->incrRef();
                        sliceReader->Read(0, 1024, buf);
                        std::cout << "read content: " << buf << std::endl; 
                        sliceReader->decrRef();
                    }

                    sleep(1);
                }
            }
        });
    }

    for(int i = 0; i < threads.size(); ++i)
        threads[i].join();
}


// int main(int argc, char **argv) {
//     test_ModelHander();
    

//     // hdfsHander.loadModelMeta(main_model);
//     return 0;
// }

