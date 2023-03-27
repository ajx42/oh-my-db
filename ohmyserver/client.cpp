#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <argparse/argparse.hpp>
#include "client.h"

int main(int argc, char **argv)
{
    argparse::ArgumentParser program("client");
    program.add_argument("--server_address")
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
    auto server_addr = program.get<std::string>("--server_address");

    OhMyDBClient client(grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()));

    client.Ping(1);

    std::string value;
    std::cout << "Get : Hello...\n";
    if (client.Get("Hello", value) < 0)
    {
        std::cout << "Get failed! Key maybe not present.\n";
    }
    else
    {
        std::cout << "Returned : " << value << "\n";
    }

    std::cout << "Put : Hello...\n";
    if (client.Put("Hello", "World") < 0)
    {
        std::cout << "Put failed!\n";
    }

    std::cout << "Get : Hello...\n";
    if (client.Get("Hello", value) < 0)
    {
        std::cout << "Get failed! Key maybe not present.\n";
    }
    else
    {
        std::cout << "Returned : " << value << "\n";
    }

    return 0;
}
