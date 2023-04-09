#pragma once

#include "ConsensusUtils.H"

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>

#include "raft.grpc.pb.h"

class RaftService final : public raftproto::Raft::Service
{
public:
    explicit RaftService() {}

    grpc::Status TestCall(grpc::ServerContext *, const raftproto::Cmd *, raftproto::Ack *);
    grpc::Status AppendEntries(grpc::ServerContext*, const raftproto::AppendEntriesRequest*, raftproto::AppendEntriesResponse*);
    grpc::Status RequestVote(grpc::ServerContext*, const raftproto::RequestVoteRequest*, raftproto::RequestVoteResponse*);
    grpc::Status NetworkUpdate(grpc::ServerContext*, const raftproto::NetworkUpdateRequest*, raftproto::NetworkUpdateResponse*);
};

class RaftClient
{
public:
    RaftClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(raftproto::Raft::NewStub(channel)) {}
    int32_t Ping(int32_t cmd);
    std::optional<raft::AppendEntriesRet> AppendEntries( raft::AppendEntriesParams );
    std::optional<raft::RequestVoteRet> RequestVote( raft::RequestVoteParams );
    void NetworkUpdate( std::vector<raft::PeerNetworkConfig> cfgVec );
private:
    std::unique_ptr<raftproto::Raft::Stub> stub_;
};

