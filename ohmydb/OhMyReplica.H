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
      std::map<int32_t, ServerInfo> clusterConfig, int id, bool waitForPeers,
      std::string dbPath, bool enableBootstrap, std::string storeDir );

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
  raft::AddServerRet AddServer( raft::AddServerParams args );
  raft::RemoveServerRet RemoveServer( raft::RemoveServerParams args );

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
};

inline void ReplicaManager::initialiseServices(
    std::map<int32_t, ServerInfo> clusterConfig, int id, bool waitForPeers,
    std::string dbPath, bool enableBootstrap, std::string storeDir )
{
  raft_.bootstrap( id, enableBootstrap, storeDir );

  // @TODO: If we are bootstrapping, we should read server info from log
  // so there is no need to pass serverInfo here. But for debuggin purpose,
  // it's still ideal to enable bootstrapping with server info.
  if ( !enableBootstrap ) {
    raft_.setClusterConfig( clusterConfig );
  }

  clusterConfig = raft_.getClusterConfig();
  auto raftPort = clusterConfig[id].raft_port;
  auto raftIp = clusterConfig[id].ip;

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
  for ( auto const& [i, serverConfig]: clusterConfig  ) {
    if ( (int)i == id ) {
      continue;
    }
    auto address = serverConfig.ip + ":" + std::to_string(serverConfig.raft_port); 
    peers[i] = std::make_unique<RaftClient>(grpc::CreateChannel(
        address, grpc::InsecureChannelCredentials())); 
  }

  LogInfo( "Waiting for a majority of peers to be up..." )
  // keep pinging until a majority of peers are up
  while ( true ) {
    size_t activePeers = 1; // myself
    for ( auto& [i, peer]: peers ) {
      if ( peer->Ping(1) > 0 ) {
        ++activePeers;
      }
    }
    if ( !waitForPeers || ( activePeers * 2 > clusterConfig.size() ) ) {
      break;
    }
    std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
  }

  for ( auto& [i, peer]: peers ) {
    raft_.addPeer( i, std::move( peer ) );
  }

  LogInfo( "Services Started: Raft Initialised" );

  // start Database RPC server
  auto dbIp = clusterConfig[id].ip;
  auto dbPort = clusterConfig[id].db_port;
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
    std::string leaderAddr = raft_.getLastKnownLeaderDBAddr();
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

  // we couldn't submit the job, this usually means we are not the leader
  if ( ! isSubmitted ) {
    raft::PromiseStore<raft::RaftOp::res_t>::Instance()
      .getAndRemove( it );
    std::string leaderAddr = raft_.getLastKnownLeaderDBAddr();
    return { ohmydb::ErrorCode::NOT_LEADER, leaderAddr, 0 };
  }

  // all went well and job is submitted -> must block for execution
  return {
    ohmydb::ErrorCode::OK, "",
    static_cast<int32_t>( std::get<bool>( ft.get() ) ) 
  };
}

inline raft::AppendEntriesRet ReplicaManager::AppendEntries( raft::AppendEntriesParams args )
{
  return raft_.AppendEntries( args );
}

inline raft::RequestVoteRet ReplicaManager::RequestVote( raft::RequestVoteParams args )
{
  return raft_.RequestVote( args );
}

inline raft::AddServerRet ReplicaManager::AddServer( raft::AddServerParams args )
{
  return raft_.AddServer( args );
}

inline raft::RemoveServerRet ReplicaManager::RemoveServer( raft::RemoveServerParams args )
{
  return raft_.RemoveServer( args );
}