#pragma once

#include "config.h"

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <functional>

#include <mutex>
#include <condition_variable>

#include "hdfs.h" 
#include <cstddef>

struct ModelMeta;


struct SliceMeta {
    std::string slice_name;
    std::string hdfs_slice_path;
    std::string local_slice_path;
    int slice_no;
    u_int64_t size;
    bool is_load;

    ModelMeta * model;
};

struct ModelMeta {
    std::string model_name;
    std::string hdfs_model_path;
    std::string local_model_path;

    int slice_num;
    u_int64_t slice_size;
    std::vector<SliceMeta*> slices;
};

// outpur format
std::ostream& operator<<(std::ostream& os, SliceMeta& slice);
std::ostream& operator<<(std::ostream& os, ModelMeta& model);

class HDFSHander {
public:
    HDFSHander(const std::string & hdfs_address, const std::string & model_root_dir, const std::string & local_root_dir);

    int init();
    ModelMeta* getLastestModel();

    HDFSHander(HDFSHander &) = delete;
    HDFSHander& operator=(HDFSHander &) = delete;

    using SliceLoadPred = std::function<bool(SliceMeta& slice)>;

    void loadModelToLocal(ModelMeta& model, SliceLoadPred pred = [](SliceMeta& slice) {return true;});
    void clearLocalModel(ModelMeta& model, SliceLoadPred pred = [](SliceMeta& slice) {return true;});
    bool loadSliceToLocal(SliceMeta& slice);
    bool clearLocalSlice(SliceMeta& slice);

    ~HDFSHander();

private:
    bool isAvailable(const std::string & model_name);
    std::string getAvailableModelName();
    ModelMeta* loadModelMeta(const std::string & model_name);

private:
    const std::string hdf_address_;
    const std::string model_root_dir_;
    const std::string local_root_dir_;

    bool is_init_;
    hdfsFS  fs_;
    std::map<std::string, ModelMeta*> model_tables;
};