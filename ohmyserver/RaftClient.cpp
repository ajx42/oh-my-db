#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <unistd.h>
#include <memory>
#include <argparse/argparse.hpp>
#include <chrono>
#include <cstring>

#include "OhMyConfig.H"
#include "WowLogger.H"
#include "RaftService.H"
#include "ConsensusUtils.H"
#include <random>


int main(int argc, char **argv)
{
    argparse::ArgumentParser program("client");
    program.add_argument("--config")
        .required()
        .help("Config file.");

    program.add_argument("--id")
        .default_value("0")
        .help("Initial node to contact.");

    try {
        program.parse_args( argc, argv );
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }


    // parse arguments
    auto configPath = program.get<std::string>("--config");
    auto id = std::stoi(program.get<std::string>("--id"));

    auto servers = ParseConfig(configPath);
    auto serverAddr = std::string(servers[id].ip) + ":" + std::to_string(servers[id].raft_port);

    LogInfo("Client: Starting contact on " + serverAddr);

    auto raftClient = RaftClient( grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials()) );
    raft::AddServerParams params;
    params.serverId = 4;
    strcpy(params.ip, "10.10.1.1");
    params.dbPort = 12349;
    params.raftPort = 8084;
    strcpy(params.name, "node4");

    raft::RemoveServerParams rm_params;
    rm_params.serverId = 3;

    auto ret = raftClient.RemoveServer( rm_params );
    if ( ret.has_value() ) {
        std::cout << ret.value().str() << std::endl;
    } else {
        std::cout << "NOT_FOUND" << std::endl;
        return 0;
    }
    
    // sleep for a bit
    std::cout << "Sleeping for 15 seconds" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(15));

    auto ret2 = raftClient.AddServer( params );
    
    return 0;
}
