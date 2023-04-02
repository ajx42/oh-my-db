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

namespace raft {

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
  std::vector<LogEntry> Logs;

  // volatile state
  RaftRole Role;
  std::chrono::time_point<std::chrono::system_clock> ElectionResetEvent;
  int32_t CommitIndex; // 
  int32_t LastApplied; // I am not sure why this is not persistent
  
  // for leaders only peer id -> next/match index
  std::map<int32_t, int32_t> NextIndex;
  std::map<int32_t, int32_t> MatchIndex;

  // for candidate only, non standard
  int32_t VotesReceived;

};

template <class ClientT>
class RaftManager
{
public:
  RaftManager() { 
    state_.CurrentTerm = 0;
    state_.VotedFor = -1;
    state_.Logs.reserve( 10000 );
    state_.Role = RaftRole::Follower;
    state_.CommitIndex = -1;
    state_.LastApplied = -1;
    state_.VotesReceived = 0;
  }

  ~RaftManager();

  void initialiseConnections( int32_t myId, std::map<int32_t, std::unique_ptr<ClientT>>&& peers );

  // control points
  void start();
  void stop();

  // job submission
  bool submit( RaftOp op );

  // raft rpc implementations
  AppendEntriesRet AppendEntries( AppendEntriesParams );
  RequestVoteRet RequestVote( RequestVoteParams );

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
  void runLeaderOneIter();


  int32_t getRandomElectionTimeout();
  void startElection();
};

// no need for locking state, this is to be called before start is called
template <class T>
void RaftManager<T>::initialiseConnections(
    int32_t myId, std::map<int32_t, std::unique_ptr<T>>&& peers )
{
  id_ = myId;
  peers_ = std::move(peers);
  for ( auto& [id, _]: peers_ ) {
    state_.NextIndex[id] = 0;
    state_.MatchIndex[id] = -1;
  }
}

template <class T>
bool RaftManager<T>::submit( RaftOp op )
{
  std::unique_lock stateLock { state_.Mut };
  if ( state_.Role != RaftRole::Leader ) {
    LogError("This Replica is not the leader. Job can't be submitted.");
    return false;
  }
  stateLock.unlock();

  std::lock_guard<std::mutex> lock( raftInMutex_ );
  dispatchOut_.push_back( op );
  moreInputsReady_.signal();
  return true;
}

template <class T>
void RaftManager<T>::runLeaderOneIter()
{
  // currently just heartbeating
  AppendEntriesParams args {};
  for ( auto& [id, _] : peers_ ) {
    auto th = std::thread([id = id, this, args](){
      peers_[id]->AppendEntries( args );
    });
    th.detach();
  }
}

template <class T>
void RaftManager<T>::raftImpl()
{
  while ( keepRunning_ ) {
    //moreInputsReady_.wait();
    raftInMutex_.lock();
    std::swap( dispatchOut_, raftIn_ ); 
    raftInMutex_.unlock();

    // @FIXME: All this logic is a placeholder till we actually build out RAFT.
    state_.Mut.lock();
    auto role = state_.Role;
    state_.Mut.unlock();
    if ( role == RaftRole::Leader ) {
      // sending heartbeats
      runLeaderOneIter();
    }

    // @FIXME: temporarily just forwarded received ops to the executer.
    // Ideally only the committed entries will be sent.
    raftOutMutex_.lock();
    for ( auto op: raftIn_ ) {
      raftOut_.push_back( op );
    }
    if ( ! raftOut_.empty() ) {
      raftOutMutex_.unlock();
      moreExecJobsReady_.signal(); 
    } else {
      raftOutMutex_.unlock();
    }

    // sleep for a while
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
   
    // prepare to receive more
    raftIn_.clear();
  }
}

template <class T>
void RaftManager<T>::executerImpl()
{
  while ( keepRunning_ ) {
    moreExecJobsReady_.wait();
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
    state_.ElectionResetEvent = std::chrono::system_clock::now();
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
  
  std::lock_guard<std::mutex> lock(state_.Mut);
  RequestVoteRet ret;
  
  int lastLogIndex = state_.Logs.size() - 1;
  int lastLogTerm = -1;
  if ( lastLogIndex >= 0 ) {
    lastLogTerm = state_.Logs[lastLogIndex].term;
  }

  if ( args.term > state_.CurrentTerm ) {
    // become follower
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

  ret.term = state_.CurrentTerm;
  LogInfo("Replying to RequestVote from " + std::to_string(args.candidateId));
  LogInfo("Ret: " + ret.str());
  return ret;
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
}

template <class T>
void RaftManager<T>::becomeCandidate(int term)
{
  LogInfo("Becoming Candidate");
  state_.CurrentTerm = term;
  state_.Role = RaftRole::Candidate;
  state_.ElectionResetEvent = std::chrono::system_clock::now();
  state_.VotedFor = id_;
}

template <class T>
void RaftManager<T>::becomeLeader()
{
  LogInfo("Becoming Leader");
  state_.Role = RaftRole::Leader;
  state_.ElectionResetEvent = std::chrono::system_clock::now();
  state_.VotedFor = -1;
  state_.NextIndex.clear();
  state_.MatchIndex.clear();
  // initialise leader state
  for ( auto& [id, _]: peers_ ) {
    state_.NextIndex[id] = state_.Logs.size();
    state_.MatchIndex[id] = -1;
  }
}
// -- end of role transition helpers

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
          if ( state_.VotesReceived * 2 > peers_.size() ) {
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
        break; // we shouldn't be here ideally
      case RaftRole::Follower:
      {
        if ( timedOut ) {
          startElection();
        }
        break;
      }
      case RaftRole::Dead:
      {
        // if the replica is dead (how?)
        // we just turn everything off
        keepRunning_ = false;
        break;
      }
    }
  }
}
} // end namespace
