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
  int32_t AppendEntries( std::vector<TransportEntry> ) { return 0; }
};

using Follower = FollowerProxy;
}
