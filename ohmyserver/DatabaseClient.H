#pragma once

#include "DatabaseUtils.H"

#include <optional>

#include <grpcpp/grpcpp.h>
#include <grpcpp/channel.h>
#include "db.grpc.pb.h"

class OhMyDBClient
{
public:
    OhMyDBClient(const std::string& server_addr)
        : channel_(grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()))
        , stub_(ohmydb::OhMyDB::NewStub(channel_)) {}
    int32_t Ping(int32_t cmd);

    std::optional<ohmydb::Ret> Put(int key, int value);
    std::optional<ohmydb::Ret> Get(int key);

    void SetServerAddress(const std::string& server_addr);

private:
    std::unique_ptr<ohmydb::OhMyDB::Stub> stub_;
    std::shared_ptr<grpc::Channel> channel_;
};

// Note this method is only used for Testing
inline int32_t OhMyDBClient::Ping(int32_t cmd)
{
    ohmydb::Cmd msg;
    msg.set_sup(cmd);
    ohmydb::Ack reply;

    grpc::ClientContext context;

    auto status = stub_->TestCall(&context, msg, &reply);
    if (status.ok())
    {
        std::cerr << "Received OK" << std::endl;
        return reply.ok();
    }
    else
    {
        std::cerr << "RPC Failed" << std::endl;
        return -1;
    }
}

inline std::optional<ohmydb::Ret> 
OhMyDBClient::Put(int key, int value)
{
    ohmydb::PutRequest request;
    request.set_key(key);
    request.set_value(value);
    ohmydb::PutResponse response;

    grpc::ClientContext context;

    auto status = stub_->Put(&context, request, &response);
    if (status.ok())
    {
        return {{ static_cast<ohmydb::ErrorCode>(response.error_code()), response.leader_addr(), -1 }};
    }
    else
    {
        std::cerr << "Put: RPC Failed" << std::endl;
        return {};
    }
}

inline std::optional<ohmydb::Ret>
OhMyDBClient::Get(int key)
{
    ohmydb::GetRequest request;
    request.set_key(key);
    ohmydb::GetResponse response;

    grpc::ClientContext context;

    auto status = stub_->Get(&context, request, &response);
    if (status.ok())
    {
        return {{ static_cast<ohmydb::ErrorCode>(response.error_code()), response.leader_addr(), response.value() }};
    }
    else
    {
        std::cerr << "Get: RPC Failed" << std::endl;
        return {};
    }
}

void OhMyDBClient::SetServerAddress(const std::string& server_addr) {
  channel_ = grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials());
  stub_ = ohmydb::OhMyDB::NewStub(channel_);
}