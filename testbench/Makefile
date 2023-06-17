
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`
IMAGE = public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/alimama-2023:v0
PROTOS_PATH = ./
CPPFLAGS += -Ofast `pkg-config --cflags protobuf grpc`  -I${HADOOP_HOME}/include -I/usr/local/include/etcd/proto/  
LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++`\
           -pthread\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
					 -lboost_log\
           -ldl 
LDFLAGS += -lpthread -letcd-cpp-api -lprotobuf -lgrpc++ -lgrpc -lz -lcpprest -lssl -lcrypto -lboost_system  -L${HADOOP_HOME}/lib/native/ -lhdfs 
#CXX = clang++
#CXX = clang++


.PRECIOUS: %.grpc.pb.cc
%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=. --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=. $<

######################################################################################

all: clean gen_data export_to_hdfs testbench

clean:
	rm -f *.o *.pb.cc *.pb.h gen_data export_to_hdfs testbench
gen_data:
	g++ -std=c++17 gen_data.cpp -o gen_data
export_to_hdfs:
	g++ -std=c++17 export_to_hdfs.cpp -o export_to_hdfs
testbench: alimama.pb.o alimama.grpc.pb.o testbench.o
	${CXX} -std=c++17 ${CPPFLAGS} $^ $(LDFLAGS) -o $@

testbench2: alimama.pb.o alimama.grpc.pb.o testbench2.o
	${CXX} -std=c++17 ${CPPFLAGS} $^ $(LDFLAGS) -o $@

testload: testload.cpp
	 g++ testload.cpp -lmodel_slice_reader -o testload


build-all-in-docker:
	docker run --rm -it  -v ./:/work  -w /work $(IMAGE) make all

build-testbench-in-docker:
	docker run --rm -it  -v ./:/work  -w /work $(IMAGE) make testbench

bash:
	docker run --rm -it  -v ./:/work  -w /work --network alimama $(IMAGE) bash
etcd:
	docker run --rm -d -e ALLOW_NONE_AUTHENTICATION=yes --name etcd --network $(IMAGE)  docker.io/bitnami/etcd:3.5
