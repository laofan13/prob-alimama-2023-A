#pragma once 
#include<string>
#include<memory>

class ModelSliceReader {
public:
        ModelSliceReader();
        virtual ~ModelSliceReader();
        virtual bool Load(const std::string &model_file_path); // 从本地加载模型数据切片文件
        virtual bool Read(size_t data_start, size_t data_len, char *data_buffer); //  读取模型数据切片数据
        virtual bool Unload(); // 卸载模型数据切片
private:
    class ModelSliceReaderImpl; 
    std::shared_ptr<ModelSliceReaderImpl> impl = nullptr;
};