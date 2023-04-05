#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <memory>
#include <argparse/argparse.hpp>

#include "OhMyConfig.H"
#include "DatabaseClient.H"
#include "DatabaseUtils.H"
#include "WowLogger.H"

void get(std::unique_ptr<OhMyDBClient>& client, int key) {
    std::optional<ohmydb::Ret> retOpt;
    ohmydb::Ret ret;

    do {
        retOpt = client->Get(key);

        if (!retOpt.has_value()) {
            LogInfo("Get " + std::to_string(key) + ": RPC No response");
            return;
        }

        ret = retOpt.value();

        if (ret.errorCode == ohmydb::ErrorCode::NOT_LEADER) {
            LogInfo("Get " + std::to_string(key) + ": Not leader, redirecting to " + ret.leaderAddr);
            client.reset(
                new OhMyDBClient(grpc::CreateChannel(ret.leaderAddr, grpc::InsecureChannelCredentials()))
            );
            sleep(1);
        } else {
            break;
        }
    } while ( 1 );

    if (ret.errorCode == ohmydb::ErrorCode::KEY_NOT_FOUND)
    {
        LogInfo( "Get " + std::to_string(key) + ": Key not found" )
    } else
    {
        LogInfo( "Get " + std::to_string(key) + ": Value" + std::to_string(ret.value) );
    }
}

void put(std::unique_ptr<OhMyDBClient>& client, int key, int value) {
    std::optional<ohmydb::Ret> retOpt;
    ohmydb::Ret ret;

    do {
        retOpt = client->Put(key, value);

        if (!retOpt.has_value()) {
            LogInfo("Put (" + std::to_string(key) + ", " + std::to_string(value) + 
                    "): RPC No response");
            return;
        }

        ret = retOpt.value();

        if (ret.errorCode == ohmydb::ErrorCode::NOT_LEADER) {
            LogInfo( "Put (" + std::to_string(key) + ", " + std::to_string(value) +
                 "): Not leader, redirecting to " + ret.leaderAddr );
            client.reset(
                new OhMyDBClient(grpc::CreateChannel(ret.leaderAddr, grpc::InsecureChannelCredentials()))
            );
            sleep(1);
        } else {
            break;
        }
    } while ( 1 );

    LogInfo( "Put (" + std::to_string(key) + ", " + std::to_string(value) +
             "): Success " );
}

void writeTest(std::string server_addr, size_t iter)
{
    // create pointer to a OhMyDBClient object
    std::unique_ptr<OhMyDBClient> client = std::make_unique<OhMyDBClient>(
        grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials())
    );

    for(size_t i = 0; i < iter; i++)
    {
        put(client, i, i);
    }

}

int main(int argc, char **argv)
{
    argparse::ArgumentParser program("client");
    program.add_argument("--config")
        .required()
        .help("Config file.");

    program.add_argument("--iter")
        .default_value("3")
        .help("Log2 of iterations to execute.");

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


    auto config_path = program.get<std::string>("--config");
    auto id = std::stoi(program.get<std::string>("--id"));
    auto iter = std::stoi(program.get<std::string>("--iter"));

    // parse arguments
    auto servers = ParseConfig(config_path);
    auto server_addr = servers[id].ip + ":" + std::to_string(servers[id].db_port);

    LogInfo("Client: Starting contact on " + server_addr);


    writeTest(server_addr, 1lu<<iter);
    // OhMyDBClient client(server_addr);
    

    // /* for test only*/
    //get(client, 1);
    //put(client, 1, 2);
    //get(client, 1);

    return 0;
}
