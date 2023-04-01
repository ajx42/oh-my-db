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

#include "PromiseStore.H"
#include "ConsensusUtils.H"
#include "TestUtils.H"
#include "WowLogger.H"

namespace raft {

enum raft_role {follower, candidate, leader};
struct RaftState
{
  int PeerId = 0;
  std::chrono::time_point<std::chrono::system_clock> electionResetEvent;
  std::chrono::milliseconds electionTimeout;
  //raft election state data
  int votedFor;
  int term;
  raft_role role;
  std::vector<int> nextIndex;
  std::vector<int> matchIndex;
};

template <class ClientT>
class RaftManager
{
public:
  RaftManager( int id ) { 
    state_.PeerId = id;
    state_.term = 0;
    state_.votedFor = -1;
    logs_.reserve( 10000 ); 
  }

  RaftManager() {}
  ~RaftManager();

  void attachPeers( std::vector<ClientT>&& peers ) {
    peers_ = std::move(peers);
    LogInfo("attaching peers");
    state_.nextIndex = std::vector<int>(peers_.size(), 0);
    state_.matchIndex = std::vector<int>(peers_.size(), -1);
  }

  // configuration
  void setId( int id ) {
    state_.PeerId = id;
  }

  // control points
  void start();
  void stop();

  void busy();

  // job submission
  void submit( RaftOp op );

  // raft rpc implementations
  AppendEntriesRet AppendEntries( AppendEntriesParams );
  RequestVoteRet RequestVote( RequestVoteParams );

private:  
  // raft core logic implementation
  // these must be run on separate threads
  void raftImpl();
  void executerImpl();

  std::vector<ClientT> peers_;
  std::vector<LogEntry> logs_;

  std::list<RaftOp> dispatchOut_, raftIn_, raftOut_, execIn_;

  std::mutex raftOutMutex_;
  std::mutex raftInMutex_;
  std::mutex raftStateMutex_;

  std::thread raft;
  std::thread exec;

  bool hasStarted = false;
  bool keepRunning_ = false;

  RaftState state_;
  
  void BecomeLeader();
  void BecomeFollower(int);
  void BecomeCandidate(int);

  std::chrono::milliseconds GetElectionTimeout();
  void StartElection();
};

template <class T>
void RaftManager<T>::submit( RaftOp op )
{
  std::lock_guard<std::mutex> lock( raftInMutex_ );
  dispatchOut_.push_back( op );
}

template <class T>
void RaftManager<T>::raftImpl()
{
  while ( keepRunning_ ) {
    raftInMutex_.lock();
    std::swap( dispatchOut_, raftIn_ ); 
    raftInMutex_.unlock();

    // @FIXME: All this logic is a placeholder till we actually build out RAFT.
 
    // append received entries to logs
    for ( const auto& op : raftIn_ ) {
      logs_.push_back( LogEntry {
        .term = 0,
        .op = op,
      });
    }

    AppendEntriesParams args;
    
    int cnt = 0;
    for ( const auto& op : raftIn_ ) {
      args.entries.push_back( AppendEntriesParams::AppendLogEntry {
        .term = 0,
        .index = cnt++,
        .op = op.withoutPromise(),
      });
    }
  
    if ( state_.PeerId  == 0 && ! args.entries.empty() ) {
      for ( auto& peer: peers_ ) {
        peer.AppendEntries( args ); 
      }
    }
  
    raftOutMutex_.lock();
    for ( auto op: raftIn_ ) {
      raftOut_.push_back(op);
    }
    raftOutMutex_.unlock();
    
    // prepare to receive more
    raftIn_.clear();
  }
}

template <class T>
void RaftManager<T>::executerImpl()
{
  while ( keepRunning_ ) {
    raftOutMutex_.lock();
    std::swap( execIn_, raftOut_ );
    raftOutMutex_.unlock();

    for ( auto op: execIn_ ) {
      op.execute();
    }
    execIn_.clear();
  }
}

template <class T>
void RaftManager<T>::start()
{
  keepRunning_ = true;
  exec = std::thread([this]{executerImpl();});
  raft = std::thread([this]{raftImpl();});
}

template <class T>
void RaftManager<T>::stop()
{
  if ( ! keepRunning_ ) {
    return;
  }
  keepRunning_ = false;
  raft.join();
  exec.join();
}

template <class T>
RaftManager<T>::~RaftManager()
{
  stop();
}

// RAFT Standard RPC Implementations. @TODO: Note the structs for args and results are
// placeholders, please fill them up ConsensusUtils.H.
// Furthermore, These calls land in RPCServiceImpl.C where data is repackaged to RPC-able
// format. So after implementing here, the packaging logic also needs to be implemented.
// Note calls can happen in parallel, so ensure thread safety while accessing state.

template <class T>
AppendEntriesRet RaftManager<T>::AppendEntries( AppendEntriesParams args )
{
  // check if heartbeat
  if ( args.entries.empty() ) {
    state_.electionResetEvent = std::chrono::system_clock::now();
    return {};
  }

  for ( auto e: args.entries ) {
    std::cout << e.op.str() << std::endl;
    submit( e.op );
  }
  return {};
}

template <class T>
RequestVoteRet RaftManager<T>::RequestVote( RequestVoteParams args )
{
  LogInfo("RequestVote from " + std::to_string(args.candidateId));
  LogInfo("term: " + std::to_string(args.term) + 
          " lastLogTerm: " + std::to_string(args.lastLogTerm) + 
          " lastLogIndex: " + std::to_string(args.lastLogIndex))
  
  RequestVoteRet ret;

  int lastLogIndex = logs_.size() - 1;
  int lastLogTerm = -1;
  if ( lastLogIndex >= 0 ) {
    lastLogTerm = logs_[lastLogIndex].term;
  }

  if ( args.term > state_.term ) {
    // become follower
    state_.term = args.term;
    state_.role = follower;
    state_.votedFor = -1;
  }

  if ( args.term == state_.term && ( state_.votedFor == -1 || state_.votedFor == args.candidateId ) &&
       ( args.lastLogTerm >= lastLogTerm || 
         ( args.lastLogTerm == lastLogTerm && args.lastLogIndex >= lastLogIndex ))) {
    // vote for candidate
    ret.voteGranted = true;
    state_.votedFor = args.candidateId;
    state_.electionResetEvent = std::chrono::system_clock::now();
  } else {
    ret.voteGranted = false;
  }

  ret.term = state_.term;
  LogInfo("Replying to RequestVote from " + std::to_string(args.candidateId));
  LogInfo("term: " + std::to_string(ret.term) + 
          " voteGranted: " + std::to_string(ret.voteGranted))

  return ret;
}

template <class T>
void RaftManager<T>::BecomeFollower( int term )
{
  state_.term = term;
  state_.role = follower;
  state_.votedFor = -1;
}

template <class T>
void RaftManager<T>::BecomeCandidate(int term)
{
  state_.term = term;
  state_.role = candidate;
  state_.electionResetEvent = std::chrono::system_clock::now();
  state_.electionTimeout = GetElectionTimeout();
  state_.votedFor = state_.PeerId;
}

template <class T>
void RaftManager<T>::BecomeLeader()
{
  state_.role = leader;
  state_.electionResetEvent = std::chrono::system_clock::now();
  state_.electionTimeout = GetElectionTimeout();
  state_.votedFor = -1;
  // @TODO: nextIndex, matchIndex
  state_.nextIndex = std::vector<int>(peers_.size(), logs_.size());
  state_.matchIndex = std::vector<int>(peers_.size(), -1);
}

template <class T>
std::chrono::milliseconds RaftManager<T>::GetElectionTimeout()
{
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> timeOutGen(150, 300);
  std::chrono::milliseconds timeOut = std::chrono::milliseconds{timeOutGen(gen)};
  return timeOut;
}

template <class T>
void RaftManager<T>::StartElection()
{
  // become candidate
  BecomeCandidate(state_.term + 1);

  LogInfo("Starting election for term: " + std::to_string(state_.term));
  LogInfo("Voted for: " + std::to_string(state_.votedFor));

  int startTerm = state_.term;
  int lastLogIndex = logs_.size() - 1;
  int lastLogTerm = -1;
  if ( lastLogIndex >= 0 ) {
    lastLogTerm = logs_[lastLogIndex].term;
  }

  // Send RequestVote RPCs to all peers and count votes
  int votesReceived = 1; // vote for self
  raft::RequestVoteParams args = {
    .candidateId = state_.PeerId,
    .term = state_.term,
    .lastLogIndex = lastLogIndex,
    .lastLogTerm = lastLogTerm
  };
  for ( auto& peer: peers_ ) {
    raft::RequestVoteRet response = peer.RequestVote( args );

    // check timeout
    if ( std::chrono::system_clock::now() - state_.electionResetEvent > state_.electionTimeout ) {
      return; // election timed out, will start a new one in the next iteration of busy()
    }

    // check if we are no longer candidate (new leader exist and we get heartbeat)
    if ( state_.role != candidate ) {
      return;
    }

    // check if new term
    if ( response.term > startTerm ) {
      BecomeFollower( response.term );
      return;
    }

    if ( response.term == startTerm && response.voteGranted ) {
      votesReceived++;

      // check if majority votes received
      if ( votesReceived > peers_.size() / 2 ) {
        BecomeLeader();
        return;
      }
    }
  }

}


template <class T>
void RaftManager<T>::busy()
{
  // start off as follower
  BecomeFollower(0);

  // Used for selecting election timeouts
  size_t leaderHeartBeatInterval = 100;

  while (1)
  {
    if (state_.role == leader)
    {
      // Send heartbeat RPCs to all peers
      raft::AppendEntriesParams args = {
        .term = state_.term,
        .leaderId = state_.PeerId,
        .prevLogIndex = -1, // not important
        .prevLogTerm = -1, // not important
        .entries = {},
        .leaderCommit = -1, // not important
      };
      for (auto& peer : peers_)
      {
        peer.AppendEntries(args);
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(leaderHeartBeatInterval));
    }
    else if (state_.role == follower)
    {
      if (std::chrono::system_clock::now() - state_.electionResetEvent > state_.electionTimeout)
      {
        // No heartbeat => lection timeout, start election
        StartElection();
      }
    }
    else if (state_.role == candidate)
    {
      StartElection();
    }
  }
}
} // end namespace
