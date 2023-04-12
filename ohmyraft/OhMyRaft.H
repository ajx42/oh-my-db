#pragma once

#include <list>
#include <future>
#include <utility>
#include <optional>
#include <map>
#include <thread>
#include <mutex>
#include <vector>
#include <random>

#include "TimeTravelSignal.H"
#include "PromiseStore.H"
#include "ConsensusUtils.H"
#include "TestUtils.H"
#include "WowLogger.H"
#include "PersistentVector.H"
#include "PersistentStore.H"
#include "OhMyConfig.H"
#include "RaftService.H"

namespace raft {

constexpr int32_t RAFT_LEADER_PERIOD_MS = 50;
constexpr int32_t RAFT_MEMBERSHIP_WAIT_ITERS = 100;

enum class RaftRole : int32_t {
  Follower = 0,
  Candidate = 1, 
  Leader = 2,
  Dead = 3
};

struct RaftState
{
  // just going to protect all state using a single mutex
  // some performance hit, but kool for now
  std::mutex Mut;
  
  // (needs to be) persistent state
  int32_t CurrentTerm;
  int32_t VotedFor;
  PersistentVector<LogEntry> Logs;

  // volatile state
  RaftRole Role;
  std::chrono::time_point<std::chrono::system_clock> ElectionResetEvent;
  int32_t CommitIndex; // 
  int32_t LastApplied; // I am not sure why this is not persistent
  int32_t LastKnownLeaderId;
  std::map<int32_t, ServerInfo> ClusterConfig; // membership
  int32_t LastConfigChangeIndex; // index of last config change
  
  // for leaders only peer id -> next/match index
  std::map<int32_t, int32_t> NextIndex;
  std::map<int32_t, int32_t> MatchIndex;

  // for candidate only, non standard
  int32_t VotesReceived;
  
  // handle persistence of VotedFor and CurrentTerm
  PersistentStore pStore;  
  void persist();
};

inline void RaftState::persist()
{
  pStore.store( "VotedFor", VotedFor );
  pStore.store( "CurrentTerm", CurrentTerm );
}

template <class ClientT>
class RaftManager
{
public:
  RaftManager()
  { 
    state_.CurrentTerm = 0;
    state_.VotedFor = -1;
    state_.Logs.reserve( 10000 );
    state_.Role = RaftRole::Follower;
    state_.CommitIndex = -1;
    state_.LastApplied = -1;
    state_.VotesReceived = 0;
    state_.LastKnownLeaderId = 0;
    state_.LastConfigChangeIndex = -1;
  }

  ~RaftManager();

  void setClusterConfig( std::map<int32_t, ServerInfo> config );
  std::map<int32_t, ServerInfo> getClusterConfig();
  void addPeer( int32_t peerId, std::unique_ptr<ClientT>&& rpcclient );
  void removePeer( int32_t peerId );
  std::string getLastKnownLeaderDBAddr();
  std::string getLastKnownLeaderRaftAddr();

  // control points
  void start();
  void stop();

  // initialise persistent state, optionally bootstrap from existing
  void bootstrap( int32_t myId, bool withBootstrap, std::string storeDir );

  // job submission
  std::pair<bool, int32_t > submit( RaftOp op );

  // raft rpc implementations
  AppendEntriesRet  AppendEntries( AppendEntriesParams );
  RequestVoteRet    RequestVote( RequestVoteParams );
  void              NetworkUpdate( std::vector<PeerNetworkConfig> );
  AddServerRet      AddServer( AddServerParams );
  RemoveServerRet   RemoveServer( RemoveServerParams );

private:  
  // raft core logic implementation
  // these must be run on separate threads
  void raftImpl();
  void executerImpl();
  void electionImpl();

  // threads to manage various concurrent activities
  std::thread raftThread; // leader stuff
  std::thread executerThread; // execute committed entries
  std::thread electionThread; // check if leader exists or call for election

  // single switch to break out of all threads (gracefully)
  bool keepRunning_ = false;

  // peer id -> rpc client 
  std::map<int32_t, std::unique_ptr<ClientT>> peers_;

  // these lists and mutexes help with I/O to various threads
  // ideally one would use channels, but going with this easy solution for now
  std::list<RaftOp> dispatchOut_, raftIn_, raftOut_, execIn_;
  std::mutex raftOutMutex_;
  std::mutex raftInMutex_;
  std::mutex raftStateMutex_;

  TimeTravelSignal moreInputsReady_;
  TimeTravelSignal moreExecJobsReady_;

  // all the state that is required by the algorithm is stored here
  // this state must be locked before use
  RaftState state_;

  int32_t id_; // id of this replica

  // helper functions
  void becomeLeader();
  void becomeFollower(int32_t term);
  void becomeCandidate(int32_t term);
  void becomeDead();
  void runLeaderOneIter();

  void ApplyAddServer( ServerInfo );
  void ApplyRemoveServer( int32_t );

  int32_t getRandomElectionTimeout();
  void startElection();
};

template <class T>
void RaftManager<T>::NetworkUpdate(
    std::vector<PeerNetworkConfig> pVec )
{
  for ( const auto& entry: pVec ) {
    if ( peers_.find( entry.peerId ) == peers_.end() ) {
      LogWarn("Unknown Peer in NetworkUpdate: " + entry.str());
    } else {
      peers_[entry.peerId]->setEnable( entry.isEnabled );
      peers_[entry.peerId]->setIsDelayed( entry.isDelayed );
      peers_[entry.peerId]->setDelayMs( entry.delayMs );
      LogInfo("Applied NetworkUpdate: " + entry.str());
    }
  }
}

template <class T>
void RaftManager<T>::addPeer( int32_t peerId, std::unique_ptr<T>&& rpcClient )
{
  std::unique_lock<std::mutex> lock( state_.Mut );
  LogInfo( "Add peer " + std::to_string(peerId) );
  state_.NextIndex[peerId] = 0;
  state_.MatchIndex[peerId] = -1;
  peers_[peerId] = std::move(rpcClient);
  state_.persist();
}

template <class T>
void RaftManager<T>::removePeer( int32_t peerId )
{
  std::unique_lock<std::mutex> lock( state_.Mut );
  LogInfo( "Remove peer " + std::to_string(peerId) );
  state_.NextIndex.erase( peerId );
  state_.MatchIndex.erase( peerId );
  peers_.erase( peerId );
}

template <class T>
void RaftManager<T>::setClusterConfig( std::map<int32_t, ServerInfo> config )
{
  state_.ClusterConfig = config;
  state_.persist();
}

template <class T>
std::map<int32_t, ServerInfo> RaftManager<T>::getClusterConfig()
{
  return state_.ClusterConfig;
}

template <class T>
std::string RaftManager<T>::getLastKnownLeaderDBAddr()
{
  std::lock_guard stateLock { state_.Mut };
  return  std::string(state_.ClusterConfig[state_.LastKnownLeaderId].ip) + ":" +
          std::to_string(state_.ClusterConfig[state_.LastKnownLeaderId].db_port);
}

// This one does not handle locking, use side code needs to do it.
template <class T>
std::string RaftManager<T>::getLastKnownLeaderRaftAddr()
{
  return  std::string(state_.ClusterConfig[state_.LastKnownLeaderId].ip) + ":" +
          std::to_string(state_.ClusterConfig[state_.LastKnownLeaderId].raft_port);
}

template <class T>
std::pair<bool, int> RaftManager<T>::submit( RaftOp op )
{
  std::unique_lock stateLock { state_.Mut };
  if ( (op.kind == RaftOp::OpType::GET || op.kind == RaftOp::OpType::PUT) && 
        state_.Role != RaftRole::Leader ) {
    LogError("This Replica is not the leader. Job can't be submitted.");
    return { false, state_.LastKnownLeaderId };
  }
  stateLock.unlock();

  std::lock_guard<std::mutex> lock( raftInMutex_ );
  dispatchOut_.push_back( op );
  moreInputsReady_.signal();
  return { true, state_.LastKnownLeaderId };
}

// This method sends one round of AppendEntries to all
// connected peers. Based on the replies, we determine if
// any additional jobs can be committed.
template <class T>
void RaftManager<T>::runLeaderOneIter()
{
  state_.Mut.lock();
  auto savedCurrentTerm = state_.CurrentTerm;
  for ( auto op: raftIn_ ) {
    state_.Logs.push_back( {
      .term = state_.CurrentTerm,
      .op = op
    });
    if ( op.kind == RaftOp::OpType::ADD_SERVER ) {
      // apply config change
      ServerInfo info = std::get<RaftOp::addserverarg_t>( op.args );
      ApplyAddServer( info );
    } else if ( op.kind == RaftOp::OpType::REMOVE_SERVER ) {
      // apply config change
      int32_t serverId = std::get<RaftOp::rmserverarg_t>( op.args );
      ApplyRemoveServer( serverId );
    }
  }
  state_.Logs.persist();
  state_.Mut.unlock();


  std::vector<std::thread*> threadHandles;

  for ( auto& [id, peer] : peers_ ) {
    auto th = new std::thread([id = id, this, savedCurrentTerm]{
      AppendEntriesParams args;

      state_.Mut.lock();
      auto nextIndex = state_.NextIndex[id];
      auto prevLogIndex = nextIndex - 1;
      auto prevLogTerm = -1;
      if ( prevLogIndex >= 0 ) {
        prevLogTerm = state_.Logs[prevLogIndex].term;
      }
      for ( size_t i = nextIndex; i < state_.Logs.size(); ++i ) {
        args.entries.push_back({
          .term = state_.Logs[i].term,
          .index = static_cast<int32_t>(i),
          .op = state_.Logs[i].op.withoutPromise()
        });
      }

      args.term = savedCurrentTerm;
      args.prevLogIndex = prevLogIndex;
      args.prevLogTerm = prevLogTerm;
      args.leaderCommit = state_.CommitIndex;
      args.leaderId = id_;
      state_.Mut.unlock();

      auto replyOpt = peers_[id]->AppendEntries( args );
      if ( ! replyOpt.has_value() ) {
        return;
      }

      // @FIXME: logs for debugging
      if ( ! args.entries.empty() ) {
        LogInfo("Sent (with entries) AppendEntriesRPC to PeerId=" + std::to_string( id ) 
            + " " + std::to_string(args.entries.size()));
        LogInfo("Response Received to AppendEntriesRPC from PeerId=" + std::to_string( id )
            + " " + replyOpt.value().str());
      }
      // --
      
      auto reply = replyOpt.value();
      std::lock_guard<std::mutex> lock( state_.Mut );
      if ( reply.term > savedCurrentTerm ) {
        becomeFollower( reply.term );
        return;
      }

      if ( state_.Role == RaftRole::Leader && savedCurrentTerm == reply.term ) {
        if ( reply.success ) {
          state_.NextIndex[id] = nextIndex + args.entries.size();
          state_.MatchIndex[id] = state_.NextIndex[id] - 1;
          auto savedCommitIndex = state_.CommitIndex;
          for ( int32_t i = state_.CommitIndex + 1; i < (int32_t)state_.Logs.size(); ++i ) {
            if ( state_.Logs[i].term == state_.CurrentTerm ) {
              int matchCount = state_.ClusterConfig.find( id_ ) != state_.ClusterConfig.end();
              for ( auto& [pid, _] : peers_ ) {
                if ( state_.MatchIndex[pid] >= i ) {
                  matchCount++;
                }
              }
              if ( matchCount * 2 > state_.ClusterConfig.size() ) {
                state_.CommitIndex = i;
              }
            }
          }
          if ( state_.CommitIndex != savedCommitIndex ) {
            std::lock_guard<std::mutex> rom( raftOutMutex_ );
            for ( int32_t i = state_.LastApplied + 1; i <= state_.CommitIndex; ++i ) {
              raftOut_.push_back( state_.Logs[i].op );
            }
            state_.LastApplied = state_.CommitIndex;
            // signal the executer to take care of queued operations
            moreExecJobsReady_.signal();
          }

        } else {
          state_.NextIndex[id] = nextIndex - 1;
          LogInfo("Unsuccessful Reply: " + reply.str());
        }
      }
    });
    th->detach();
  }

}

template <class T>
void RaftManager<T>::raftImpl()
{
  while ( keepRunning_ ) {
    // this is a hot loop, but is invoked periodically
    // so it's not spinning as tightly as we may think
    raftInMutex_.lock();
    std::swap( dispatchOut_, raftIn_ ); 
    raftInMutex_.unlock();

    state_.Mut.lock();
    auto role = state_.Role;
    state_.Mut.unlock();
    
    if ( role == RaftRole::Leader ) {
      // send one round of appendentries
      runLeaderOneIter();
    }

    // sleep for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(RAFT_LEADER_PERIOD_MS));
   
    // prepare to receive more
    raftIn_.clear();
  }
}

template <class T>
void RaftManager<T>::executerImpl()
{
  while ( keepRunning_ ) {
    // we are using the TimeTravelSignal wait for jobs
    moreExecJobsReady_.wait();

    // once we know we actually have stuff to execute, we pull
    // whatever is in the queue
    raftOutMutex_.lock();
    std::swap( execIn_, raftOut_ );
    raftOutMutex_.unlock();

    LogInfo("Received # OPS: " + std::to_string(execIn_.size()));
    for ( auto op: execIn_ ) {
      op.execute();
    }
    execIn_.clear();
  }
}

template <class T>
void RaftManager<T>::bootstrap( int32_t myId, bool withBootstrap, std::string storeDir )
{
  id_ = myId;

  auto storeFilePrefix = storeDir + "/raft." + std::to_string( id_ ) + '.';
  
  LogInfo("EnableBootstrap=" + std::to_string( withBootstrap ) + " "
          "StoreDir=" + storeDir);
  
  state_.Logs.setup(
      storeFilePrefix + "log.persist",
      withBootstrap,
      []( LogEntry val ) { val.op = val.op.withoutPromise(); return val; }
  );
  
  LogInfo("Bootstrapped Log Length: " + std::to_string( state_.Logs.size() ) );
  for ( const auto& entry: state_.Logs ) {
    LogInfo("BOOT OP: " + entry.str() );
  }

  state_.pStore.setup( storeFilePrefix );
  
  if ( withBootstrap ) {
    state_.VotedFor = state_.pStore.load( "VotedFor", -1 );
    state_.CurrentTerm = state_.pStore.load( "CurrentTerm", 0 );
  } else {
    state_.persist();
  }

  LogInfo("Bootstrapped VotedFor: " + std::to_string( state_.VotedFor ));
  LogInfo("Bootstrapped CurrentTerm: " + std::to_string( state_.CurrentTerm ));
}

template <class T>
void RaftManager<T>::start()
{
  // The application consists of 3 threads that last throughout
  // the run and several more threads spawned by these for a short
  // time to achieve parallelism where possible.
  keepRunning_ = true;
  electionThread = std::thread([this]{electionImpl();});
  executerThread = std::thread([this]{executerImpl();});
  raftThread = std::thread([this]{raftImpl();});
}

template <class T>
void RaftManager<T>::stop()
{
  if ( ! keepRunning_ ) {
    return;
  }
  keepRunning_ = false;
  electionThread.join();
  raftThread.join();
  executerThread.join();
}

template <class T>
RaftManager<T>::~RaftManager()
{
  stop();
}

// RAFT Standard RPC ImplementationsN
// These calls land in RPCServiceImpl.C where data is repackaged to RPC-able
// format. So after implementing here, the packaging logic also needs to be implemented.
// Note calls can happen in parallel, so ensure thread safety while accessing state.

template <class T>
AppendEntriesRet RaftManager<T>::AppendEntries( AppendEntriesParams args )
{
  std::lock_guard<std::mutex> lock(state_.Mut);
  
  // This means we are going to accept this RPC, so good to reset
  // the election timer now.
  if ( args.term >= state_.CurrentTerm ) {
    state_.ElectionResetEvent = std::chrono::system_clock::now();
  }

  if ( state_.Role == RaftRole::Dead ) {
    return {};
  }

  // update leader
  state_.LastKnownLeaderId = args.leaderId;

  if ( args.term > state_.CurrentTerm ) {
    LogInfo("CurrentTerm out of date");
    becomeFollower( args.term );
  }
  
  AppendEntriesRet reply;
  reply.success = false;

  if ( args.term == state_.CurrentTerm ) {
    if ( state_.Role != RaftRole::Follower ) {
      becomeFollower( args.term );
    }
    if ( args.prevLogIndex == -1 ||
         ( args.prevLogIndex < (int32_t)state_.Logs.size() && args.prevLogTerm == state_.Logs[args.prevLogIndex].term) )
    {
      reply.success = true;
      auto logInsertIndex = args.prevLogIndex + 1;
      auto newEntriesIndex = 0;

      while ( true ) {
        if ( logInsertIndex >= (int32_t)state_.Logs.size() || newEntriesIndex >= (int32_t)args.entries.size() ) {
          break;
        }
        if ( state_.Logs[logInsertIndex].term != args.entries[newEntriesIndex].term ) {
          break;
        }
        logInsertIndex++;
        newEntriesIndex++;
      }

      if ( newEntriesIndex < (int32_t)args.entries.size() ) {
        for ( size_t i = logInsertIndex; i < state_.Logs.size(); ++i ) {
          state_.Logs[i].op.abort(); // release any pending service requests
        }
        state_.Logs.resize( logInsertIndex );
        for ( size_t i = newEntriesIndex; i < args.entries.size(); ++i ) {
          state_.Logs.push_back({
            .term = args.entries[i].term,
            .op = args.entries[i].op
          });
          // apply config change
          if ( args.entries[i].op.kind ==  RaftOp::OpType::ADD_SERVER ) {
            ServerInfo info = std::get<RaftOp::addserverarg_t>(args.entries[i].op.args);
            ApplyAddServer( info );
          } else if ( args.entries[i].op.kind ==  RaftOp::OpType::REMOVE_SERVER ) {
            int serverId = std::get<RaftOp::rmserverarg_t>(args.entries[i].op.args);
            ApplyRemoveServer( serverId );
          }

          if ( (int32_t)state_.Logs.size() - 1 != args.entries[i].index ) {
            LogError("mismatch of index");
          }
        }
        state_.Logs.persist();
      }

      if ( args.leaderCommit > state_.CommitIndex ) {
        // this means we have new jobs that can now be committed
        state_.CommitIndex = std::min( args.leaderCommit, (int32_t) state_.Logs.size() - 1 );
        std::lock_guard<std::mutex> rom(raftOutMutex_);
        // queue all jobs that can be committed to be fed to the executer
        for ( int32_t i = state_.LastApplied + 1; i <= state_.CommitIndex; ++i ) {
          raftOut_.push_back( state_.Logs[i].op );
        }
        state_.LastApplied = state_.CommitIndex;
        // signal the executer to take care of the queued jobs
        moreExecJobsReady_.signal();
      }
    }
  }

  reply.term = state_.CurrentTerm;
  // @FIXME: remove, the below log is for debugging only
  if ( ! args.entries.empty() ) {
    LogInfo("Replying: " + reply.str());
  }
  // --
  return reply;
}

template <class T>
RequestVoteRet RaftManager<T>::RequestVote( RequestVoteParams args )
{
  LogInfo("Received " + args.str()); 
  std::lock_guard<std::mutex> lock(state_.Mut);
  RequestVoteRet ret;

  // check if it is a member
  if ( peers_.find(args.candidateId) == peers_.end() ) {
    LogInfo("Candidate is not a member");
    ret.term = state_.CurrentTerm;
    ret.voteGranted = false;
    return ret;
  }
  
  int lastLogIndex = state_.Logs.size() - 1;
  int lastLogTerm = -1;
  if ( lastLogIndex >= 0 ) {
    lastLogTerm = state_.Logs[lastLogIndex].term;
  }

  if ( args.term > state_.CurrentTerm ) {
    becomeFollower( args.term );
  }

  if ( args.term == state_.CurrentTerm && ( state_.VotedFor == -1 || state_.VotedFor == args.candidateId ) &&
       ( args.lastLogTerm > lastLogTerm || 
         ( args.lastLogTerm == lastLogTerm && args.lastLogIndex >= lastLogIndex ))) {
    // vote for candidate
    ret.voteGranted = true;
    state_.VotedFor = args.candidateId;
    state_.ElectionResetEvent = std::chrono::system_clock::now();
  } else {
    ret.voteGranted = false;
  }

  state_.persist();
  ret.term = state_.CurrentTerm;
  LogInfo("Replying to RequestVote from " + std::to_string(args.candidateId));
  LogInfo("Ret: " + ret.str());
  return ret;
}

template <class T>
AddServerRet RaftManager<T>::AddServer( AddServerParams args)
{
  LogInfo("Received " + args.str());
  std::unique_lock stateLock { state_.Mut };
  AddServerRet ret;

  if ( state_.Role != RaftRole::Leader ) {
    ret.errorCode = raft::ErrorCode::NOT_LEADER;
    ret.leaderAddr = getLastKnownLeaderRaftAddr();
    LogInfo("Not leader, returning " + ret.str());
    return ret;
  }
  int serverId = args.serverId;

  // check if server exist
  if ( state_.ClusterConfig.find(serverId) != state_.ClusterConfig.end() ) {
    ret.errorCode = raft::ErrorCode::SERVER_EXISTS;
    ret.leaderAddr = getLastKnownLeaderRaftAddr();
    LogInfo("Server exists, returning " + ret.str());
    return ret;
  }

  stateLock.unlock();

  ServerInfo info = {
    .id = serverId,
    .raft_port = args.raftPort,
    .db_port = args.dbPort,
  };
  strcpy(info.ip, args.ip);
  strcpy(info.name, args.name);
  std::string serverAddr = std::string(info.ip) + ":" + std::to_string(info.raft_port);

  // We have not implemented CatchUp logic here. So CatchUp happens
  // using the usual AppendEntries RPC. This work is tracked here:
  // https://github.com/ajain365/oh-my-db/issues/23

  // wait until previous config change is commited
  int retryCount = 0;
  LogInfo("Waiting for previous config change to be committed");
  
  while ( true ) {
    stateLock.lock();
    if ( state_.LastConfigChangeIndex == -1 || 
         state_.LastConfigChangeIndex <= state_.CommitIndex ) {
      break;
    }

    retryCount++;
    if ( retryCount == RAFT_MEMBERSHIP_WAIT_ITERS ) 
    {
      ret.errorCode = raft::ErrorCode::PREV_NOT_COMMITTED_TIMEOUT;
      ret.leaderAddr = getLastKnownLeaderRaftAddr();
      return ret;
    }
    stateLock.unlock();

    std::this_thread::sleep_for( std::chrono::milliseconds(100) );
  }
  LogInfo("Previous config change is committed");

  int prevConfigChangeIndex = state_.LastConfigChangeIndex;
  stateLock.unlock();

  raft::RaftOp op {
    .kind = raft::RaftOp::ADD_SERVER,
    .args = info,
    .promiseHandle = { }
  };

  // the operation gets submitted
  // leader apply config change in runOneLeaderIter
  // once the entry is applied to log (before committed)
  auto [ isSubmitted, leaderId ] = submit( op );
  if ( ! isSubmitted ) {
    ret.errorCode = raft::ErrorCode::OTHER;
    std::lock_guard<std::mutex> lock( state_.Mut );
    ret.leaderAddr = getLastKnownLeaderRaftAddr();
    return ret;
  }

  // wait until it is committed
  retryCount = 0;
  LogInfo("Waiting for the new config change to be committed");
  while (1) 
  {
    stateLock.lock();
    // config change could trigger leader to step down
    // since the newbie could have very large term
    // we add this check to speed up progress so 
    // we don't have to wait for timeout
    // Just for performance. Not necessary for correctness
    if ( state_.Role != RaftRole::Leader ) {
      ret.errorCode = raft::ErrorCode::NOT_LEADER;
      ret.leaderAddr = getLastKnownLeaderRaftAddr();
      return ret;
    }

    if ( prevConfigChangeIndex < state_.LastConfigChangeIndex &&
         state_.LastConfigChangeIndex <= state_.CommitIndex ) {
      break;
    }
    LogInfo("Waiting for the new config change to get committed...");

    retryCount++;
    if ( retryCount == RAFT_MEMBERSHIP_WAIT_ITERS ) 
    {
      ret.errorCode = raft::ErrorCode::CUR_NOT_COMMITTED_TIMEOUT;
      ret.leaderAddr = getLastKnownLeaderRaftAddr();
      return ret;
    }
    stateLock.unlock();

    std::this_thread::sleep_for( std::chrono::milliseconds(100) );
  }

  LogInfo("Committing new config change");
  ret.errorCode = raft::ErrorCode::OK;
  ret.leaderAddr = "";
  return ret;
}

template <class T>
RemoveServerRet RaftManager<T>::RemoveServer( RemoveServerParams args)
{
  LogInfo("Received " + args.str());
  std::unique_lock stateLock { state_.Mut };
  RemoveServerRet ret;

  if ( state_.Role != RaftRole::Leader ) {
    ret.errorCode = raft::ErrorCode::NOT_LEADER;
    ret.leaderAddr = getLastKnownLeaderRaftAddr();
    LogInfo("Not leader, returning " + ret.str());
    return ret;
  }

  int serverId = args.serverId;

  // check if server exists
  if ( state_.ClusterConfig.find( serverId ) == state_.ClusterConfig.end() ) {
    ret.errorCode = raft::ErrorCode::SERVER_NOT_FOUND;
    ret.leaderAddr = getLastKnownLeaderRaftAddr();
    LogInfo("Server not found, returning " + ret.str());
    return ret;
  }

  // wait until previous config change is commited
  int retryCount = 0;
  stateLock.unlock();
  while (1) {
    stateLock.lock();
    if ( state_.LastConfigChangeIndex == -1 || 
         state_.LastConfigChangeIndex <= state_.CommitIndex) {
      break;
    }

    retryCount++;
    if ( retryCount == RAFT_MEMBERSHIP_WAIT_ITERS ) 
    {
      ret.errorCode = raft::ErrorCode::PREV_NOT_COMMITTED_TIMEOUT;
      ret.leaderAddr = getLastKnownLeaderRaftAddr();
      return ret;
    }
    stateLock.unlock();

    std::this_thread::sleep_for( std::chrono::milliseconds(100) );
  }

  int prevConfigChangeIndex = state_.LastConfigChangeIndex;
  stateLock.unlock();

  raft::RaftOp op {
    .kind = raft::RaftOp::REMOVE_SERVER,
    .args = args.serverId,
    .promiseHandle = { }
  };

  // the operation gets submitted
  // leader apply config change in runOneLeaderIter
  // once the entry is applied to log (before committed)
  auto [ isSubmitted, leaderId ] = submit( op );
  if ( ! isSubmitted ) {
    LogInfo("Failed to submit remove server op");
    ret.errorCode = raft::ErrorCode::OTHER;
    ret.leaderAddr = getLastKnownLeaderRaftAddr();
    return ret;
  }

  // wait until it is committed
  retryCount = 0;
  while (1) 
  {
    stateLock.lock();
    if ( prevConfigChangeIndex < state_.LastConfigChangeIndex &&
         state_.LastConfigChangeIndex <= state_.CommitIndex ) {
      break;
    }

    retryCount++;
    if ( retryCount == RAFT_MEMBERSHIP_WAIT_ITERS ) 
    {
      ret.errorCode = raft::ErrorCode::CUR_NOT_COMMITTED_TIMEOUT;
      ret.leaderAddr = getLastKnownLeaderRaftAddr();
      return ret;
    }
    stateLock.unlock();

    std::this_thread::sleep_for( std::chrono::milliseconds(100) );
  }

  ret.errorCode = raft::ErrorCode::OK;
  ret.leaderAddr = "";

  if ( serverId == id_ ) {
    LogInfo("I am removed from the cluster, becoming follower");
    becomeFollower( -1 );
  }

  LogInfo("Good return");
  return ret;
}

template <class T>
void RaftManager<T>::ApplyAddServer( ServerInfo info )
{
  // caller should have acquired the state lock
  LogInfo("Applying add server " + std::to_string(info.id));
  state_.LastConfigChangeIndex = state_.Logs.size() - 1;
  state_.ClusterConfig[info.id] = info;
  state_.persist();
  if ( id_ != info.id )
  {
    state_.Mut.unlock();
    std::string addr = std::string(info.ip) + ":" + std::to_string(info.raft_port);
    addPeer( info.id, 
        std::make_unique<T>(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials())));
    state_.Mut.lock();
  }
}

template <class T>
void RaftManager<T>::ApplyRemoveServer( int serverId )
{
  // caller should have acquired the state lock
  LogInfo("Applying remove server " + std::to_string(serverId));
  state_.LastConfigChangeIndex = state_.Logs.size() - 1;
  state_.ClusterConfig.erase( serverId );
  state_.persist();
  if ( id_ != serverId )
  {
    state_.Mut.unlock();
    removePeer( serverId );
    state_.Mut.lock();
  }
}

// these are helpers, state should be locked before calling
template <class T>
void RaftManager<T>::becomeFollower( int term )
{
  LogInfo("Becoming Follower");
  state_.CurrentTerm = term;
  state_.Role = RaftRole::Follower;
  state_.VotedFor = -1;
  state_.ElectionResetEvent = std::chrono::system_clock::now();
  state_.persist();
}

template <class T>
void RaftManager<T>::becomeCandidate(int term)
{
  LogInfo("Becoming Candidate");
  state_.CurrentTerm = term;
  state_.Role = RaftRole::Candidate;
  state_.ElectionResetEvent = std::chrono::system_clock::now();
  state_.VotedFor = id_;
  state_.persist();
}

template <class T>
void RaftManager<T>::becomeLeader()
{
  LogInfo("Becoming Leader");
  state_.Role = RaftRole::Leader;
  state_.ElectionResetEvent = std::chrono::system_clock::now();
  state_.VotedFor = -1;
  state_.LastKnownLeaderId = id_;
  state_.NextIndex.clear();
  state_.MatchIndex.clear();
  // initialise leader state
  for ( auto& [id, _]: peers_ ) {
    state_.NextIndex[id] = state_.Logs.size();
    state_.MatchIndex[id] = -1;
  }
  state_.persist();
}

template <class T>
void RaftManager<T>::becomeDead()
{
  state_.Role = RaftRole::Dead;
  state_.persist();
  keepRunning_ = false;
  LogInfo("I'm dead. No one loves me.");
}
// -- end of role transition helpers

// this may feel like a long election timeout, it actually is
// but helps while coding and testing since it reduces the noise in logs
template <class T>
int32_t RaftManager<T>::getRandomElectionTimeout()
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> timeOutGen(3500, 5000);
  return timeOutGen( gen );
}

template <class T>
void RaftManager<T>::startElection()
{
  // state is already locked at this point
  becomeCandidate(state_.CurrentTerm + 1);

  LogInfo("Starting election for term: " + std::to_string(state_.CurrentTerm));
  LogInfo("Voted for: " + std::to_string(state_.VotedFor));

  int savedCurrentTerm = state_.CurrentTerm;

  // Send RequestVote RPCs to all peers and count votes
  state_.VotesReceived = 1; // vote for self
  
  for ( auto& [id, _] : peers_ ) {
    // parallel send RequestVote to all connected peers
    auto th = std::thread([id = id, savedCurrentTerm, this]{
      // this means we will wait here till the launching method
      // is done
      state_.Mut.lock();
      auto sendLastLogIndex = static_cast<int32_t>( state_.Logs.size() ) - 1;
      auto sendLastLogTerm = ! state_.Logs.empty() ? state_.Logs.back().term : -1;
      state_.Mut.unlock();

      raft::RequestVoteParams args = {
        .candidateId = id_,
        .term = savedCurrentTerm, // we are using the term with which we started
                                  // the election, so that if there is a new leader
                                  // our requests will get turned down
                                  // it will be a mess if half of our requests are for
                                  // one term and the remaining for another
        .lastLogIndex = sendLastLogIndex,
        .lastLogTerm = sendLastLogTerm
      };
      LogInfo("Sending RequestVote to PeerId=" + std::to_string( id ) + " " + args.str());
      auto replyOpt = peers_[id]->RequestVote( args );
      if ( ! replyOpt.has_value() ) {
        return; // rpc failed, we can't do anything, we shouldn't retry for now
      }

      auto reply = replyOpt.value();
      LogInfo("Received RequestVote Response from PeerId=" + std::to_string( id )
              + " " + reply.str());

      std::lock_guard<std::mutex> lock( state_.Mut );
      if ( state_.Role != RaftRole::Candidate ) {
        return;
      }

      if ( reply.term > savedCurrentTerm ) {
        becomeFollower( reply.term );
        return;
      } else if ( reply.term == savedCurrentTerm ) {
        if ( reply.voteGranted ) {
          state_.VotesReceived++;
          if ( state_.VotesReceived * 2 > state_.ClusterConfig.size() ) {
            LogInfo("Id=" + std::to_string(id_) + " elected as leader");
            becomeLeader();
          }
        }
      } 
    });
    th.detach();
  }

}

template <class T>
void RaftManager<T>::electionImpl()
{
  // Used for selecting election timeouts
  auto electionTimeoutMillis = getRandomElectionTimeout();

  while( keepRunning_ )
  {
    std::this_thread::sleep_for( std::chrono::milliseconds( electionTimeoutMillis ) );
    std::lock_guard<std::mutex> lock( state_.Mut );
    auto role = state_.Role;
    auto timedOut = std::chrono::system_clock::now() 
                      - state_.ElectionResetEvent > std::chrono::milliseconds( electionTimeoutMillis );
    switch ( role ) {
      case RaftRole::Leader:
        break;
      case RaftRole::Candidate:
        break;
      case RaftRole::Follower:
      {
        if ( timedOut ) {
          startElection();
        }
        break;
      }
      case RaftRole::Dead:
      {
        // we still need to figure out when to set replica as dead
        // keeping this role here because maybe it is useful during config change
        // if the replica is dead (how?)
        // we just turn everything off
        keepRunning_ = false;
        break;
      }
    }
  }
}
} // end namespace
