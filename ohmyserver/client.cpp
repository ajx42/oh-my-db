#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include "client.h"
#include <argparse/argparse.hpp>

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

    string value;
    cout << "Get : Hello...\n";
    if (client.Get("Hello", value) < 0)
    {
        cout << "Get failed! Key maybe not present.\n";
    }
    else
    {
        cout << "Returned : " << value << "\n";
    }

    cout << "Put : Hello...\n";
    if (client.Put("Hello", "World") < 0)
    {
        cout << "Put failed!\n";
    }

    cout << "Get : Hello...\n";
    if (client.Get("Hello", value) < 0)
    {
        cout << "Get failed! Key maybe not present.\n";
    }
    else
    {
        cout << "Returned : " << value << "\n";
    }

    return 0;
}
