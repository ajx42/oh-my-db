#pragma once

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
  AppendEntriesRet AppendEntries( AppendEntriesParams ) { return {}; }
  RequestVoteRet RequestVote( RequestVoteParams ) { return {}; }
};

using Follower = FollowerProxy;
}
