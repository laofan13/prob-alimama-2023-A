#include "hdfs_hander.h"

#include <cstddef>
#include <ostream>
#include <sstream>
#include <fstream>
#include <filesystem>

#include <unistd.h>

char hdfs_buf[BUFFER_SIZE];

std::ostream& operator<<(std::ostream& os, SliceMeta& slice) {
    os << '\t' << "slice_name: " << slice.slice_name<< "|"
        << '\t' << "slice_no: " << slice.slice_no << "|"
        << '\t' << "hdfs_slice_path: " << slice.hdfs_slice_path << "|"
        << '\t' << "local_slice_path: " << slice.local_slice_path << "|"
        << '\t' << "size: " << slice.size << "|"
        << '\t' << "is_load: " << slice.is_load;
    return os;
}

std::ostream& operator<<(std::ostream& os, ModelMeta& model) {
    os << "ModelMeta: {"  <<  std::endl;
    os << '\t' << "model_name: " << model.model_name << std::endl
        << '\t' << "hdfs_model_path: " << model.hdfs_model_path << std::endl
        << '\t' << "local_model_path: " << model.local_model_path << std::endl
        << '\t' << "slice_num: " << model.slice_num << std::endl
        << '\t' << "slice_size: " << model.slice_size << std::endl;
    os << '\t' << "model slices: {"<< std::endl;
    for(auto slice: model.slices)
        os << '\t' << *slice << std::endl;
    os <<'\t' << "}"  << std::endl;
    os << "}"  << std::endl;
    return os;
}

HDFSHander::HDFSHander(const std::string & hdfs_address, 
    const std::string & model_root_dir, 
    const std::string & local_root_dir)
    :hdf_address_(hdfs_address), 
    model_root_dir_(model_root_dir), 
    local_root_dir_(local_root_dir)
{
    is_init_ = false;
}

HDFSHander::~HDFSHander() {
    for(auto it: model_tables){
        ModelMeta* model = it.second;
        // clearLocalModel(*model, [](SliceMeta& slice) {return true;});
        for(auto slice: model->slices)
            delete slice;
        delete model;
    }
        
}

int HDFSHander::init() {
    fs_ = hdfsConnect(hdf_address_.c_str(), 0);
    if(nullptr == fs_)
        return -1;
    if(hdfsExists(fs_, model_root_dir_.c_str()) != 0) 
        return -1;
    is_init_ = true;
    return 0;
}

ModelMeta* HDFSHander::getLastestModel() {
    std::string model_name = getAvailableModelName();
    if(model_name.empty() || model_name == "")
        return nullptr;
    if(model_tables.find(model_name) != model_tables.end()) {
        return model_tables[model_name];
    }
    return loadModelMeta(model_name);
}

void HDFSHander::loadModelToLocal(ModelMeta& model, SliceLoadPred pred) {
    // if local model dir exsit 
    if (!std::filesystem::exists(model.local_model_path)) {
        if(!std::filesystem::create_directory(model.local_model_path)) {
            std::cout << "faild to create local directory :" << model.local_model_path << std::endl;
            return ;
        }
    }

    // load slice data from hdfs
    for(auto slice: model.slices) {
        if(pred(*slice)) {
            loadSliceToLocal(*slice);
        }
    }
}

bool HDFSHander::loadSliceToLocal(SliceMeta& slice) {
    // open hdfs slice file : model_.../model_slice.01
    // if local model dir exsit 
    if (!std::filesystem::exists(slice.model->local_model_path)) {
        if(!std::filesystem::create_directory(slice.model->local_model_path)) {
            std::cout << "faild to create local directory :" << slice.model->local_model_path << std::endl;
            return false;
        }
    }

    if(slice.is_load)
        return true;
    hdfsFile sliceFile = hdfsOpenFile(fs_, slice.hdfs_slice_path.c_str(), O_RDONLY, 0, 0, 0);
    if(!sliceFile) {
        fprintf(stdout, "Failed to open %s for slice \n", slice.hdfs_slice_path.c_str());
        hdfsCloseFile(fs_, sliceFile);
        return false;
    }

    // open local slice file : model_.../model_slice.0;
    std::ofstream local_slice_file(slice.local_slice_path, std::ios::binary);
    if (!local_slice_file.is_open()) {
        fprintf(stdout, "Failed to open %s for local slice file\n", slice.local_slice_path.c_str());
        hdfsCloseFile(fs_, sliceFile);
        local_slice_file.close();
        return false;
    }

    // download file content to local
    tSize num_read_bytes = 0;
    while((num_read_bytes = hdfsRead(fs_, sliceFile, hdfs_buf, BUFFER_SIZE)) != 0) {
        local_slice_file.write(hdfs_buf, num_read_bytes);
    }
    
    hdfsCloseFile(fs_, sliceFile);
    local_slice_file.close();
    slice.is_load = true;
    return true;
} 

void HDFSHander::clearLocalModel(ModelMeta& model, SliceLoadPred pred) {
    // rm hdfs slice file : model_.../model_slice.01
    // if (!std::filesystem::remove_all(model.local_model_path)) {
    //     fprintf(stdout, "Failed to remove %s model local file\n", model.local_model_path.c_str());
    //     return;
    // }
    // load slice data from hdfs
    for(auto slice: model.slices) {
        if(pred(*slice)) {
            clearLocalSlice(*slice);
        }
    }
} 

bool HDFSHander::clearLocalSlice(SliceMeta& slice) {
    // rm hdfs slice file : model_.../model_slice.01
    if (!std::filesystem::remove(slice.local_slice_path)) {
        fprintf(stdout, "Failed to remove %s local slice file\n", slice.local_slice_path.c_str());
        return false;
    }

    slice.is_load = false;
    return true;
} 


bool HDFSHander::isAvailable(const std::string & model_name) {
    std::string model_path = model_root_dir_ + model_name;
    if(hdfsExists(fs_, model_path.c_str()) != 0) {
        std::cout << model_path << " does not exist in hdfs" << std::endl;
        return false;
    }
    std::string model_done_path = model_path + "/model.done";
    if(hdfsExists(fs_, model_done_path.c_str()) != 0) {
        // std::cout << model_done_path << " does not exist in hdfs, but model unavailable" << std::endl;
        return false;
    }
    return true;
}

std::string HDFSHander::getAvailableModelName() {
    char buf[128];

    // if rollback.version exist
    std::string rollback_version = model_root_dir_ + "rollback.version";
    if(hdfsExists(fs_, rollback_version.c_str()) == 0) {
        hdfsFile rollbackFile = hdfsOpenFile(fs_, rollback_version.c_str(), O_RDONLY, 0, 0, 0);
        if(rollbackFile) {
            tSize num_read_bytes = hdfsRead(fs_, rollbackFile, &buf, MODEL_NAME_LEN);
            buf[MODEL_NAME_LEN] = '\0';
            // std::cout << "rollback.version: " << buf << std::endl; 
            if(isAvailable(buf)) {
                // std::cout << "rollback model version: " << buf << std::endl; 
                hdfsCloseFile(fs_, rollbackFile);
                return buf;
            }
        }
        hdfsCloseFile(fs_, rollbackFile);
    }

    // find lastest model version
    int numEntries = 0;
    hdfsFileInfo* hdfsFileInfolist = hdfsListDirectory(fs_, model_root_dir_.c_str(), &numEntries);
    for(int i = numEntries - 1; i >= 0; --i) {
        std::string model_path = hdfsFileInfolist[i].mName;
        std::string model_name = model_path.substr( model_path.find_last_of('/') + 1);
        // if file name prefix == 'model_'
        if(model_name.compare(0, MODEL_PREFIX.size(), MODEL_PREFIX) == 0) {
            if(isAvailable(model_name)) {
                return model_name;
            }
        }
    }
    // std::cout << "Not found available model" << std::endl;
    return "";
}

ModelMeta* HDFSHander::loadModelMeta(const std::string & model_name) {
    // if model exsit
    ModelMeta* model = new ModelMeta();
    model->model_name = model_name;
    model->hdfs_model_path = model_root_dir_ + model_name;
    model->local_model_path = local_root_dir_ + model_name;

    // modelMeta->load_num = 0;
    // modelMeta->local_slice_num = 0;
    // modelMeta->load_ = false;

    // read model.meta
    std::ostringstream oss;
    {
        std::string meta_path = model->hdfs_model_path + "/model.meta";
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
    int slice_size = 0;
    std::stringstream iss(oss.str());
    std::string line;
    while (std::getline(iss, line)) {
        if(line.compare(0, 5, "slice") == 0) {
            SliceMeta* slice = new SliceMeta();
            int first_comma_pos = line.find_first_of(',');
            slice->slice_name = line.substr(6, first_comma_pos - 6);
            slice->hdfs_slice_path = model->hdfs_model_path + "/" + slice->slice_name;
            slice->local_slice_path = model->local_model_path + "/" + slice->slice_name;
            slice->slice_no = slice_num++;
            slice->size = std::stoi(line.substr(first_comma_pos + 6));
            slice->model = model;
            model->slices.push_back(slice);

            slice_size = slice->size;
        }
    }
    model->slice_num = slice_num;
    model->slice_size = slice_size;
    // model->local_slice_num = slice_num / node_num_ + ( slice_num % node_num_ <= node_id_ ? 0 : 1);
    model_tables[model_name] = model;

    return model;
}


void hdfs_test1() {
    HDFSHander hdfsHander(HDFS_ADDRES, MODEL_ROOT_DIR, LOCAL_ROOT_DIR);
    if(hdfsHander.init() != 0) {
        std::cout << "HDFSHander faild to init ...." << std::endl;
        return ;
    }

    ModelMeta * model = nullptr;
    while((model = hdfsHander.getLastestModel()) == nullptr) {
        sleep(1);
    }
    std::cout << *model;

    // hdfsHander.loadModelToLocal(*model);
 
    hdfsHander.loadModelToLocal(*model, [](SliceMeta &slice) {
        return  true;
    });

    hdfsHander.clearLocalModel(*model);
    
    hdfsHander.clearLocalModel(*model, [](SliceMeta &slice) {
        return slice.slice_no % 2;
    });
}

// int main() {
//     hdfs_test1();
//     return 0;
// }