
#这个只是当前的 demo 模板，后续需要根据需求拆分，比如拆分成 build.sh, tar-submit.sh，debug.sh这样的
set -ex
IMAGE=public-images-registry.cn-hangzhou.cr.aliyuncs.com/public/alimama-2023:v0

echo "0-Init"
cd `dirname $0`
#docker compose -f docker-compose-hadoop.yml up || true
docker compose -f docker-compose-run-local.yml down || true
#cp data-1 data -r
rm -rf build node-1 node-2 node-3 node-4 node-5 node-6 node-testbench || true
#删除版本文件
#docker run --rm --network alimama ${IMAGE} hdfs dfs -fs hdfs://namenode:9000/ -rm -f //rollback.version

echo "1-Tar"
cd `dirname $0`
rm -rf code.tar.gz || true
#考生相关：指定这个code.tar.gz就是用于提交的代码压缩包
tar -czf code.tar.gz --directory=code .

echo "2-Build"
cd `dirname $0`
rm -rf build || true
mkdir build
tar -xzf code.tar.gz --director=build
#考生相关: build.sh是构建入口脚本, run.sh是运行入口脚本
docker run --rm  -v /`pwd`/build://work -w //work --network none ${IMAGE} bash build.sh



echo "3-Run"
cd `dirname $0`
cp -rp build node-1 
cp -rp build node-2 
cp -rp build node-3 
cp -rp build node-4 
cp -rp build node-5 
cp -rp build node-6 
docker compose -f docker-compose-run-local.yml up -d

#cp -rp testbench/testbench testbench/data.json node-testbench/ 
#mkdir node-testbench
#echo "4-Tail Log"
#tail -F node-testbench/logs/test.log
#docker run --cpuset-cpus="16-23" --cpus=8 -m 2G --rm -it -v ./testbench:/work -w /work --network alimama ${IMAGE} ./testbench

#echo "4-Tail Log"
#tail -F node-testbench/logs/test.log




