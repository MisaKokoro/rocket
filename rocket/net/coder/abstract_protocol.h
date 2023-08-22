#ifndef ROCKET_NET_ABSTRACT_PROTOCOL_H
#define ROCKET_NET_ABSTRACT_PROTOCOL_H

#include <memory>
#include <string>
namespace rocket {

struct AbstractProtocol : public std::enable_shared_from_this<AbstractProtocol> {
public:
    using s_ptr = std::shared_ptr<AbstractProtocol>;

    AbstractProtocol() {};

    virtual ~AbstractProtocol() {}

    std::string getReqId() {
        return m_req_id;
    }

    void setReqId(const std::string &req_id) {
        m_req_id = req_id;
    }

public:
    std::string m_req_id; // 请求号，唯一标识一个请求

};
}
#endif