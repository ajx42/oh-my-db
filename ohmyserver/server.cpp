#include <iostream>
#include <leveldb/db.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <argparse/argparse.hpp>
#include "server.h"
#include "raft.h"

int main(int argc, char **argv)
{
    argparse::ArgumentParser program("server");
    program.add_argument("--config")
        .required()
        .help("Config file path");

    program.add_argument("--db_port")
        .required()
        .help("DB service port");

    program.add_argument("--raft_port")
        .required()
        .help("Raft service port");

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
    auto raft_port = program.get<std::string>("--raft_port");
    auto config_path = program.get<std::string>("--config");

    auto servers = ParseConfig(config_path);
    
    
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    grpc::ServerBuilder builder;

    // start raft server
    RaftService raft_service;
    std::string raft_server_addr("0.0.0.0:" + raft_port);
    builder.AddListeningPort(raft_server_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&raft_service);

    std::unique_ptr<grpc::Server> raft_server(builder.BuildAndStart());
    std::cout << "Raft server listening on " << raft_server_addr << std::endl;

    // test that we can connect to all servers with grpc
    for (auto server : servers)
    {
        std::cout << "Testing connection to " << server.name << " at " << server.ip << ":" << server.port << std::endl;
        std::string address(server.ip + ":" + std::to_string(server.port));
        RaftClient client(grpc::CreateChannel(address, grpc::InsecureChannelCredentials()));

        while (client.Ping(1) < 0)
        {
            std::cout << "Ping failed, retrying in 1 second" << std::endl;
            sleep(1);
        }
    }

    // start db server
    OhMyDBService db_service;
    std::string db_server_addr("0.0.0.0:" + db_port);
    builder.AddListeningPort(db_server_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&db_service);

    std::unique_ptr<grpc::Server> db_server(builder.BuildAndStart());
    std::cout << "DB Server listening on " << db_server_addr << std::endl;

    db_server->Wait();

    return 0;
}
