#pragma once

#include "ConsensusUtils.H"

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>

#include "raft.grpc.pb.h"

class RaftService final : public raft::Raft::Service
{
public:
    explicit RaftService() {}

    grpc::Status TestCall(grpc::ServerContext *, const raft::Cmd *, raft::Ack *);
    grpc::Status AppendEntries(grpc::ServerContext*, const raft::AppendEntriesArg*, raft::Ack*);
};

class RaftClient
{
public:
    RaftClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(raft::Raft::NewStub(channel)) {}
    int32_t Ping(int32_t cmd);
    raft::AppendEntriesRet AppendEntries( raft::AppendEntriesParams );
    raft::RequestVoteRet RequestVote( raft::RequestVoteParams );
private:
    std::unique_ptr<raft::Raft::Stub> stub_;
};

