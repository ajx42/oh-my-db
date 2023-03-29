#pragma once

#include "ConsensusUtils.H"

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>

#include "raft.grpc.pb.h"

class ReplicaManager;

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
    int32_t AppendEntries( std::vector<raft::TransportEntry> entries );
private:
    std::unique_ptr<raft::Raft::Stub> stub_;
};

inline grpc::Status RaftService::TestCall(
    grpc::ServerContext *, const raft::Cmd *cmd, raft::Ack *ack)
{
    std::cout << "Raft client has made contact... " << std::endl;
    ack->set_ok(cmd->sup());
    return grpc::Status::OK;
}

inline grpc::Status RaftService::AppendEntries(
    grpc::ServerContext*, const raft::AppendEntriesArg* args, raft::Ack* ack )
{
  std::cout << "Received Entries: " << std::endl;
  for ( size_t i = 0; i < args->entries().size(); i += sizeof(raft::TransportEntry) ) {
    auto& entry = *reinterpret_cast<const raft::TransportEntry*>( args->entries().data() + i );
    std::cout << "[ " << entry.term << " " << entry.kind << " " << entry.arg1 << " " << entry.arg2 << " ]" << std::endl;
  }
  ack->set_ok(1);
  return grpc::Status::OK;
}

inline int32_t RaftClient::Ping(int32_t cmd)
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

inline int32_t RaftClient::AppendEntries( std::vector<raft::TransportEntry> entries )
{
  std::string toSend( reinterpret_cast<const char*>( entries.data() ), entries.size() * sizeof(raft::TransportEntry) );
  raft::AppendEntriesArg args;
  args.set_entries( toSend );
  
  raft::Ack reply;
  grpc::ClientContext context;
  
  auto status = stub_->AppendEntries(&context, args, &reply);
  if ( status.ok() ) {
    return reply.ok();
  } else {
    return -1;
  }
}

