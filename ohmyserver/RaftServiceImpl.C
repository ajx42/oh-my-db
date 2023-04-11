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
    raft::RaftOp::arg_t args;
    if ( entry.kind == raft::RaftOp::GET ) {
      args = raft::RaftOp::arg_t(entry.arg1);
    } else if ( entry.kind == raft::RaftOp::PUT ) {
      args = raft::RaftOp::arg_t(std::make_pair( entry.arg1, entry.arg2 ));
    } else if ( entry.kind == raft::RaftOp::ADD_SERVER ) {
      args = raft::RaftOp::arg_t(entry.serverInfo);
    } else {
      args = raft::RaftOp::arg_t(entry.arg1);
    }

    param.entries.push_back({ 
      .term = entry.term,
      .index = entry.index,
      .op = raft::RaftOp {
        .kind = entry.kind,
        .args = args,
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

grpc::Status RaftService::AddServer(
    grpc::ServerContext *, const raftproto::AddServerRequest *request,
    raftproto::AddServerResponse *response)
{
  raft::AddServerParams param;
  param.serverId = request->server_id();
  param.raftPort = request->raft_port();
  param.dbPort = request->db_port();
  strcpy( param.ip, request->ip().c_str() );
  strcpy( param.name, request->name().c_str() );

  auto ret = ReplicaManager::Instance().AddServer( param );
  response->set_error_code( ret.errorCode );
  response->set_leader_addr( ret.leaderAddr );

  return grpc::Status::OK;
}

grpc::Status RaftService::RemoveServer(
    grpc::ServerContext *, const raftproto::RemoveServerRequest *request,
    raftproto::RemoveServerResponse *response)
{
  raft::RemoveServerParams param;
  param.serverId = request->server_id();

  auto ret = ReplicaManager::Instance().RemoveServer( param );
  response->set_error_code( ret.errorCode );
  response->set_leader_addr( ret.leaderAddr );
  return grpc::Status::OK;
}

grpc::Status RaftService::NetworkUpdate(
    grpc::ServerContext*,
    const raftproto::NetworkUpdateRequest* request,
    raftproto::NetworkUpdateResponse* response )
{
  std::vector<raft::PeerNetworkConfig> pVec;
  for ( size_t i = 0; i < request->data().size(); i += sizeof(raft::PeerNetworkConfig) ) {
    pVec.push_back(
        *reinterpret_cast<const raft::PeerNetworkConfig*>( request->data().data() + i ) );
  }
  ReplicaManager::Instance().NetworkUpdate( pVec );
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
    int32_t arg1 = 0, arg2 = 0;
    ServerInfo serverInfo;
    
    switch ( op.kind ) {
      case raft::RaftOp::GET: {
        arg1 = std::get<raft::RaftOp::getarg_t>( op.args );
        break;
      }
      case raft::RaftOp::PUT: {
        arg1 = std::get<raft::RaftOp::putarg_t>( op.args ).first;
        arg2 = std::get<raft::RaftOp::putarg_t>( op.args ).second;
        break;
      }
      case raft::RaftOp::ADD_SERVER: {
        serverInfo = std::get<raft::RaftOp::addserverarg_t>( op.args );
        break;
      }
      case raft::RaftOp::REMOVE_SERVER: {
        arg1 = std::get<raft::RaftOp::getarg_t>( op.args );
        break;
      }
    }
     
    entriesToShip.push_back( raft::TransportEntry {
      .term = entry.term,
      .index = entry.index,
      .kind = op.kind,
      .arg1 = arg1,
      .arg2 = arg2,
      .serverInfo = serverInfo
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

std::optional<raft::AddServerRet>
RaftClient::AddServer( raft::AddServerParams args )
{
  raftproto::AddServerRequest request;
  request.set_server_id( args.serverId );
  request.set_ip( args.ip );
  request.set_raft_port( args.raftPort );
  request.set_db_port( args.dbPort );
  request.set_name( args.name );

  raftproto::AddServerResponse response;
  grpc::ClientContext context;
  
  auto status = stub_->AddServer(&context, request, &response);

  if ( !status.ok() ) {
    return {};
  }

  raft::AddServerRet ret = {
    .errorCode = static_cast<raft::ErrorCode>(response.error_code()),
    .leaderAddr = response.leader_addr()
  };
  return {ret};
}

std::optional<raft::RemoveServerRet>
RaftClient::RemoveServer( raft::RemoveServerParams args )
{
  raftproto::RemoveServerRequest request;
  request.set_server_id( args.serverId );

  raftproto::RemoveServerResponse response;
  grpc::ClientContext context;
  
  auto status = stub_->RemoveServer(&context, request, &response);

  if ( !status.ok() ) {
    return {};
  }

  raft::RemoveServerRet ret = {
    .errorCode = static_cast<raft::ErrorCode>(response.error_code()),
    .leaderAddr = response.leader_addr()
  };
  return {ret};
}

void RaftClient::NetworkUpdate( std::vector<raft::PeerNetworkConfig> pVec )
{
  raftproto::NetworkUpdateRequest request;
  request.set_num_entries( pVec.size() );
  
  std::string toSend(reinterpret_cast<const char*>(pVec.data()),
      sizeof(raft::PeerNetworkConfig) * pVec.size() );
  
  request.set_data( toSend );

  raftproto::NetworkUpdateResponse response;
  grpc::ClientContext context;

  std::ignore  = stub_->NetworkUpdate(&context, request, &response);
}

