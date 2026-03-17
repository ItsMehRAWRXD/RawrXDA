#include <winsock2.h>
#include <thread>
#include <string>
#include "runtime_core.h"

#pragma comment(lib, "ws2_32.lib")

void handle_client(SOCKET s) {
    char buf[4096];
    int r = recv(s, buf, sizeof(buf), 0);
    if (r <= 0) {
        closesocket(s);
        return;
    }

    std::string req(buf, r);
    auto pos = req.find("\r\n\r\n");
    std::string body = pos!=std::string::npos ? req.substr(pos+4) : "";

    std::string out = process_prompt(body);

    std::string resp =
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" + out;
    send(s, resp.c_str(), resp.size(), 0);
    closesocket(s);
}

void start_server(int port) {
    WSADATA wsa; 
    WSAStartup(MAKEWORD(2,2),&wsa);
    SOCKET sock=socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY;

    bind(sock,(sockaddr*)&addr,sizeof(addr));
    listen(sock,5);

    while(true){
        SOCKET c=accept(sock,0,0);
        if (c != INVALID_SOCKET) {
             std::thread(handle_client,c).detach();
        }
    }
}
