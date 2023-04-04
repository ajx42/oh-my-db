#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <memory>
#include <argparse/argparse.hpp>

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

int main(int argc, char **argv)
{
    argparse::ArgumentParser program("client");
    program.add_argument("--server_address")
        .required()
        .help("DB server address");

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

    // OhMyDBClient client(server_addr);
    // create pointer to a OhMyDBClient object
    std::unique_ptr<OhMyDBClient> client = std::make_unique<OhMyDBClient>(
        grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials())
    );
    

    // /* for test only*/
    get(client, 1);
    put(client, 1, 2);
    get(client, 1);

    return 0;
}
