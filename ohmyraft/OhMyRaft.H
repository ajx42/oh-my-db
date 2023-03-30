#pragma once

#include <list>
#include <future>
#include <utility>
#include <optional>
#include <map>
#include <thread>
#include <mutex>
#include <vector>

#include "PromiseStore.H"
#include "ConsensusUtils.H"
#include "TestUtils.H"
#include "WowLogger.H"

namespace raft {

struct RaftState
{
  int PeerId = 0;
};

template <class ClientT>
class RaftManager
{
public:
  RaftManager( int id ) { 
    state_.PeerId = id;
    logs_.reserve( 10000 ); 
  }

  RaftManager() {}
  ~RaftManager();

  void attachPeers( std::vector<ClientT>&& peers ) {
    peers_ = std::move(peers);
    LogInfo("attaching peers");
  }

  // configuration
  void setId( int id ) {
    state_.PeerId = id;
  }

  // control points
  void start();
  void stop();

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

template <class T>
AppendEntriesRet RaftManager<T>::AppendEntries( AppendEntriesParams args )
{
  for ( auto e: args.entries ) {
    std::cout << e.op.str() << std::endl;
    submit( e.op );
  }
  return {};
}

template <class T>
RequestVoteRet RaftManager<T>::RequestVote( RequestVoteParams args )
{
  (void) args;
  return {};
}
} // end namespace
