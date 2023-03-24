#include <iostream>
#include <leveldb/db.h>
#include <string>
#include <sstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include "db.grpc.pb.h"

#define PORT 8080

class OhMyDBService final : public ohmydb::OhMyDB::Service
{
    private:

    public:

};

int main()
{
    leveldb::DB *db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "/tmp/testdb", &db);
    if (!status.ok())
    {
        std::cerr << "Unable to open/create database" << std::endl;
        return 1;
    }

    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    char buffer[1024] = {0};
    std::string response;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        return 1;
    }

    // Attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        return 1;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        return 1;
    }
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        return 1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;

    while (true)
    {
        // Accept a new client connection.
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            return 1;
        }

        std::cout << "New client connected: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;

        // Process the client requests.
        while (true)
        {
            std::memset(buffer, 0, sizeof(buffer));

            // Receive the client's request.
            ssize_t bytes_received = recv(new_socket, buffer, sizeof(buffer), 0);
            if (bytes_received < 0)
            {
                perror("recv");
                return 1;
            }
            else if (bytes_received == 0)
            {
                // The client has closed the connection.
                std::cout << "Client disconnected: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;
                break;
            }

            std::string request(buffer, bytes_received);
            std::istringstream iss(request);
            std::string command, key, value;
            iss >> command >> key;
            if (command == "put")
            {
                iss >> value;
                leveldb::Status status = db->Put(leveldb::WriteOptions(), key, value);
                if (status.ok())
                {
                    response = "OK";
                }
                else
                {
                    response = "ERROR";
                }
            }
            else if (command == "get")
            {
                std::string value;
                leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &value);
                if (status.ok())
                {
                    response = "OK " + value;
                }
                else
                {
                    response = "ERROR";
                }
            }
            else if (command == "quit")
            {
                response = "GOODBYE";
                break;
            }
            else
            {
                response = "ERROR";
            }

            response += "\n";
            // Send the response back to the client. end with newline
            send(new_socket, response.c_str(), response.length(), 0);
        }

        close(new_socket);
    }

    return 0;
}
