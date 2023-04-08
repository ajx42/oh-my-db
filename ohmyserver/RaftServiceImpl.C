#include "RaftService.H"
#include "OhMyReplica.H"
#include "WowLogger.H"

grpc::Status RaftService::TestCall(
    grpc::ServerContext *, const raftproto::Cmd *cmd, raftproto::Ack *ack)
{
    ack->set_ok(cmd->sup());
    return grpc::Status::OK;
}

grpc::Status RaftService::AppendEntries(
    grpc::ServerContext*, const raftproto::AppendEntriesRequest* request, raftproto::AppendEntriesResponse* response )
{
  // The idea is to decode the received args and repackage them to match exact
  // raft specification. So that our Raft impl  doesn't need to handle decoding.
  raft::AppendEntriesParams param;
  param.term = request->term();
  param.leaderId = request->leader_id();
  param.prevLogIndex = request->prev_log_index();
  param.prevLogTerm = request->prev_log_term();
  param.leaderCommit = request->leader_commit();

  for ( size_t i = 0; i < request->entries().size(); i += sizeof(raft::TransportEntry) ) {
    auto& entry = *reinterpret_cast<const raft::TransportEntry*>( request->entries().data() + i );
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
  
  response->set_term( ret.term );
  response->set_success( ret.success );

  return grpc::Status::OK;
}

grpc::Status RaftService::RequestVote(
    grpc::ServerContext *, const raftproto::RequestVoteRequest *request,
    raftproto::RequestVoteResponse *response)
{
  raft::RequestVoteParams param;
  param.term = request->term();
  param.candidateId = request->candidate_id();
  param.lastLogIndex = request->last_log_index();
  param.lastLogTerm = request->last_log_term();

  auto ret = ReplicaManager::Instance().RequestVote( param );
  response->set_term( ret.term );
  response->set_vote_granted( ret.voteGranted );

  return grpc::Status::OK;
}

int32_t RaftClient::Ping(int32_t cmd)
{
    raftproto::Cmd request;
    request.set_sup(cmd);
    raftproto::Ack response;

    grpc::ClientContext context;

    auto status = stub_->TestCall(&context, request, &response);
    if (status.ok())
    {
        return response.ok();
    }
    else
    {
        return -1;
    }
}


std::optional<raft::AppendEntriesRet> 
RaftClient::AppendEntries( raft::AppendEntriesParams args )
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
  raftproto::AppendEntriesRequest request;
  request.set_term( args.term );
  request.set_leader_id( args.leaderId );
  request.set_prev_log_index( args.prevLogIndex );
  request.set_prev_log_term( args.prevLogTerm );
  request.set_entries( toSend );
  request.set_leader_commit( args.leaderCommit );

  
  raftproto::AppendEntriesResponse response;
  grpc::ClientContext context;
  
  auto status = stub_->AppendEntries(&context, request, &response);
  
  if ( status.ok() ) {
    return raft::AppendEntriesRet{
      .term = response.term(),
      .success = static_cast<bool>( response.success() ) 
    };
  } else {
    return {};
  }
}

std::optional<raft::RequestVoteRet> 
RaftClient::RequestVote( raft::RequestVoteParams args )
{
  raftproto::RequestVoteRequest request;
  request.set_term( args.term );
  request.set_candidate_id( args.candidateId );
  request.set_last_log_index( args.lastLogIndex );
  request.set_last_log_term( args.lastLogTerm );

  raftproto::RequestVoteResponse response;
  grpc::ClientContext context;
  
  auto status = stub_->RequestVote(&context, request, &response);

  if ( !status.ok() ) {
    return {};
  }

  raft::RequestVoteRet ret = {
    .term = response.term(),
    .voteGranted = static_cast<bool>( response.vote_granted() )
  };
  return {ret};
}
