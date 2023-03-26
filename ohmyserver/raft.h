#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "raft.grpc.pb.h"

class RaftService final : public raft::Raft::Service
{
public:
    explicit RaftService() {}

    grpc::Status TestCall(grpc::ServerContext *, const raft::Cmd *, raft::Ack *);
};

class RaftClient
{
public:
    RaftClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(raft::Raft::NewStub(channel)) {}
    int32_t Ping(int32_t cmd);

private:
    std::unique_ptr<raft::Raft::Stub> stub_;
};

grpc::Status RaftService::TestCall(
    grpc::ServerContext *, const raft::Cmd *cmd, raft::Ack *ack)
{
    std::cout << "Raft client has made contact... " << std::endl;
    ack->set_ok(cmd->sup());
    return grpc::Status::OK;
}

int32_t RaftClient::Ping(int32_t cmd)
{
    raft::Cmd msg;
    msg.set_sup(cmd);
    raft::Ack reply;

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