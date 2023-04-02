#pragma once

#include "ConsensusUtils.H"
#include "OhMyConfig.H"
#include "OhMyRaft.H"
#include "RaftService.H"

class ReplicaManager {
public:
  static ReplicaManager& Instance() {
    static ReplicaManager obj;
    return obj;
  }

  // Initialise replica services, this should bring up all RPC interfaces
  // and connect to peers as well. waitForPeers makes the replica ping each
  // peer before starting operation. This should be used for testing only!
  void initialiseServices(
      std::vector<ServerInfo> servers, int id, bool waitForPeers ); 

  // These methods are accessed by the Database RPC server layer. But exposing
  // them as public methods here allows for quick testing :D
  std::optional<int> get( int key );
  bool put( std::pair<int, int> kvp );

  // Similarly providing handle for AppendEntries and RequestVote here. These
  // are called from the Raft RPC interface during normal operation. These should
  // not be used by the user. Maybe we can move these to private later.
  // See ConsensusUtils for the struct definitions.
  raft::AppendEntriesRet AppendEntries( raft::AppendEntriesParams args );
  raft::RequestVoteRet RequestVote( raft::RequestVoteParams args ); 

  void start();
  void stop();

  ~ReplicaManager();

private:
  ReplicaManager() {}
  raft::RaftManager<RaftClient> raft_;
  
  grpc::ServerBuilder builder_;
  RaftService raftService_;
  
  std::thread raftServer_;

};

inline void ReplicaManager::initialiseServices(
    std::vector<ServerInfo> servers, int id, bool waitForPeers )
{
  auto raftPort = servers[id].port;
  auto raftIp = servers[id].ip;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  // start raft server
  std::string raftServerAddr(raftIp + ":" + std::to_string(raftPort));
  builder_.AddListeningPort(raftServerAddr, grpc::InsecureServerCredentials());
  builder_.RegisterService(&raftService_);
  raftServer_ = std::thread([this]{ 
    std::unique_ptr<grpc::Server> raftServer(builder_.BuildAndStart());   
    raftServer->Wait(); 
  });
  raftServer_.detach();

  std::map<int32_t, std::unique_ptr<RaftClient>> peers;
  for ( size_t i = 0; i < servers.size(); ++i ) {
    if ( (int)i == id ) {
      continue;
    }
    auto address = servers[i].ip + ":" + std::to_string(servers[i].port); 
    peers[i] = std::make_unique<RaftClient>(grpc::CreateChannel(
        address, grpc::InsecureChannelCredentials()));
    
    if ( waitForPeers ) {
      while ( peers[i]->Ping(1) < 0 ) {
        sleep(1);
      }
      LogInfo( "ping OK: " + std::to_string(i) );
    }
  }
  raft_.initialiseConnections( id, std::move(peers) );
  LogInfo( "Services Started: Raft Initialised" );
}

inline void ReplicaManager::start()
{
  raft_.start();
}

inline void ReplicaManager::stop()
{
  raft_.stop();
}

inline ReplicaManager::~ReplicaManager()
{
  stop();
}

inline std::optional<int> ReplicaManager::get( int key )
{
  std::promise<raft::RaftOp::res_t> pr;
  auto ft = pr.get_future();
  auto it = raft::PromiseStore<raft::RaftOp::res_t>::Instance()
              .insert( std::move( pr ) );
  raft::RaftOp op {
    .kind = raft::RaftOp::GET,
    .args = { key },
    .promiseHandle = { it }
  };

  auto isSubmitted = raft_.submit( op );
  if ( ! isSubmitted ) {
    raft::PromiseStore<raft::RaftOp::res_t>::Instance()
     .getAndRemove( it );
    return {}; 
  }
  return std::get<std::optional<int>>( ft.get() );
}

inline bool ReplicaManager::put( std::pair<int, int> kvp )
{
  std::promise<raft::RaftOp::res_t> pr;  
  auto ft = pr.get_future();
  auto it = raft::PromiseStore<raft::RaftOp::res_t>::Instance()
              .insert( std::move( pr ) );

  raft::RaftOp op {
    .kind = raft::RaftOp::PUT,
    .args = kvp,
    .promiseHandle = { it }
  };

  bool isSubmitted = raft_.submit( op );
  if ( ! isSubmitted ) {
    raft::PromiseStore<raft::RaftOp::res_t>::Instance()
      .getAndRemove( it );
    return false;
  }
  return std::get<bool>( ft.get() );
}

inline raft::AppendEntriesRet ReplicaManager::AppendEntries( raft::AppendEntriesParams args )
{
  return raft_.AppendEntries( args );
}

inline raft::RequestVoteRet ReplicaManager::RequestVote( raft::RequestVoteParams args )
{
  return raft_.RequestVote( args );
}

