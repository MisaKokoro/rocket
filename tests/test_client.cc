#include <unistd.h>
#include <string.h>
#include "rocket/net/tcp/net_addr.h"
#include "rocket/common/log.h"
#include "rocket/common/config.h"
#include "rocket/net/tcp/tcp_acceptor.h"
#include "rocket/net/tcp/tcp_server.h"
#include "rocket/net/fd_event_group.h"
#include "rocket/net/tcp/tcp_connection.h"
#include "rocket/net/tcp/tcp_client.h"
#include "rocket/net/coder/string_coder.h"
#include "rocket/net/coder/tinypb_coder.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/net/rpc/rpc_dispatcher.h"
#include "rocket/common/msg_id_util.h"

void test_connect() {
  // 调用 conenct 连接 server
  // wirte 一个字符串
  // 等待 read 返回结果

  int fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd < 0) {
    ERRORLOG("invalid fd %d", fd);
    exit(0);
  }

  sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(12345);
  inet_aton("127.0.0.1", &server_addr.sin_addr);

  int rt = connect(fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));

  DEBUGLOG("connect success");

  std::string msg = "hello rocket!cxcx";
  
  rt = write(fd, msg.c_str(), msg.length());

  DEBUGLOG("success write %d bytes, [%s]", rt, msg.c_str());

  char buf[100] = {0};
  rt = read(fd, buf, 100);
  DEBUGLOG("success read %d bytes, [%s]", rt, std::string(buf).c_str());

}

void test_tcp_client() {
  rocket::IPNetAddr::s_ptr addr = std::make_shared<rocket::IPNetAddr>("127.0.0.1",12345);
  rocket::TcpClient client(addr);

  client.connect([addr,&client]() {
    DEBUGLOG("success connect [%s]", addr->toString().c_str());
    auto message = std::make_shared<rocket::TinyPBProtocol>();
    // message->info = "hello rocket";
    message->m_msg_id = "123456789";
    message->m_pb_data = "test pb data";
    client.writeMessage(message, [](rocket::AbstractProtocol::s_ptr msg_ptr){
      DEBUGLOG("send message success");
    });

    client.readMessage("123456789", [](rocket::AbstractProtocol::s_ptr msg_ptr) {
      auto message = std::dynamic_pointer_cast<rocket::TinyPBProtocol>(msg_ptr);
      DEBUGLOG("msg_id[%s], get response [%s]", message->getReqId().c_str(), message->m_pb_data.c_str());
    });
  });
}

int main() {
    rocket::Config::SetGlobalConfig("/home/yanxiang/Desktop/MyProject/rocket/conf/rocket.xml");
    rocket::Logger::InitGlobalLogger();


    // test_connect();
    test_tcp_client();
    return 0;
}
