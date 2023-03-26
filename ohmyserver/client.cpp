#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include "client.h"

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
