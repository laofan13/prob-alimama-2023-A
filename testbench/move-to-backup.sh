hdfs dfs -fs hdfs://namenode:9000/ -rm -f /rollback.version
hdfs dfs  -fs hdfs://namenode:9000/ -mv /model_2023_03_01_12_30_00  /backup
hdfs dfs  -fs hdfs://namenode:9000/ -mv /model_2023_03_01_13_30_00  /backup
hdfs dfs  -fs hdfs://namenode:9000/ -mv /model_2023_03_01_13_00_00  /backup
hdfs dfs -fs hdfs://namenode:9000/ -ls /
hdfs dfs -fs hdfs://namenode:9000/ -ls /backup
