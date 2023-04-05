#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <memory>
#include <thread>
#include <argparse/argparse.hpp>

#include "ReplicatedDB.H"
#include "DatabaseClient.H"
#include "DatabaseUtils.H"
#include "WowLogger.H"

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
    auto serverAddr = program.get<std::string>("--server_address");

    auto repDB = ohmydb::ReplicatedDB( serverAddr );

    // for test only
    auto printOpt = []( auto&& tag, auto&& opt ) {
      if ( opt.has_value() ) {
        std::cout << tag << " = " << opt.value() << std::endl;
      } else {
        std::cout << tag << " = NOT_FOUND" << std::endl;
      }
    };
    auto val = repDB.get( 789 );
    printOpt( "first_attempt", val );
    repDB.put( std::make_pair( 789, 10987654 ) );
    auto valAgain = repDB.get( 789 );
    printOpt( "second_attempt", valAgain );
    
    return 0;
}
