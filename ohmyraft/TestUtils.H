#pragma once

#include <optional>
#include <vector>
#include <iostream>
#include "ConsensusUtils.H"

namespace raft {

class FollowerProxy {
public:   
  FollowerProxy( int id ) : id_( id ) {}

  void copyOp( RaftOp op ) {
    std::cout << "[" << id_ << "]: " <<  op.str() << std::endl;
  }

private:
  int id_;
};

// This is just a proxy to test other stuff. Don't add logic here.
// The real deal is in RaftService and OhMyRaft
class RaftClientProxy {
public:
  std::optional<AppendEntriesRet> AppendEntries( AppendEntriesParams ) { return {}; }
  std::optional<RequestVoteRet> RequestVote( RequestVoteParams ) { return {}; }
};

using Follower = FollowerProxy;
}
