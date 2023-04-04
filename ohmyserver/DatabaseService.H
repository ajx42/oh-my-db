#pragma once

#include "OhMyReplica.H"
#include "WowLogger.H"

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>

#include "db.grpc.pb.h"

class OhMyDBService final : public ohmydb::OhMyDB::Service
{
public:
    explicit OhMyDBService() {}

    grpc::Status TestCall(grpc::ServerContext *, const ohmydb::Cmd *, ohmydb::Ack *);
    grpc::Status Put(grpc::ServerContext *, const ohmydb::PutRequest *, ohmydb::PutResponse *);
    grpc::Status Get(grpc::ServerContext *, const ohmydb::GetRequest *, ohmydb::GetResponse *);
};

inline grpc::Status OhMyDBService::TestCall(
    grpc::ServerContext *, const ohmydb::Cmd *cmd, ohmydb::Ack *ack)
{
    std::cout << "Client has made contact... " << std::endl;
    ack->set_ok(cmd->sup());
    return grpc::Status::OK;
}

inline grpc::Status OhMyDBService::Put(
    grpc::ServerContext *, const ohmydb::PutRequest *request, ohmydb::PutResponse *response)
{
    int key = request->key();
    int val = request->value();
    auto ret = ReplicaManager::Instance().put( {key, val} );

    response->set_error_code(ret.errorCode);
    response->set_leader_addr(ret.leaderAddr);
    return grpc::Status::OK;
}

inline grpc::Status OhMyDBService::Get(
    grpc::ServerContext *, const ohmydb::GetRequest *request, ohmydb::GetResponse *response)
{
    int key = request->key();
    auto ret = ReplicaManager::Instance().get( key );

    response->set_error_code(ret.errorCode);
    response->set_leader_addr(ret.leaderAddr);
    response->set_value(ret.value.value_or(-1));

    return grpc::Status::OK;
}