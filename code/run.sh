echo "this is NODE-$NODE_ID"
if [[ "$NODE_ID" == "1" ]]; then
	# 启动节点1对应服务	
    ./src/model_server
    sleep 1000;
else
	# 启动其他节点对应服务
    ./src/model_server
    sleep 1000;
fi



