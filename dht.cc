#include <stdio.h>
#include <string.h>
#include <uv.h>

#include <iostream>
#include <string>
#include <vector>

#include "json.hpp"

std::vector<std::string> start_urls{
    "dht.transmissionbt.com", "router.bittorrent.com", "router.utorrent.com"};

using json = nlohmann::json;

std::string bencode(const json& j) {
  std::string ret;
  if (j.is_object()) {
    ret.append("d");
    for (auto it = j.begin(); it != j.end(); ++it) {
      ret.append(std::to_string(it.key().size()));
      ret.append(":");
      ret.append(it.key());

      ret.append(bencode(it.value()));
    }
    ret.append("e");
  } else if (j.is_array()) {
    ret.append("l");
    for (auto it = j.begin(); it != j.end(); ++it) {
      ret.append(*it);
    }
    ret.append("e");
  } else if (j.is_string()) {
    std::string x = j.get<std::string>();
    ret.append(std::to_string(x.size()));
    ret.append(":");
    ret.append(x);
  } else if (j.is_number()) {
    ret.append("i");
    int64_t x = j.get<int64_t>();
    ret.append(std::to_string(x));
    ret.append("e");
  }

  return ret;
}

int bdecode(const std::string& data, json* j) {
  if (data.size() == 0) {
    return 0;
  }

  // printf("-- decode -- %s\n", data.c_str());

  // string case
  int c = data[0];
  if (isdigit(c)) {
    // printf("string\n");
    int e = data.find(':');
    std::string numz = data.substr(0, e);
    int num = std::stoi(numz);
    std::string val = data.substr(e + 1, num);

    *j = val;
    // std::cout << "|" << numz << "|=" << val << std::endl;
    return 1 + e + num;
  } else if (c == 'i') {
    // printf("integer\n");
    auto e = data.find('e');
    std::string s = data.substr(1, e - 2);
    int val = std::stoi(s);
    *j = val;
    return e;
  } else if (c == 'l') {
    // printf("list\n");
    *j = json::array();
    int pos = 1;
    while (1) {
      json j0;
      int x = bdecode(data.substr(pos), &j0);
      pos += x;
      if (x != 0) {
        (*j).push_back(j0);
      }
      if (data[pos] == 'e') {
        return pos + 1;
      }
    }
  } else if (c == 'd') {
    // printf("dict\n");
    *j = json::object();
    int pos = 1;
    while (1) {
      json key, val;
      int x = bdecode(data.substr(pos), &key);
      pos += x;
      x = bdecode(data.substr(pos), &val);
      pos += x;
      if (x != 0) {
        (*j)[key.get<std::string>()] = val;
      }
      if (data[pos] == 'e') {
        return pos + 1;
      }
    }
  }

  return 0;
}

uv_loop_t* loop = nullptr;

void alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = (char*)malloc(suggested_size);
  buf->len = suggested_size;
}

void ping_send(uv_udp_send_t* req, int status) {
  printf("-- ping send: %d\n", status);
}

void response(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
              const struct sockaddr* addr, unsigned flags) {
  if (nread > 0) {
    std::string data(buf->base, nread);

    json j;
    bdecode(data, &j);
    std::string id = j["r"]["id"].get<std::string>();

    printf("-- recv [%zu]: ", id.size());
    for (uint32_t i = 0; i < id.size(); ++i) {
      printf("%d.", (uint8_t)(id[i]));
    }
    printf("\n");

  } else {
    printf(" === fail\n");
  }
}

void host_resolved(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  auto it = res;
  for (; it != nullptr; it = it->ai_next) {
    if (it->ai_family == AF_INET && it->ai_socktype == SOCK_STREAM &&
        it->ai_addrlen >= 4) {
      uint8_t* addr =
          (uint8_t*)&((struct sockaddr_in*)(it->ai_addr))->sin_addr.s_addr;
      printf("-- ip: %d.%d.%d.%d\n", addr[0], addr[1], addr[2], addr[3]);

      json ping, a;
      a["id"] = "12345678901234567890";
      ping["t"] = "aa";
      ping["y"] = "q";
      ping["q"] = "ping";
      ping["a"] = a;

      std::string x = bencode(ping);

      printf("-- send [%zu]: %s\n", x.size(), x.c_str());

      // krpc
      std::string xping = bencode(ping);

      uv_udp_t* udp = new uv_udp_t;

      uv_udp_init(loop, udp);

      uv_udp_recv_start(udp, alloc, response);

      uv_udp_send_t* req = new uv_udp_send_t;

      uv_buf_t* buf = new uv_buf_t;
      buf->base = strdup(xping.c_str());
      buf->len = xping.size();

      struct sockaddr_in addr1;
      memset(&addr1, 0, sizeof(addr1));
      addr1.sin_family = AF_INET;
      addr1.sin_port = htons(6881);
      addr1.sin_addr.s_addr = *(int32_t*)(addr);

      uv_udp_send(req, udp, buf, 1, (struct sockaddr*)(&addr1), ping_send);
    }
  }
}

int main() {
  loop = new uv_loop_t;
  uv_loop_init(loop);

  for (const auto s : start_urls) {
    uv_getaddrinfo_t* info = new uv_getaddrinfo_t;
    uv_getaddrinfo(loop, info, host_resolved, s.c_str(), nullptr, nullptr);
  }

  uv_run(loop, UV_RUN_DEFAULT);
}
