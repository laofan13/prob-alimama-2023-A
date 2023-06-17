
# etcd

# node
docker run --rm  -v `pwd`:/work -w /work --network alimama public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/alimama-2023:v0

docker run --rm  -it  -v `pwd`:/work -w /work --network alimama public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/alimama-2023:v0 -e NODE_ID=1,NODE_NUM=6

docker run --rm  -it  -v `pwd`:/work \
-w /work --env NODE_ID=0 --env NODE_NUM=1 --env MEMORY=4G --env CPU=2 \
--network alimama public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/alimama-2023:v0

hdfs  dfs -fs hdfs://namenode:9000 -ls /

hdfs  dfs -fs hdfs://namenode:9000 -put ./rollback.version /backup/rollback.version

hdfs  dfs -fs hdfs://namenode:9000 -rm  /backup/rollback.version
hdfs  dfs -fs hdfs://namenode:9000 -cat  /backup/rollback.version

hdfs  dfs -fs hdfs://namenode:9000 -ls /backup/model_2023_03_01_12_30_00/model.done
hdfs  dfs -fs hdfs://namenode:9000 -cat /backup/model_2023_03_01_12_30_00/model.done
hdfs  dfs -fs hdfs://namenode:9000 -rm /backup/model_2023_03_01_12_30_00/model.done

hdfs  dfs -fs hdfs://namenode:9000 -rm /backup/model_2023_03_01_13_30_00/model.done

rm -rf /tmp/model_*