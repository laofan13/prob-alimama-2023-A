
set -ex
IMAGE=public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/alimama-2023:v0
docker pull ${IMAGE}
docker pull public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/hadoop-historyserver:2.1.1-hadoop3.3.1-java8
docker pull public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/hadoop-nodemanager:2.1.1-hadoop3.3.1-java8
docker pull public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/hadoop-resourcemanager:2.1.1-hadoop3.3.1-java8
docker pull public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/hadoop-datanode:2.1.1-hadoop3.3.1-java8
docker pull public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/hadoop-namenode:2.1.1-hadoop3.3.1-java8
docker compose -f docker-compose-hadoop.yml up -d && echo "sleep 60 for hadoop start" && sleep 60
docker run --rm -v ./testbench:/work -w /work --network none ${IMAGE}  make
docker run --rm -v ./testbench:/work -w /work --network none ${IMAGE} ./gen_data localdata.json
docker run --rm -v ./testbench:/work -w /work --network alimama ${IMAGE} ./export_to_hdfs localdata.json

