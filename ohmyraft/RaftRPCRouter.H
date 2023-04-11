#pragma once

#include <optional>

#include "ConsensusUtils.H"
#include "OhMyConfig.H"
#include "RaftService.H"

#include <atomic>

namespace raft {

class RaftRPCRouter : public RaftClient {
public:
  template <class ...Args>
  RaftRPCRouter(Args&& ...args)
    : RaftClient( std::forward<Args>( args )... )
  {}

  std::optional<AppendEntriesRet> AppendEntries( AppendEntriesParams );
  std::optional<RequestVoteRet> RequestVote( RequestVoteParams );

  void setEnable( bool en ) { isEnabled_ = en; }
  void setIsDelayed( bool dl ) { isDelayed_ = dl; }
  void setDelayMs( int32_t dlms ) { delayMs_ = dlms; }

private:
  std::atomic<bool> isEnabled_ { true };
  std::atomic<bool> isDelayed_ { false };
  std::atomic<int32_t> delayMs_ { 0 };
};

inline std::optional<AppendEntriesRet>
RaftRPCRouter::AppendEntries( AppendEntriesParams prm )
{
  if ( ! isEnabled_.load() ) {
    return {};
  } else if ( isDelayed_.load() ) {
    std::this_thread::sleep_for( std::chrono::milliseconds( delayMs_.load() ) );
  } 
  return RaftClient::AppendEntries( prm );
}

inline std::optional<RequestVoteRet>
RaftRPCRouter::RequestVote( RequestVoteParams prm )
{
  if ( ! isEnabled_.load() ) {
    return {};
  } else if ( isDelayed_.load() ) {
    std::this_thread::sleep_for( std::chrono::milliseconds( delayMs_.load() ) );
  } 
  return RaftClient::RequestVote( prm );
}

} // end namespace raft
