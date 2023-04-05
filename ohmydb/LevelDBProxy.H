#pragma once

#include <map>
#include <optional>
#include <utility>
#include <leveldb/db.h>
#include <sstream>
#include "WowLogger.H"

namespace raft {

template <class KeyT, class ValT>
class LevelDBReal{
public:
  static_assert(std::is_integral<KeyT>::value);
  static_assert(std::is_integral<ValT>::value);

  static LevelDBReal& Instance() {
    static LevelDBReal obj;
    return obj;
  }
  
  std::optional<ValT> get( KeyT key ) {
    std::string valueStr;
    
    // Create a leveldb::Slice object for the key
    auto keyStr = std::to_string( key );
    leveldb::Slice keySlice( keyStr.c_str(), keyStr.size() );

    leveldb::Status status = db->Get(leveldb::ReadOptions(), keySlice, &valueStr);
    if ( status.ok() ) {
      LogInfo("Get successful.");
      if ( !valueStr.empty() ) {
        return std::stoi( valueStr );
      }
    }

    return {};
  }

  bool put( std::pair<KeyT, ValT> kvp ) {
    // Create a leveldb::Slice object for the key and value
    auto keyStr = std::to_string( kvp.first );
    auto valStr = std::to_string( kvp.second );
    leveldb::Slice key( keyStr.c_str(), keyStr.size() );
    leveldb::Slice val( valStr.c_str(), valStr.size() );

    //Put key/value pair.
    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, val);

    if (status.ok())
    {
      LogInfo("Put successful.");
      return true;
    }
    else
    {
      LogError("Put failed.");
      return false;
    }
  }

  void initialize(std::string db_path)
  {
    options.create_if_missing = true;

    //Will currently fail to open if multiple instances running on same node as
    //paths conflict.
    status = leveldb::DB::Open(options, db_path, &db);

    if (!status.ok())
    {
        LogError("Unable to open/create database.");
    }
    else
    {
      LogInfo("Started leveldb.");
    }

  }

private:
  LevelDBReal() {}
  leveldb::DB *db;
  leveldb::Options options;
  leveldb::Status status;
};

template <class KeyT, class ValT>
class LevelDBProxy {
public:
  static LevelDBProxy& Instance() {
    static LevelDBProxy obj;
    return obj;
  }
  
  std::optional<ValT> get( KeyT key ) {
    if ( mpp.find( key ) != mpp.end() ) {
      return { mpp[key] };
    } else {
      return {};
    }
  }

  bool put( std::pair<KeyT, ValT> kvp ) {
    mpp[kvp.first] = kvp.second;
    return true;
  }

  void initialize(std::string db_path)
  {
  }

private:
  LevelDBProxy() {}
  std::map<KeyT, ValT> mpp;
};

template <class KeyT, class ValT>
using LevelDB = LevelDBReal<KeyT, ValT>;

}
