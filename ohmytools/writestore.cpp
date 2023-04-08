#include <iostream>
#include <fstream>

#include <argparse/argparse.hpp>

#include "WowLogger.H"
#include "ConsensusUtils.H"
#include "OhMyConfig.H"
#include "PersistentStore.H"
#include "PersistentVector.H"

using namespace raft;

int main( int argc, char** argv ) {
  argparse::ArgumentParser program( "writestore" );
  
  program.add_argument( "--input" )
    .required()
    .help("input file defining the log structure to generate");

  program.add_argument( "--outputdir" )
    .required()
    .help("dir to dump all the output files in");

  program.add_argument( "--votedfor" )
    .default_value("-1")
    .help("VotedFor value to generate store for");

  program.add_argument( "--currentterm" )
    .default_value("0")
    .help("CurrentTerm value generate term for");

  program.add_argument( "--id" )
    .required()
    .help("ReplicaId for which the store is being generated");

  try {
      program.parse_args( argc, argv );
  }
  catch (const std::runtime_error& err) {
      std::cerr << err.what() << std::endl;
      std::cerr << program;
      std::exit(1);
  }

  auto getInt = [&]( auto&& key ) {
    return std::stoi( program.get<std::string>( key ) );
  };

  auto id = getInt( "--id" );
  auto votedFor = getInt( "--votedfor" );
  auto currentTerm = getInt( "--currentterm" );
  auto inputFile = program.get<std::string>( "--input" );
  auto outputDir = program.get<std::string>( "--outputdir" ) + '/';

  auto parsedInp = TokenizeCSV( inputFile );

  for ( auto row: parsedInp.tokensByRow ) {
    for ( auto& [k, v] : parsedInp.header ) {
      std::cout << k << "->" << row[v] << "|";
    }
    std::cout << std::endl;
  }

  auto storePrefix = outputDir + "raft." + std::to_string(id) + ".";
  auto logFilename = storePrefix + "log.persist";

  PersistentVector<LogEntry> pVec;
  pVec.setup( logFilename, false,
     []( auto&& e ) { e.op = e.op.withoutPromise(); return e; } );

  for ( auto row: parsedInp.tokensByRow ) {
    auto term = std::stoi(row[parsedInp.header["term"]]);
    auto count = std::stoi(row[parsedInp.header["count"]]); 
    while ( count-- ) {
      auto kind = rand() % 2 ? RaftOp::GET : RaftOp::PUT;
      pVec.push_back( LogEntry {
        .term = term,
        .op = RaftOp {
          .kind = kind,
          .args = kind == RaftOp::GET
                ? RaftOp::arg_t( rand()%100 )
                : RaftOp::arg_t( std::make_pair( rand()%100, rand()%100 ) ),
          .promiseHandle = {}
        }
      });
    }
  }
  pVec.persist();
  LogInfo("Wrote NumItems=" + std::to_string(pVec.size())
          + " Location=" + logFilename );

  PersistentStore pStore;
  pStore.setup( storePrefix );
  
  pStore.store( "VotedFor", votedFor );
  pStore.store( "CurrentTerm", currentTerm );

  return 0;
}
