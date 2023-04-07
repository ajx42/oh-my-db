#pragma once

<<<<<<< HEAD
<<<<<<< HEAD
=======
>>>>>>> 11d0b56 (atomic file writes and configurable store)
#include <string>
#include <functional>
#include <vector>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace raft {

// Persistent Key-Val Store to store RAFT persistent state.
// We use one file per key to avoid reading data we don't need to
// when loading a key. It also simplifies writes.
// The files are named as:
//        <fileBaseName><key>.persist 
// OhMyRaft uses the following format:
//        <path/to/storeDir>/raft.<replica_id>.<key>.persist
class PersistentStore {
public:
  PersistentStore() {};
  
  void setup( std::string fileBaseName );
  void store( std::string key, int32_t val );
  int32_t load( std::string key, int32_t defaultVal );
  std::string getFilename( std::string key );
  
  static std::optional<int32_t> loadInt( std::string filename );

private:
  std::string fileBaseName_; 
  bool initialised_ = false;
};

inline void PersistentStore::setup( std::string fileBaseName )
{
  if ( initialised_ ) {
    return;
  }

  initialised_ = true;
  fileBaseName_ = fileBaseName;
}

inline std::string PersistentStore::getFilename( std::string key )
{
  return fileBaseName_ + key + ".persist";
}

inline void PersistentStore::store( std::string key, int32_t val )
{
  // write to a tmp file and then rename to achieve atomic write
  auto tmpFile = getFilename( key ) + ".tmp";
  auto fd = open( tmpFile.c_str(), O_WRONLY | O_CREAT, 0777 );
  write( fd, (const char*) &val, sizeof(val) );
  close( fd );

  rename( tmpFile.c_str(), getFilename( key ).c_str() );
}

inline std::optional<int32_t>
PersistentStore::loadInt( std::string filename )
{
  auto fd = open( filename.c_str(), O_RDONLY );
  if ( fd <= 0 ) {
    return {};
  }
  uint8_t buf[4] = {0};
  auto numBytes = pread( fd, buf, 4, 0 );
  close( fd );
  if ( numBytes != sizeof(int32_t) ) {
    return {};
  }
  return *reinterpret_cast<int32_t*>( buf );
}


inline int32_t PersistentStore::load( std::string key, int32_t defaultVal )
{
  auto filename = getFilename( key );
  return loadInt( filename ).value_or( defaultVal );
}
}
