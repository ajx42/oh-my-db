#pragma once

#include <vector>
#include <optional>
#include <utility>
#include <future>
#include <variant>
#include <string>
#include <sstream>
#include <iostream>

#include "PromiseStore.H"
#include "ohmydb/LevelDBProxy.H"

namespace raft {

template <class KeyT, class ValT>
struct Operation {
  using getarg_t = KeyT;
  using putarg_t = std::pair<KeyT, ValT>;
  using getres_t = std::optional<ValT>;
  using putres_t = bool;
  using arg_t = std::variant<getarg_t, putarg_t>;
  using res_t = std::variant<getres_t, putres_t>;
  
  enum OpType : int32_t { GET = 0, PUT = 1 };

  OpType kind;
  arg_t args;
  std::optional<typename PromiseStore<res_t>::handle_t> promiseHandle;

  Operation<KeyT, ValT> withoutPromise() const {
    auto copy = *this;
    copy.promiseHandle.reset();
    return copy;
  }

  void execute() {
    std::cout << "EXEC: " << str() << std::endl;
  
    res_t res;
    if ( kind == GET ) {
      res = LevelDB<KeyT, ValT>::Instance().get(
              std::get<getarg_t>( args ) );
    } else {
      res = LevelDB<KeyT, ValT>::Instance().put(
              std::get<putarg_t>( args ) );
    }
    if ( promiseHandle.has_value() ) {
      // we have a promise to fulfill
      auto promise = PromiseStore<res_t>::Instance().getAndRemove(
          promiseHandle.value() );
      promise.set_value( res );
      promiseHandle.reset();
    }
  }

  void abort() {
    if ( ! promiseHandle.has_value() ) {
      // there is no promise, so we don't need to service
      return;
    }
    auto promise = PromiseStore<res_t>::Instance().getAndRemove(
        promiseHandle.value() );
    if ( kind == GET ) {
      promise.set_value({}); // get failed
    } else {
      promise.set_value( false ); // put failed
    }
    promiseHandle.reset();
  }

  std::string str() const {
    std::stringstream oss;
    oss << "Operation[ ";
    if ( kind == GET ) {
      oss << "GET(" << std::get<getarg_t>( args ) << ") ";
    } else {
      auto putarg = std::get<putarg_t>( args );
      oss << "PUT(" << putarg.first << ", " << putarg.second << ") ";
    }
    oss << "HasPromise=" << promiseHandle.has_value() << " ]";
    return oss.str();
  }

  ~Operation() {
  }
} __attribute__((__packed__));

// We are only going to care for these int int KVP ops
using RaftOp = Operation<int, int>;

struct LogEntry {
  int term;
  RaftOp op;
  std::string str() const;
} __attribute__((__packed__));

inline std::string LogEntry::str() const
{
  std::stringstream ss;
  ss  << "LogEntry=["
      << "Term=" << term << " "
      << "Op=" << op.str() << "]";
  return ss.str();
}

struct TransportEntry {
  int32_t term;
  int32_t index;
  RaftOp::OpType kind;
  int32_t arg1;
  int32_t arg2;
} __attribute__((__packed__));

enum class Role 
{
  Follower,
  Candidate,
  Leader,
  Dead,
};

struct AppendEntriesParams {
  struct AppendLogEntry {
    int32_t term;
    int32_t index;
    RaftOp op;

    std::string str() const;
  };
  int32_t term;
  int32_t leaderId;
  int32_t prevLogIndex;
  int32_t prevLogTerm;
  std::vector<AppendLogEntry> entries;
  int32_t leaderCommit;

  std::string str() const;
};

inline std::string AppendEntriesParams::str() const
{
  std::stringstream ss;
  ss  << "AppendEntriesParam={"
      << "Term="          << term         << " "
      << "LeaderId="      << leaderId     << " "
      << "PrevLogIndex="  << prevLogIndex << " "
      << "PrevLogTerm="   << prevLogTerm  << " "
      << "LeaderCommit="  << leaderCommit << " "
      << "Entries=[ ";
  for ( auto e: entries ) {
    ss << e.str() << " ";
  }
  ss  << "]}";
  return ss.str();
}

inline std::string AppendEntriesParams::AppendLogEntry::str() const
{
  std::stringstream ss;
  ss  << "AppendLogEntry={"
      << "Term=" << term << " "
      << "Index=" << index << " "
      << "Op=" << op.str() << "}";
  return ss.str();
}

struct AppendEntriesRet {
  int32_t term;
  bool success;

  std::string str() const;
};

inline std::string AppendEntriesRet::str() const
{
  std::stringstream ss;
  ss  << "AppendEntriesRet=["
      << "Term=" << term << " "
      << "Success=" << success << "]";
  return ss.str();
}

struct RequestVoteParams {
  int candidateId;
  int term;
  int lastLogIndex;
  int lastLogTerm;

  std::string str() const;
};

inline std::string RequestVoteParams::str() const {
  std::stringstream ss;
  ss  << "RequestVoteParams=["
      << "CandidateId=" << candidateId << " "
      << "Term=" << term << " "
      << "LastLogIndex=" << lastLogIndex << " "
      << "LastLogTerm=" << lastLogTerm << "]";
  return ss.str();
}

struct RequestVoteRet {
  int term;
  bool voteGranted;
  
  std::string str() const;  
};

inline std::string RequestVoteRet::str() const {
  std::stringstream ss;
  ss  << "RequestVoteRet=["
      << "Term=" << term << " "
      << "VoteGranted=" << voteGranted << "]";
  return ss.str();
}

struct PeerNetworkConfig {
  int32_t peerId;
  bool isEnabled = true;
  bool isDelayed = false;
  int32_t delayMs = 0;

  std::string str() const;
} __attribute__((__packed__));

inline std::string PeerNetworkConfig::str() const {
  std::stringstream ss;
  ss  << "PeerNetworkConfig=["
      << "PeerId=" << peerId << " "
      << "IsEnabled=" << isEnabled << " "
      << "IsDelayed=" << isDelayed << " "
      << "DelayMS=" << delayMs << "]";
  return ss.str();
}

} // end namespace 
