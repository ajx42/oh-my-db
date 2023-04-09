#include <iostream>
#include <string>
#include <argparse/argparse.hpp>


#include "OhMyConfig.H"
#include "DatabaseClient.H"
#include "DatabaseUtils.H"
#include "WowLogger.H"
#include "RaftService.H"

int main( int argc, char** argv )
{
  argparse::ArgumentParser program("partitionnet");
  program.add_argument("--config")
    .required()
    .help("Replica detail config file path");

  program.add_argument("--partition")
    .required()
    .help("string of 1s and 0s to define partition, please read inline comment");

  /*
   * --partition "11100" defines a partition with rep 0-2 in first partition and 
   * 3-4 in the next
   */

  try {
    program.parse_args( argc, argv );
  } catch ( const std::runtime_error& err ) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }

  auto configPath = program.get<std::string>( "--config" );
  auto opCfg      = program.get<std::string>( "--partition" );

  auto servers = ParseConfig( configPath );  
  
  std::map<int32_t, std::unique_ptr<RaftClient>> peers;
  
  for ( size_t i = 0; i < servers.size(); ++i ) {
    auto address = servers[i].ip + ":" + std::to_string(servers[i].raft_port);
    peers[i] = std::make_unique<RaftClient>(grpc::CreateChannel(
          address, grpc::InsecureChannelCredentials()));
  }

  std::vector<raft::PeerNetworkConfig> pVec[2];
  std::vector<int32_t> partition[2];

  for ( int32_t i = 0; i < std::min(opCfg.size(), servers.size()); ++i ) {
    auto partitionId = (int)(opCfg[i]-'0');
    partition[partitionId].push_back(i);
    pVec[partitionId ^ 1].push_back(  raft::PeerNetworkConfig{
      .peerId = i,
      .isEnabled = false,
      .isDelayed = false,
      .delayMs = 0
    } );
    pVec[partitionId].push_back(  raft::PeerNetworkConfig{
      .peerId = i,
      .isEnabled = true,
      .isDelayed = false,
      .delayMs = 0
    } );
  }

  for ( auto pid = 0; pid < 2; ++pid ) {
    for ( auto peerId : partition[pid] ) {
      LogInfo("Sending Network Update to PeerId=" + std::to_string( peerId ) );
      peers[peerId]->NetworkUpdate( pVec[pid] ); 
    }
  }

}
