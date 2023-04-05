#pragma once

#include <string>
#include <optional>
#include <utility>
#include <memory>
#include <chrono>

#include "DatabaseClient.H"

namespace ohmydb {

class ReplicatedDB {
public:
  ReplicatedDB( std::string serverAddr );

  std::optional<int32_t> get( int32_t key );
  bool put( std::pair<int32_t, int32_t> kvp );

private:
  static constexpr const int32_t MAX_TRIES = 1000;
  OhMyDBClient client_;
  std::string serverAddr_;
};

inline ReplicatedDB::ReplicatedDB( std::string serverAddr )
  : client_( grpc::CreateChannel( serverAddr, grpc::InsecureChannelCredentials() ) )
  , serverAddr_( serverAddr )
{ }

inline std::optional<int32_t> ReplicatedDB::get( int32_t key )
{
  auto iters = MAX_TRIES;
  while ( iters-- ) {
    // try until you find a leader
    auto retOpt = client_.Get( key );
    
    if ( ! retOpt.has_value() ) {
      LogError( "Failed to connect to DB server: RPC Failed" );
      return {};
    }

    auto ret = retOpt.value();

    switch ( ret.errorCode ) {
      case ErrorCode::NOT_LEADER: {
        if ( ret.leaderAddr == serverAddr_ ) {
          std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
        }
        LogWarn( "Contacted server(" + serverAddr_ + ") is not the leader. "
                 "Switching to: " + ret.leaderAddr );
        serverAddr_ = ret.leaderAddr;
        client_ = OhMyDBClient(
          grpc::CreateChannel( ret.leaderAddr, grpc::InsecureChannelCredentials() )
        );
        break;
      }
      case ErrorCode::KEY_NOT_FOUND: {
        return {};
      }
      case ErrorCode::OK: {
        return ret.value;
      }
    }
  }
  LogError( "Exceeded MAX_TRIES, could not find leader. Likely a bug in Consensus!");
  return {};
}

inline bool ReplicatedDB::put( std::pair<int32_t, int32_t> kvp )
{
  auto iters = MAX_TRIES;
  while ( iters-- ) {
    auto retOpt = client_.Put( kvp.first, kvp.second );

    if ( ! retOpt.has_value() ) {
      LogError( "Failed to connect to DB server: RPC Failed" );
      return false;
    }

    auto ret = retOpt.value();

    switch ( ret.errorCode ) {
      case ErrorCode::NOT_LEADER: {
        if ( ret.leaderAddr == serverAddr_ ) {
          std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
        }
        LogWarn( "Contacted server(" + serverAddr_ + ") is not the leader. "
                 "Switching to: " + ret.leaderAddr );
        serverAddr_ = ret.leaderAddr;
        client_ = OhMyDBClient(
          grpc::CreateChannel( ret.leaderAddr, grpc::InsecureChannelCredentials() )
        );
        break;
      }
      case ErrorCode::KEY_NOT_FOUND: {
        LogError( "Unexpected error code returned by server, for put." );
        return false;
      }
      case ErrorCode::OK: {
        return !! ret.value;
      }
    }
  }

  LogError( "Exceeded MAX_TRIES, could not find leader. Likely a bug in Consensus!");
  return false;
}


} // end namespace ohmydb
