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

using namespace std;
class OhMyDBClient
{
public:
    OhMyDBClient(std::shared_ptr<grpc::Channel> channel)
        : stub_(ohmydb::OhMyDB::NewStub(channel)) {}
    int32_t Ping(int32_t cmd);

    int32_t Put(std::string key, std::string value);
    int32_t Get(std::string key, std::string &value);

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

int32_t OhMyDBClient::Put(std::string key, std::string value)
{
    ohmydb::PutRequest request;
    request.set_key(key);
    request.set_value(value);
    ohmydb::Ack reply;

    grpc::ClientContext context;

    auto status = stub_->Put(&context, request, &reply);
    if (status.ok())
    {
        return reply.ok();
    }
    else
    {
        std::cerr << "Put: RPC Failed" << std::endl;
        return -1;
    }
}

int32_t OhMyDBClient::Get(std::string key, std::string &value)
{
    ohmydb::GetRequest request;
    request.set_key(key);
    ohmydb::GetResponse reply;

    grpc::ClientContext context;

    auto status = stub_->Get(&context, request, &reply);
    if (status.ok())
    {
        value = reply.value();
        return reply.ok();
    }
    else
    {
        std::cerr << "Put: RPC Failed" << std::endl;
        return -1;
    }
}

int main()
{
    string server_addr("0.0.0.0:50051");
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
