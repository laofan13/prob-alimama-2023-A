

std::string getLocalIP() {
  struct ifaddrs *ifAddrStruct = NULL;
  void *tmpAddrPtr = NULL;
  std::string localIP;
  getifaddrs(&ifAddrStruct);
  while (ifAddrStruct != NULL) {
    if (ifAddrStruct->ifa_addr->sa_family == AF_INET) {
      tmpAddrPtr = &((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
      char addressBuffer[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
      std::string interfaceName(ifAddrStruct->ifa_name);
      if (interfaceName == "en0" || interfaceName == "eth0") {
        return addressBuffer;
      }
    }
    ifAddrStruct = ifAddrStruct->ifa_next;
  }
  return "";
}

std::string local_ip = getLocalIP();
std::string external_address =
    local_ip + std::string(":") + std::to_string(port);
etcd::Client etcd("http://etcd:2379");
std::string key = std::string("/services/modelservice/") + external_address;
std::string value("");
etcd.set(key, value);

int main(int argc, char **argv) {
  //   RunServer();
  Response res;
  res.set_status(0);

  res.add_slice_data("1234567891");
  res.add_slice_data("abcd");

  // 访问字段内容
  for (int i = 0; i < res.slice_data_size(); i++) {
    std::cout << res.slice_data(i) << std::endl;
  }



  return 0;
}