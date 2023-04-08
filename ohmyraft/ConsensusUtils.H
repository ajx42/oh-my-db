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
#include "OhMyConfig.H"

namespace raft {
enum ErrorCode: int32_t {
  OK = 0,
  NOT_LEADER = 1,
  PREV_NOT_COMMITTED_TIMEOUT = 2,
  CUR_NOT_COMMITTED_TIMEOUT = 3,
  SERVER_EXISTS = 4,    // for add server
  SERVER_NOT_FOUND = 5, // for remove server
  OTHER = 6
};

template <class KeyT, class ValT>
struct Operation {
  using getarg_t = KeyT;
  using putarg_t = std::pair<KeyT, ValT>;
  using addserverarg_t = ServerInfo;
  using rmserverarg_t = int; //todo
  using getres_t = std::optional<ValT>;
  using putres_t = bool;
  // @TODO: How to remove duplicate type? (getarg_t and rmserverarg_t both int)
  using arg_t = std::variant<getarg_t, putarg_t, addserverarg_t>;
  using res_t = std::variant<getres_t, putres_t>;
  
  enum OpType : int32_t { GET = 0, PUT = 1, ADD_SERVER = 2, REMOVE_SERVER = 3 };

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
    } else if ( kind == PUT ) {
      res = LevelDB<KeyT, ValT>::Instance().put(
              std::get<putarg_t>( args ) );
    } else if ( kind == ADD_SERVER ) {
      res = true;
    } else if ( kind == REMOVE_SERVER ) {
      res = true;
    } else {
      std::cout << "Unknown operation kind: " << kind << std::endl;
      abort();
      return;
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
    } else if ( kind == PUT ) {
      promise.set_value( false ); // put failed
    } else if ( kind == ADD_SERVER ) {
      promise.set_value( false );
    } else if ( kind == REMOVE_SERVER ) {
      promise.set_value( false );
    } else {
      std::cout << "Unknown operation kind: " << kind << std::endl;
      abort();
      return;
    }

    promiseHandle.reset();
  }

  std::string str() const {
    std::stringstream oss;
    oss << "Operation[ ";
    if ( kind == GET ) {
      oss << "GET(" << std::get<getarg_t>( args ) << ") ";
    } else if ( kind == PUT ) {
      auto putarg = std::get<putarg_t>( args );
      oss << "PUT(" << putarg.first << ", " << putarg.second << ") ";
    } else if ( kind == ADD_SERVER ) {
      oss << "ADD_SERVER(" << std::get<addserverarg_t>( args ).ip << ") ";
    } else if ( kind == REMOVE_SERVER ) {
      oss << "REMOVE_SERVER(" << std::get<rmserverarg_t>( args ) << ") ";
    } else {
      oss << "UNKNOWN_OP(" << kind << ") ";
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
  ServerInfo serverInfo;
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

struct AddServerParams {
  int serverId;
  char ip[20];
  int raftPort;
  int dbPort;
  char name[20];

  std::string str() const;
};

inline std::string AddServerParams::str() const {
  std::stringstream ss;
  ss  << "AddServerParams=["
      << "ServerId=" << serverId << " "
      << "IP=" << ip << " "
      << "RaftPort=" << raftPort << " "
      << "DBPort=" << dbPort << " "
      << "Name=" << name << "]";
  return ss.str();
}

struct AddServerRet {
  raft::ErrorCode errorCode;
  std::string leaderAddr;

  std::string str() const;
};

inline std::string AddServerRet::str() const {
  std::stringstream ss;
  ss  << "AddServerRet=["
      << "ErrorCode=" << errorCode << " "
      << "LeaderAddr=" << leaderAddr << "]";
  return ss.str();
}

struct RemoveServerParams {
  int serverId;

  std::string str() const;
};

inline std::string RemoveServerParams::str() const {
  std::stringstream ss;
  ss  << "RemoveServerParams=["
      << "ServerId=" << serverId << "]";
  return ss.str();
}

struct RemoveServerRet {
  raft::ErrorCode errorCode;
  std::string leaderAddr;

  std::string str() const;
};

inline std::string RemoveServerRet::str() const {
  std::stringstream ss;
  ss  << "RemoveServerRet=["
      << "ErrorCode=" << errorCode << " "
      << "LeaderAddr=" << leaderAddr << "]";
  return ss.str();
}

} // end namespace 
