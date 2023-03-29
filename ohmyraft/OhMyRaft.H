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
  
  void attachPeers( std::vector<ClientT>&& peers ) {
    peers_ = std::move(peers);
    LogInfo("attaching peers");
  }

  void start();
  void submit( RaftOp op );
  void raftImpl();
  void executerImpl();
  void stop();

  ~RaftManager();

private:
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

    std::vector<TransportEntry> transportVec;
    
    int cnt = 0;
    for ( const auto& op : raftIn_ ) {
      transportVec.push_back( TransportEntry {
        .term = 0,
        .index = cnt++,
        .kind = op.kind == RaftOp::GET ? 0 : 1,
        .arg1 = op.kind == RaftOp::GET ? std::get<RaftOp::getarg_t>( op.args ) : std::get<RaftOp::putarg_t>( op.args ).first,
        .arg2 = op.kind == RaftOp::GET ? 0 : std::get<RaftOp::putarg_t>( op.args ).second
      });
    }
  
    if ( ! transportVec.empty() ) {
      for ( auto& peer: peers_ ) {
        peer.AppendEntries( transportVec ); 
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



} // end namespace
