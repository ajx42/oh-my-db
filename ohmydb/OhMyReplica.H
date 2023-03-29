#pragma once

#include "OhMyConfig.H"
#include "OhMyRaft.H"
#include "RaftService.H"

class ReplicaManager {
public:
  static ReplicaManager& Instance() {
    static ReplicaManager obj;
    return obj;
  }

  void initialiseServices( std::vector<ServerInfo> servers, int id ); 

  std::optional<int> get( int key );
  bool put( std::pair<int, int> kvp );

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
    std::vector<ServerInfo> servers, int id )
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

  std::vector<RaftClient> peers;
  for ( int i = 0; i < servers.size(); ++i ) {
    if ( i == id ) {
      continue;
    }
    auto address = servers[i].ip + ":" + std::to_string(servers[i].port); 
    peers.push_back( RaftClient(grpc::CreateChannel(
        address, grpc::InsecureChannelCredentials())));
   
    while ( peers.back().Ping(1) < 0 ) {
      sleep(1);
    }
    LogInfo( "ping OK: " + std::to_string(i) );
  }
  raft_.attachPeers( std::move(peers) );
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

  raft_.submit( op );

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

  raft_.submit( op );

  return std::get<bool>( ft.get() );
}

