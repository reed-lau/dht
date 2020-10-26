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

uv_loop_t* loop = nullptr;

void alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = (char*)malloc(suggested_size);
  buf->len = suggested_size;
  printf("alloc size = %d\n", suggested_size);
}

void ping_send(uv_udp_send_t* req, int status) {
  printf("ping send, status = %d\n", status);
}

void response(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
              const struct sockaddr* addr, unsigned flags) {
  printf("status = %d buf->len = %d buf->base = %s\n", nread, buf->len,
         buf->base);
}

void host_resolved(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  printf(" resolved status = %d\n", status);

  auto it = res;
  for (it; it != nullptr; it = it->ai_next) {
    if (it->ai_family == AF_INET && it->ai_socktype == SOCK_STREAM &&
        it->ai_addrlen >= 4) {
      uint8_t* addr =
          (uint8_t*)&((struct sockaddr_in*)(it->ai_addr))->sin_addr.s_addr;
      printf("ip: %d.%d.%d.%d\n", addr[0], addr[1], addr[2], addr[3]);

      // json p = {"t":"aa", "y":"q", "q":"ping",
      // "a":{"id":"abcdefghij0123456789"}};
      json ping, a;
      a["id"] = "12345678901234567890";
      ping["t"] = "aa";
      ping["y"] = "q";
      ping["q"] = "ping";
      ping["a"] = a;

      std::string x = bencode(ping);

      std::cout << std::string(80, '=') << std::endl;
      std::cout << x << std::endl;
      std::cout << std::string(80, '=') << std::endl;

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
