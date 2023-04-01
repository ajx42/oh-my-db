#pragma once

#include "ConsensusUtils.H"
#include "OhMyConfig.H"
#include "OhMyRaft.H"
#include "RaftService.H"
#include <random>

enum raft_state {follower, candidate, leader};

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
  void busy();

  ~ReplicaManager();

private:
  ReplicaManager() {
    term = 0;
    votes = 0;
    state = follower;
    heartbeat = true; // For now, on start up, not assuming a timeout has occured
  }
  raft::RaftManager<RaftClient> raft_;
  
  grpc::ServerBuilder builder_;
  RaftService raftService_;
  
  std::thread raftServer_;

  //raft election state data
  size_t votes;
  size_t term;
  raft_state state;
  bool heartbeat;
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

  std::vector<RaftClient> peers;
  for ( unsigned int i = 0; i < servers.size(); ++i ) {
    if ( (int)i == id ) {
      continue;
    }
    auto address = servers[i].ip + ":" + std::to_string(servers[i].port); 
    peers.push_back( RaftClient(grpc::CreateChannel(
        address, grpc::InsecureChannelCredentials())));
    
    if ( waitForPeers ) {
      while ( peers.back().Ping(1) < 0 ) {
        sleep(1);
      }
      LogInfo( "ping OK: " + std::to_string(i) );
    }
  }
  raft_.attachPeers( std::move(peers) );
  raft_.setId( id ); 
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

inline void ReplicaManager::busy()
{
  //Used for selecting election timeouts
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> timeOutGen(150,300);
  size_t leaderHeartBeatInterval = 100;


  while(1) { 

    if(this->state == leader)
    {
      //Send heartbeat RPC
      std::this_thread::sleep_for(std::chrono::milliseconds(leaderHeartBeatInterval)); 
    }
    else if(this->state == follower)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(timeOutGen(gen))); 

      if(heartbeat)
      {
        //Heartbeat was received, leader is still active

        heartbeat = false; //Heartbeat RPC will set this back to true

      }
      else
      {
        //No heartbeat received, begin election.

        //Increment current term, switch to candidate state, vote for self.
        this->term++;
        this->state = candidate;
        this->votes = 1;

        //Request votes from others:
        raft::RequestVoteParams args; //TODO: Fill in parameters
        RequestVote(args);
      }
    }
    else if(this->state == candidate)
    {
      //if heartbeat
      //Someone else won the election and sent a valid heartbeat.
      //this->state = follower

      //if votes >= (N/2)+1
      //win election, this->state = leader

      //else
      //Didn't win within election time limit, timeout for now
      //this->state = follower

      //TODO: We should detect winning the majority vote earlier
      //than the election timeout period. The longer we wait,
      //the more likely servers that voted for us will timeout
      //and start another election
    }
    }
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

inline raft::AppendEntriesRet ReplicaManager::AppendEntries( raft::AppendEntriesParams args )
{
  return raft_.AppendEntries( args );
}

inline raft::RequestVoteRet ReplicaManager::RequestVote( raft::RequestVoteParams args )
{
  return raft_.RequestVote( args );
}

