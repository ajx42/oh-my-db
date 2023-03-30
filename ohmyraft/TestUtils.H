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

class RaftClientProxy {
public:
  AppendEntriesRet AppendEntries( AppendEntriesParams ) { return {}; }
  RequestVoteRet RequestVote( RequestVoteParams ) { return {}; }
};

using Follower = FollowerProxy;
}
