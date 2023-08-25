#include <google/protobuf/service.h>
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include "rocket/net/rpc/rpc_channel.h"
#include "rocket/net/coder/tinypb_protocol.h"
#include "rocket/common/msg_id_util.h"
#include "rocket/net/rpc/rpc_controller.h"
#include "rocket/common/log.h"
#include "rocket/common/error_code.h"
#include "rocket/net/tcp/tcp_client.h"

namespace rocket {
RpcChannel::RpcChannel(NetAddr::s_ptr peer_addr) : m_peer_addr(peer_addr) {
    m_client = std::make_shared<TcpClient>(m_peer_addr);
}

RpcChannel::~RpcChannel() {
    DEBUGLOG("~RpcChannel");
}

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController* controller, 
                            const google::protobuf::Message* request,
                            google::protobuf::Message* response, 
                            google::protobuf::Closure* done) {
    
    // 发送的请求message
    std::shared_ptr<TinyPBProtocol> req_protocol = std::make_shared<TinyPBProtocol>();
    RpcController* my_controller = dynamic_cast<RpcController*>(controller);

    if (my_controller == nullptr) {
        ERRORLOG("controller dynamic cast failed");
        return;
    }

    // 获取msgID
    if (my_controller->GetMsgId().empty()) {
        req_protocol->m_msg_id = MsgIDUtil::GenMsgID();
        my_controller->SetMsgId(req_protocol->m_msg_id);
    } else {
        req_protocol->m_msg_id = my_controller->GetMsgId();
    }

    // 获取method name
    req_protocol->m_method_name = method->full_name();
    INFOLOG("%s | call method name [%s]", req_protocol->m_msg_id.c_str(), req_protocol->m_method_name.c_str());
    // 一定要初始化完成
    if (!m_is_init) {
        std::string err_info = "RpcChannel not call init()";
        my_controller->SetError(ERROR_RPC_CHANNEL_INIT, err_info);
        ERRORLOG("%s | %s, RpcChannel not init ", req_protocol->m_msg_id.c_str(), err_info.c_str());
        return;
    }
    // 将request序列化
    if (!request->SerializeToString(&req_protocol->m_pb_data)) {
        std::string err_info = "failed to serialize";
        my_controller->SetError(ERROR_FAILED_SERIALIZE, err_info);
        ERRORLOG("%s | error : %s, origin request [%s] ", req_protocol->m_msg_id.c_str(), 
                    err_info.c_str(), request->ShortDebugString().c_str());
        return;
    }
    // 使用智能指针持久化 channel对象，防止其析构
    s_ptr channel = shared_from_this();
    // 创建客户端，将message发送
    // auto &client = *m_client;
    m_client->connect([req_protocol, channel, my_controller]() mutable {
        // auto my_controller = dynamic_cast<RpcController*>(channel->getController());
        // 连接服务器发生了错误
        if (channel->getTcpClient()->getConnectErrorCode() != 0) {
            my_controller->SetError(channel->getTcpClient()->getConnectErrorCode(), channel->getTcpClient()->getConnectErrorInfo());
            ERRORLOG("%s | connect error , error code [%d] , error info [%s], peer addr [%s]",
                req_protocol->m_msg_id.c_str(),my_controller->GetErrorCode(), my_controller->GetErrorInfo().c_str(), 
                channel->getTcpClient()->getPeerAddr()->toString().c_str());
            return;
        }
        // 发送message
        channel->getTcpClient()->writeMessage(req_protocol, [req_protocol, channel, my_controller](AbstractProtocol::s_ptr)  mutable {
            INFOLOG("%s |, send request success: call method name [%s], peer addr [%s], local addr[%s]", req_protocol->m_msg_id.c_str(),
                    req_protocol->m_method_name.c_str(),
                    channel->getTcpClient()->getPeerAddr()->toString().c_str(),
                    channel->getTcpClient()->getLocalAddr()->toString().c_str());
            // 拿到回包后进行解析
            channel->getTcpClient()->readMessage(req_protocol->m_msg_id, [channel, my_controller](AbstractProtocol::s_ptr msg) mutable {
                auto rsp_protocol = std::dynamic_pointer_cast<TinyPBProtocol>(msg);
                INFOLOG("%s | success get response method name [%s], peer addr [%s], local addr [%s]", rsp_protocol->m_msg_id.c_str(),
                        rsp_protocol->m_method_name.c_str(),
                        channel->getTcpClient()->getPeerAddr()->toString().c_str(),
                        channel->getTcpClient()->getLocalAddr()->toString().c_str());
                // 成功收到回包，执行对应的回调函数

                // 将收到的回包反序列化
                if (!channel->getResponse()->ParseFromString(rsp_protocol->m_pb_data)) {
                    ERRORLOG("deserialize failed");
                    my_controller->SetError(ERROR_FAILED_DESERIALIZE, "deserialize error");
                    return;
                }

                if (rsp_protocol->m_err_code != 0) {
                    ERRORLOG("%s | call rpc method [%s] failed, error code [%d], error info [%s]",
                        rsp_protocol->m_msg_id.c_str(),rsp_protocol->m_err_code, rsp_protocol->m_err_info.c_str());
                    my_controller->SetError(rsp_protocol->m_err_code, rsp_protocol->m_err_info);
                    return;
                }

                INFOLOG("%s | call rpc succsee method name [%s], peer addr [%s], local addr [%s]", rsp_protocol->m_msg_id.c_str(),
                        rsp_protocol->m_method_name.c_str(),
                        channel->getTcpClient()->getPeerAddr()->toString().c_str(),
                        channel->getTcpClient()->getLocalAddr()->toString().c_str());

                if (channel->getClosure()) {
                    channel->getClosure()->Run();
                }
                // 执行完毕，可以析构了
                channel.reset();
            });
        });
    });
}

// 将用到的指针通过智能指针的方式存下来，防止在使用过程中析构
void RpcChannel::Init(controller_s_ptr controller, message_s_ptr req, message_s_ptr rsp, closure_s_ptr done) {
    if (m_is_init) {
        return;
    }

    m_controller = controller;
    m_request = req;
    m_response = rsp;
    m_closure = done;

    m_is_init = true;
}

google::protobuf::RpcController* RpcChannel::getController() {
    return m_controller.get();
}

google::protobuf::Message* RpcChannel::getRequest() {
    return m_request.get();
}

google::protobuf::Message* RpcChannel::getResponse() {
    return m_response.get();
}

google::protobuf::Closure* RpcChannel::getClosure() {
    return m_closure.get();
}

TcpClient* RpcChannel::getTcpClient() {
    return m_client.get();
}
}