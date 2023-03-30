#include "RaftService.H"
#include "OhMyReplica.H"
#include "WowLogger.H"

grpc::Status RaftService::TestCall(
    grpc::ServerContext *, const raft::Cmd *cmd, raft::Ack *ack)
{
    ack->set_ok(cmd->sup());
    return grpc::Status::OK;
}

grpc::Status RaftService::AppendEntries(
    grpc::ServerContext*, const raft::AppendEntriesArg* args, raft::Ack* ack )
{
  // The idea is to decode the received args and repackage them to match exact
  // raft specification. So that our Raft impl  doesn't need to handle decoding.
  raft::AppendEntriesParams param;

  for ( size_t i = 0; i < args->entries().size(); i += sizeof(raft::TransportEntry) ) {
    auto& entry = *reinterpret_cast<const raft::TransportEntry*>( args->entries().data() + i );
    param.entries.push_back({ 
      .term = entry.term,
      .index = entry.index,
      .op = raft::RaftOp {
        .kind = entry.kind,
        .args = entry.kind == raft::RaftOp::GET
              ? raft::RaftOp::arg_t(entry.arg1)
              : raft::RaftOp::arg_t(std::make_pair( entry.arg1, entry.arg2 )),
        .promiseHandle = {}
      }
    });
  }
  
  // hook to pass AppendEntries to ReplicaManager
  auto ret = ReplicaManager::Instance().AppendEntries( param );
  (void) ret;
  // @FIXME: we should be using the return value here to setup our response

  ack->set_ok(1);
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
        LogInfo( "Received OK" );
        return reply.ok();
    }
    else
    {
        return -1;
    }
}

raft::AppendEntriesRet RaftClient::AppendEntries( raft::AppendEntriesParams args )
{
  std::vector<raft::TransportEntry> entriesToShip;
  for ( auto entry: args.entries ) {
    auto& op = entry.op;
    entriesToShip.push_back( raft::TransportEntry {
      .term = entry.term,
      .index = entry.index,
      .kind = op.kind,
      .arg1 = op.kind == raft::RaftOp::GET ? std::get<raft::RaftOp::getarg_t>( op.args ) : std::get<raft::RaftOp::putarg_t>( op.args ).first,
      .arg2 = op.kind == raft::RaftOp::GET ? 0 : std::get<raft::RaftOp::putarg_t>( op.args ).second
    });
  }
  std::string toSend( reinterpret_cast<const char*>( entriesToShip.data() ), entriesToShip.size() * sizeof(raft::TransportEntry) );
  raft::AppendEntriesArg argsToShip;
  argsToShip.set_entries( toSend );
  
  raft::Ack reply;
  grpc::ClientContext context;
  
  auto status = stub_->AppendEntries(&context, argsToShip, &reply);
  if ( status.ok() ) {
    return {};
  } else {
    return {};
  }
}

raft::RequestVoteRet RaftClient::RequestVote( raft::RequestVoteParams )
{
  return {};
}
