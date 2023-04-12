#include <iostream>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <chrono>

#include <argparse/argparse.hpp>

#include "WowLogger.H"
#include "OhMyConfig.H"
#include "OhMyRaft.H"
#include "RaftService.H"
#include "OhMyReplica.H"

//Leader election notes:
//Server starts in follower state, maintains follower
//state so long as it receives heartbeat from leader
//If no heartbeat within election timeout phase,
//then server begins election to choose new leader.

//To begin election, follower increments current term
//and switches to "canidate" state. Votes for itself
//and requests votes from others.

//Three conditions can break election,
//* the server wins
//* another server wins
//* nobody wins within a given time period

//When a candidate wins, it sends a heartbeat to all
//other servers to establish its rule

//If a candidate receives a heartbeat from a server
//with a term >= the candidate's term, it accepts it as leader.
//Otherwise, reject the heartbeat and continue the election.

//To prevent repeat splits, election time outs are chosen
//randomly (150-300ms in the paper). 

//I think there needs to be a further grey period on this though
//for restarting an election. Each node has a randomized
//election timeout time, where if the election fails,
//it then waits for a random amount of time before
//trying again? Essentially two election timeouts

int main(int argc, char **argv)
{
  argparse::ArgumentParser program("server");
  program.add_argument("--config")
      .required()
      .help("Config file path");

  program.add_argument("--id")
      .required()
      .help("should match the row idx (0-based)");

  program.add_argument("--db_path")
      .required()
      .help("leveldb file path");

  program.add_argument("--bootstrap")
      .help("bootstrap from existing state")
      .default_value( false )
      .implicit_value( true );

  program.add_argument("--storedir")
      .help("directory to store persistent raft state in")
      .default_value("/tmp/");

  program.add_argument("--addedNode")
      .help("whether or not this node is being added to an existing cluster")
      .default_value( false )
      .implicit_value( true );
  
  program.add_argument("--ip")
      .help("IP address of the node. Only needed when addedNode is true.")
      .default_value("");
   
  program.add_argument("--raft_port")
      .help("Raft port of the node. Only needed when addedNode is true.")
      .default_value("-1");

  program.add_argument("--db_port")
      .help("DB port of the node. Only needed when addedNode is true.")
      .default_value("-1");
    
  program.add_argument("--quicktest")
      .help("generates two ops after startup for a quick test")
      .default_value( false )
      .implicit_value( true );

  try {
      program.parse_args( argc, argv );
  }
  catch (const std::runtime_error& err) {
      std::cerr << err.what() << std::endl;
      std::cerr << program;
      std::exit(1);
  }

  // parse arguments
  auto config_path = program.get<std::string>("--config");
  auto id = std::stoi(program.get<std::string>("--id"));
  auto db_path = program.get<std::string>("--db_path");
  auto store_dir = program.get<std::string>("--storedir");  
  auto enableBootstrap = program["--bootstrap"] == true;
  auto isAddedNode = program["--addedNode"] == true;
  auto ip = program.get<std::string>("--ip");
  auto raft_port = std::stoi(program.get<std::string>("--raft_port"));
  auto db_port = std::stoi(program.get<std::string>("--db_port"));
  auto enableQuickTest = program["--quicktest"] == true;

  auto servers = ParseConfig(config_path);

  auto printServer = [&]( std::string tag, auto&& id ) {
    LogInfo(tag +
            " ServerId=" + std::to_string(id) +
            " IP=" + servers[id].ip +
            " Raft Port=" + std::to_string(servers[id].raft_port) +
            " DB Port=" + std::to_string(servers[id].db_port));
  };


  if ( ! isAddedNode ) {
    printServer("ServerDetails", id);
    ReplicaManager::Instance().initialiseServices(
        servers, id, true, db_path, enableBootstrap, store_dir);
  } else {
    ReplicaManager::Instance().initialiseServices(
      servers, id, false, db_path, enableBootstrap, store_dir,
      ip, raft_port, db_port );  
  }
  
  // start up the replica
  ReplicaManager::Instance().start();

  std::this_thread::sleep_for(std::chrono::seconds(5));

  // -- @FIXME: remove once done, for test only
  if ( enableQuickTest ) {
    ReplicaManager::Instance().put( std::make_pair(1, 2) );
    LogInfo( "Received Output: " + std::to_string( ReplicaManager::Instance().get(1).value ) );
  }
  // -- 

  while( 1 ) { std::this_thread::sleep_for(std::chrono::seconds(5)); }
}
