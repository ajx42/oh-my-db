#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <unistd.h>
#include <memory>
#include <argparse/argparse.hpp>
#include <chrono>

#include "OhMyConfig.H"
#include "DatabaseClient.H"
#include "DatabaseUtils.H"
#include "WowLogger.H"
#include "ReplicatedDB.H"
#include <random>

void writeTest(ohmydb::ReplicatedDB &repDB, size_t numPairs, size_t iter)
{
    auto start = std::chrono::high_resolution_clock::now();
    for(size_t i = 0; i < iter; i++)
    {
        repDB.put( std::make_pair( rand()%numPairs, rand()) );
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double seconds = duration / 1000.0;

    double avgLatency = duration / iter;

    std::cout << "Write Test Results:\n";
    std::cout << "Operations: " << iter << "\n";
    std::cout << "Elapsed Time: " << seconds << " s\n";
    std::cout << "Average Latency: " << avgLatency << " ms\n";

}

void readTest(ohmydb::ReplicatedDB &repDB, size_t numPairs, size_t iter)
{
    auto start = std::chrono::high_resolution_clock::now();
    for(size_t i = 0; i < iter; i++)
    {
        repDB.get(rand()%numPairs);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double seconds = duration / 1000.0;

    double avgLatency = duration / iter;

    std::cout << "Read Test Results:\n";
    std::cout << "Operations: " << iter << "\n";
    std::cout << "Elapsed Time: " << seconds << " s\n";
    std::cout << "Average Latency: " << avgLatency << " ms\n";

}

void readWriteTest(ohmydb::ReplicatedDB &repDB, size_t numPairs, size_t iter)
{
    auto start = std::chrono::high_resolution_clock::now();
    for(size_t i = 0; i < iter; i++)
    {
        if(rand()%2)
        {
            repDB.get(rand()%numPairs);
        }
        else
        {
            repDB.put( std::make_pair( rand()%numPairs, rand()) );
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    double seconds = duration / 1000.0;

    double avgLatency = duration / iter;

    // Print the time taken in seconds.milliseconds
    std::cout << "Read/Write Test Results:\n";
    std::cout << "Operations: " << iter << "\n";
    std::cout << "Elapsed Time: " << seconds << " s\n";
    std::cout << "Average Latency: " << avgLatency << " ms\n";

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

    program.add_argument("--numkeys")
        .default_value("10")
        .help("Number of possible keys for testing.");

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


    // parse arguments
    auto configPath = program.get<std::string>("--config");
    auto id = std::stoi(program.get<std::string>("--id"));
    auto iter = std::stoi(program.get<std::string>("--iter"));
    auto numPairs = std::stoi(program.get<std::string>("--numkeys"));

    auto servers = ParseConfig(configPath);
    auto serverAddr = servers[id].ip + ":" + std::to_string(servers[id].db_port);

    LogInfo("Client: Starting contact on " + serverAddr);

    auto repDB = ohmydb::ReplicatedDB(serverAddr);
    writeTest(repDB, numPairs, 1lu<<iter);
    readTest(repDB, numPairs, 1lu<<iter);
    readWriteTest(repDB, numPairs, 1lu<<iter);

    // for test only
    //auto printOpt = []( auto&& tag, auto&& opt ) {
    //  if ( opt.has_value() ) {
    //    std::cout << tag << " = " << opt.value() << std::endl;
    //  } else {
    //    std::cout << tag << " = NOT_FOUND" << std::endl;
    //  }
    //};
    //auto val = repDB.get( 789 );
    //printOpt( "first_attempt", val );
    //repDB.put( std::make_pair( 789, 10987654 ) );
    //auto valAgain = repDB.get( 789 );
    //printOpt( "second_attempt", valAgain );
    
    return 0;
}
