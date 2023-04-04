#pragma once

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