#pragma once

#include "ConsensusUtils.H"
#include "OhMyConfig.H"
#include "OhMyRaft.H"
#include "RaftService.H"
#include "DatabaseService.H"
#include "DatabaseUtils.H"
#include "ohmydb/LevelDBProxy.H"

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
      std::vector<ServerInfo>& servers, int id, bool waitForPeers, std::string dbPath ); 

  // These methods are accessed by the Database RPC server layer. But exposing
  // them as public methods here allows for quick testing :D
  ohmydb::Ret get( int key );
  ohmydb::Ret put( std::pair<int, int> kvp );

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
  
  grpc::ServerBuilder raftBuilder_;
  RaftService raftService_;
  std::thread raftServer_;

  grpc::ServerBuilder dbBuilder_;
  OhMyDBService dbService_;
  std::thread dbServer_;

  std::vector<ServerInfo> servers_;
};

inline void ReplicaManager::initialiseServices(
    std::vector<ServerInfo>& servers, int id, bool waitForPeers, std::string dbPath )
{
  servers_ = servers;
  auto raftPort = servers_[id].raft_port;
  auto raftIp = servers_[id].ip;

  raft::LevelDB<int,int>::Instance().initialize(dbPath);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  // start raft server
  std::string raftServerAddr(raftIp + ":" + std::to_string(raftPort));
  raftBuilder_.AddListeningPort(raftServerAddr, grpc::InsecureServerCredentials());
  raftBuilder_.RegisterService(&raftService_);
  raftServer_ = std::thread([this]{ 
    std::unique_ptr<grpc::Server> raftServer(raftBuilder_.BuildAndStart());   
    raftServer->Wait(); 
  });
  raftServer_.detach();

  std::map<int32_t, std::unique_ptr<RaftClient>> peers;
  
  // construct RPC clients for all peers (excluding the replica we are at)
  for ( size_t i = 0; i < servers_.size(); ++i ) {
    if ( (int)i == id ) {
      continue;
    }
    auto address = servers_[i].ip + ":" + std::to_string(servers_[i].raft_port); 
    peers[i] = std::make_unique<RaftClient>(grpc::CreateChannel(
        address, grpc::InsecureChannelCredentials())); 
  }

  LogInfo( "Waiting for a majority of peers to be up..." )
  // keep pinging until a majority of peers are up
  while ( true ) {
    auto activePeers = 1; // myself
    for ( auto& [i, peer]: peers ) {
      if ( peer->Ping(1) > 0 ) {
        LogInfo( "ping OK: " + std::to_string(i) );
        ++activePeers;
      }
    }
    if ( !waitForPeers || ( activePeers * 2 > servers_.size() ) ) {
      break;
    }
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
  }

  raft_.initialiseConnections( id, std::move(peers) );
  LogInfo( "Services Started: Raft Initialised" );

  // start Database RPC server
  auto dbIp = servers_[id].ip;
  auto dbPort = servers_[id].db_port;
  std::string dbServerAddr(dbIp + ":" + std::to_string(dbPort));
  dbBuilder_.AddListeningPort(dbServerAddr, grpc::InsecureServerCredentials());
  dbBuilder_.RegisterService(&dbService_);
  dbServer_ = std::thread([this]{ 
    std::unique_ptr<grpc::Server> dbServer(dbBuilder_.BuildAndStart());   
    dbServer->Wait(); 
  });
  dbServer_.detach();
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

inline ohmydb::Ret ReplicaManager::get( int key )
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

  auto [ isSubmitted, leaderId ] = raft_.submit( op );
  if ( ! isSubmitted ) {
    raft::PromiseStore<raft::RaftOp::res_t>::Instance()
     .getAndRemove( it );
    std::string leaderAddr = servers_[leaderId].ip + ":" + std::to_string(servers_[leaderId].db_port);
    return { ohmydb::ErrorCode::NOT_LEADER, leaderAddr, -1 };
  }

  auto val = std::get<std::optional<int>>( ft.get() );
  if ( !val.has_value() ) {
    return { ohmydb::ErrorCode::KEY_NOT_FOUND, "", -1 };
  } 

  return { ohmydb::ErrorCode::OK, "", val.value() };
}

inline ohmydb::Ret ReplicaManager::put( std::pair<int, int> kvp )
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

  auto [ isSubmitted, leaderId ] = raft_.submit( op );
  if ( ! isSubmitted ) {
    raft::PromiseStore<raft::RaftOp::res_t>::Instance()
      .getAndRemove( it );
    std::string leaderAddr = servers_[leaderId].ip + ":" + std::to_string(servers_[leaderId].db_port);
    return { ohmydb::ErrorCode::NOT_LEADER, leaderAddr, -1 };
  }

  return { ohmydb::ErrorCode::OK, "", -1 };
}

inline raft::AppendEntriesRet ReplicaManager::AppendEntries( raft::AppendEntriesParams args )
{
  return raft_.AppendEntries( args );
}

inline raft::RequestVoteRet ReplicaManager::RequestVote( raft::RequestVoteParams args )
{
  return raft_.RequestVote( args );
}

