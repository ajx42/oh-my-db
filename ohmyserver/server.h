#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "db.grpc.pb.h"

class OhMyDBService final : public ohmydb::OhMyDB::Service
{
private:
    leveldb::DB *db;
    leveldb::Options options;

public:
    explicit OhMyDBService()
    {
        // leveldb setup
        options.create_if_missing = true;
        leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
        if (!status.ok())
        {
            std::cerr << "Unable to open/create database" << std::endl;
        }
    }

    grpc::Status TestCall(grpc::ServerContext *, const ohmydb::Cmd *, ohmydb::Ack *);
    grpc::Status Put(grpc::ServerContext *, const ohmydb::PutRequest *, ohmydb::Ack *);
    grpc::Status Get(grpc::ServerContext *, const ohmydb::GetRequest *, ohmydb::GetResponse *);
};

grpc::Status OhMyDBService::TestCall(
    grpc::ServerContext *, const ohmydb::Cmd *cmd, ohmydb::Ack *ack)
{
    std::cout << "Client has made contact... " << std::endl;
    ack->set_ok(cmd->sup());
    return grpc::Status::OK;
}

grpc::Status OhMyDBService::Put(
    grpc::ServerContext *, const ohmydb::PutRequest *request, ohmydb::Ack *ack)
{
    leveldb::Status status = db->Put(leveldb::WriteOptions(), request->key(), request->value());
    if (status.ok())
    {
        ack->set_ok(0);
    }
    else
    {
        ack->set_ok(-1);
    }
    return grpc::Status::OK;
}

grpc::Status OhMyDBService::Get(
    grpc::ServerContext *, const ohmydb::GetRequest *request, ohmydb::GetResponse *response)
{
    std::string value;
    leveldb::Status status = db->Get(leveldb::ReadOptions(), request->key(), &value);
    if (status.ok())
    {
        response->set_ok(0);
        response->set_value(value);
    }
    else
    {
        response->set_ok(-1);
    }

    return grpc::Status::OK;
}