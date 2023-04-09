#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <argparse/argparse.hpp>
#include <chrono>
#include <cstring>
#include <map>

#include "OhMyConfig.H"
#include "WowLogger.H"
#include "RaftService.H"
#include "ConsensusUtils.H"

class Admin {
public:
  Admin( std::map<int32_t, ServerInfo> servers ): servers_(servers),
        client_( grpc::CreateChannel( "10.10.1.1:1234", grpc::InsecureChannelCredentials() ) ) {}

  bool AddServer( int id, std::string ip, int db_port, int raft_port, std::string name );
  bool RemoveServer( int id );
  bool WriteConfig( std::string filename, std::map<int32_t, ServerInfo> servers );

private:
  static constexpr const int32_t MAX_TRIES = 1000;
  RaftClient client_;
  std::map<int32_t, ServerInfo> servers_; // map of server id to server info

  void SwitchClient ( int server_id );
  void SwitchClient ( std::string addr );
};

void Admin::SwitchClient ( int server_id )
{
    ServerInfo info = servers_[server_id];
    std::string addr = std::string(info.ip) + ":" + std::to_string(info.raft_port);
    LogInfo ( "Switching to " + addr );
    client_ = RaftClient ( grpc::CreateChannel( addr, grpc::InsecureChannelCredentials() ) );
}

void Admin::SwitchClient ( std::string addr )
{
    LogInfo ( "Switching to " + addr );
    client_ = RaftClient ( grpc::CreateChannel( addr, grpc::InsecureChannelCredentials() ) );
}

bool Admin::AddServer( int id, std::string ip, int db_port, int raft_port, std::string name ) 
{
    // make sure the new server is already present
    client_ = RaftClient (
        grpc::CreateChannel( ip + ":" + std::to_string(raft_port), grpc::InsecureChannelCredentials() ));
    if ( client_.Ping(1) < 0 )
    {
        LogError( "Failed to connect to new server. Aborting." );
        return false;
    }

    raft::AddServerParams param = {
        .serverId = id,
        .raftPort = raft_port,
        .dbPort = db_port
    };
    strcpy( param.ip, ip.c_str() );
    strcpy( param.name, name.c_str() );

    auto iters = MAX_TRIES;
    int server_id = 0;
    SwitchClient( server_id );

    while ( iters-- ) {
        auto ret = client_.AddServer( param );

        if ( !ret.has_value() ) {
            LogError( "Failed to connect to server. It may be dead. Retrying with others." );
            server_id++;
            if ( server_id >= servers_.size() ) {
                LogError( "All servers are dead. Aborting." );
                return false;
            }
            SwitchClient( server_id );
            continue;
        }
        
        switch ( ret.value().errorCode ) {
            case raft::ErrorCode::OK: {
                LogInfo( "Successfully added server " );
                return true;
            }
            case raft::ErrorCode::NOT_LEADER: {
                LogWarn( "This server is not the leader. ");
                SwitchClient( ret.value().leaderAddr );
                break;
            }
            case raft::ErrorCode::PREV_NOT_COMMITTED_TIMEOUT: {
                LogWarn( "Previous operation not committed. Retrying." );
                break;
            }
            case raft::ErrorCode::CUR_NOT_COMMITTED_TIMEOUT: {
                LogWarn( "Current operation not committed. Retrying." );
                break;
            }
            case raft::ErrorCode::SERVER_EXISTS: {
                LogWarn( "Server already exists in the cluster. Success." );
                return true;
            }
            case raft::ErrorCode::OTHER: {
                LogWarn( "Unknown error. Retrying." );
                break;
            }
        }

        std::this_thread::sleep_for( std::chrono::milliseconds(500) );
    }

    LogWarn( "Failed to add server after " + std::to_string(MAX_TRIES) + " tries." );
    return false;
}

bool Admin::RemoveServer( int id ) 
{
    raft::RemoveServerParams param = {
        .serverId = id
    };

    auto iters = MAX_TRIES;
    int server_id = 0;
    SwitchClient( server_id );

    while ( iters-- ) {
        auto ret = client_.RemoveServer( param );

        if ( !ret.has_value() ) {
            LogError( "Failed to connect to server. It may be dead. Retrying with others." );
            server_id++;
            if ( server_id >= servers_.size() ) {
                LogError( "All servers are dead. Aborting." );
                return false;
            }
            SwitchClient( server_id );
            continue;
        }
        
        switch ( ret.value().errorCode ) {
            case raft::ErrorCode::OK: {
                LogInfo( "Successfully removed server " + std::to_string(id) );
                return true;
            }
            case raft::ErrorCode::NOT_LEADER: {
                LogWarn( "This server is not the leader. ");
                SwitchClient( ret.value().leaderAddr );
                break;
            }
            case raft::ErrorCode::PREV_NOT_COMMITTED_TIMEOUT: {
                LogWarn( "Previous operation not committed. Retrying." );
                break;
            }
            case raft::ErrorCode::CUR_NOT_COMMITTED_TIMEOUT: {
                LogWarn( "Current operation not committed. Retrying." );
                break;
            }
            case raft::ErrorCode::SERVER_NOT_FOUND: {
                LogWarn( "Server not found in the cluster. Aborting." );
                return false;
            }
            case raft::ErrorCode::OTHER: {
                LogWarn( "Unknown error. Retrying." );
                break;
            }
        }
    }

    LogWarn( "Failed to remove server after " + std::to_string(MAX_TRIES) + " tries." );
    return false;
}

bool Admin::WriteConfig( std::string filename, std::map<int32_t, ServerInfo> servers )
{
    // file is a csv, with header 
    // name,intf_ip,raft_port,db_port
    std::ofstream file( filename );
    if ( !file.is_open() ) {
        LogError( "Failed to open file " + filename );
        return false;
    }

    file << "id,name,intf_ip,raft_port,db_port" << std::endl;
    for ( const auto& [id, info] : servers ) {
        file << info.id << "," << info.name << "," << info.ip << "," << info.raft_port << "," << info.db_port << std::endl;
    }

    file.close();
    LogInfo( "Successfully wrote config to " + filename );
    return true;
}

int main(int argc, char **argv)
{
    argparse::ArgumentParser program("client");
    program.add_argument("--config")
        .required()
        .help("Config file.");

    program.add_argument("--op")
        .required()
        .help("Operation to perform. Either add or rm.");
    
    program.add_argument("--id")
        .help("The node ID to add or to remove.")
        .default_value("-1");

    program.add_argument("--ip")
        .help("IP address of the node. Only needed when addedNode is true.")
        .default_value("");
    
    program.add_argument("--raft_port")
        .help("Raft port of the node. Only needed when addedNode is true.")
        .default_value("-1");

    program.add_argument("--db_port")
        .help("DB port of the node. Only needed when addedNode is true.")
        .default_value("-1");
    
    program.add_argument("--name")
        .help("Name of the node. Only needed when addedNode is true.")
        .default_value("");


    try {
        program.parse_args( argc, argv );
    }
    catch (const std::runtime_error& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        std::exit(1);
    }

    std::string op = program.get<std::string>("--op");
    if ( op != "add" && op != "rm" ) {
        std::cerr << "Invalid operation. Must be either add or rm." << std::endl;
        std::exit(1);
    }

    auto configPath = program.get<std::string>("--config");
    auto id = std::stoi(program.get<std::string>("--id"));
    auto ip = program.get<std::string>("--ip");
    auto raft_port = std::stoi(program.get<std::string>("--raft_port"));
    auto db_port = std::stoi(program.get<std::string>("--db_port"));
    auto name = program.get<std::string>("--name");

    auto servers = ParseConfig(configPath);

    auto admin = Admin( servers );

    if ( op == "add" ) {
        auto ret = admin.AddServer( id, ip, db_port, raft_port, name );
        if ( ret )
        {
            ServerInfo info = {
                .id = id,
                .raft_port = raft_port,
                .db_port = db_port,
            };
            strcpy( info.ip, ip.c_str() );
            strcpy( info.name, name.c_str() );
            servers[id] = info;
            admin.WriteConfig( configPath, servers );
        }
    } else if ( op == "rm" ) {
        auto ret = admin.RemoveServer( id );
        if ( ret )
        {
            servers.erase( id );
            admin.WriteConfig( configPath, servers );
        }
    }
    
    return 0;
}
