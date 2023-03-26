#include <iostream>
#include <leveldb/db.h>
#include <string>
#include <sstream>
#include <unistd.h>
#include <argparse/argparse.hpp>
#include "server.h"


int main(int argc, char **argv)
{
    argparse::ArgumentParser program("server");
    program.add_argument("--config")
        .required()
        .help("Config file path");

    program.add_argument("--port")
        .required()
        .help("Config file path");

    try {
        program.parse_args( argc, argv );
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    // parse arguments
    auto port = program.get<std::string>("--port");
    auto config_path = program.get<std::string>("--config");

    auto servers = ParseConfig(config_path);

    std::string server_addr("0.0.0.0:" + port);
    OhMyDBService service;

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    grpc::ServerBuilder builder;

    builder.AddListeningPort(server_addr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_addr << std::endl;

    server->Wait();

    return 0;
}
