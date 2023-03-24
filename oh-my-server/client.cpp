#include <iostream>
#include <string>
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <grpcpp/grpcpp.h>
#include <grpcpp/channel.h>
#include "db.grpc.pb.h"

#define PORT 8080

class OhMyDBClient
{
public:
    OhMyDBClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(ohmydb::OhMyDB::NewStub(channel)) {}
    int32_t Ping(int32_t cmd);

private:
    std::unique_ptr<ohmydb::OhMyDB::Stub> stub_;
};

// Note this method is only used for Testing
int32_t OhMyDBClient::Ping(int32_t cmd)
{
    ohmydb::Cmd msg;
    msg.set_sup(cmd);
    ohmydb::Ack reply;

    grpc::ClientContext context;

    auto status = stub_->TestCall(&context, msg, &reply);
    if (status.ok())
    {
        std::cerr << "Received OK" << std::endl;
        return reply.ok();
    }
    else
    {
        std::cerr << "RPC Failed" << std::endl;
        return -1;
    }
}

int main()
{
    std::string server_addr("0.0.0.0:50051");
    OhMyDBClient client(grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()));

    client.Ping(1);

    return 0;
}
