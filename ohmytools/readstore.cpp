#pragma once

#include <string>

#include <argparse/argparse.hpp>

#include "WowLogger.H"
#include "ConsensusUtils.H"
#include "PersistentStore.H"
#include "PersistentVector.H"

using namespace raft;

int main( int argc, char** argv) {
  argparse::ArgumentParser program("readstore");
  program.add_argument("--file")
    .required()
    .help("the store file to decode");

  program.add_argument("--vec")
    .help("the provided file is a persistent vector store")
    .default_value( false )
    .implicit_value( true );


  try {
      program.parse_args( argc, argv );
  }
  catch (const std::runtime_error& err) {
      std::cerr << err.what() << std::endl;
      std::cerr << program;
      std::exit(1);
  }
  
  auto filename = program.get<std::string>( "--file" );
  auto isVec = program["--vec"] == true;

  LogInfo("Reading File=" + filename + " "
          + " IsPersistentVector=" + std::to_string(isVec) );

  if ( isVec ) {
    PersistentVector<LogEntry> pVec;
    pVec.setup( filename, true );
    auto cntr = 0;
    for ( const auto& entry: pVec ) {
      std::cout << "[" << cntr++ << "]\t" <<  
        entry.str() << std::endl;
    }
  } else {
    auto valOpt = PersistentStore::loadInt( filename );
    if ( ! valOpt.has_value() ) {
      LogError("Value could not be read. File corrupted?");
    } else {
      std::cout << "Value: " << valOpt.value() << std::endl;
    }
  }

}
