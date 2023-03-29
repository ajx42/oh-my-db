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

int main(int argc, char **argv)
{
  argparse::ArgumentParser program("server");
  program.add_argument("--config")
      .required()
      .help("Config file path");

  program.add_argument("--db_port")
      .required()
      .help("DB service port");

  program.add_argument("--id")
      .required()
      .help("should match the row idx (0-based)");

  try {
      program.parse_args( argc, argv );
  }
  catch (const std::runtime_error& err) {
      std::cerr << err.what() << std::endl;
      std::cerr << program;
      std::exit(1);
  }

  // parse arguments
  auto db_port = program.get<std::string>("--db_port");
  auto config_path = program.get<std::string>("--config");
  auto id = std::stoi(program.get<std::string>("--id"));

  auto servers = ParseConfig(config_path);

  auto printServer = [&]( std::string tag, auto&& id ) {
    LogInfo(tag +
            " ServerId=" + std::to_string(id) +
            " IP=" + servers[id].ip +
            " Port=" + std::to_string(servers[id].port));
  };

  auto selfDetails = servers[id];
  printServer("MyDetails", id);

  ReplicaManager::Instance().initialiseServices( servers, id );   
  
  ReplicaManager::Instance().start();

  if ( id == 0 ) {
  ReplicaManager::Instance().put(std::make_pair(1, 2) );

  std::cout << ReplicaManager::Instance().get(1).value_or(-1) << std::endl;
  }
  
  LogInfo("services started");

  while(1) { std::this_thread::sleep_for(std::chrono::seconds(10)); }
}
