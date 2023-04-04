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
  static LevelDBReal& Instance() {
    static LevelDBReal obj;
    return obj;
  }
  
  std::optional<ValT> get( KeyT key ) {

    static_assert(std::is_integral<KeyT>::value);

    std::string value_str;
    ValT value;
    
    // Create a leveldb::Slice object for the key
    leveldb::Slice key_slice(reinterpret_cast<const char*>(&key), sizeof(key));

    leveldb::Status status = db->Get(leveldb::ReadOptions(), key_slice, &value_str);
    if (status.ok())
    {
      LogInfo("Get successful.");
      if(!value_str.empty())
      {
        std::istringstream iss(value_str);
        iss >> value;
        return {value};
      }
    }

    return {};
  }

  bool put( std::pair<KeyT, ValT> kvp ) {
    static_assert(std::is_integral<KeyT>::value);
    static_assert(std::is_integral<ValT>::value);

    // Create a leveldb::Slice object for the key and value
    leveldb::Slice key(reinterpret_cast<const char*>(&kvp.first), sizeof(kvp.first));
    leveldb::Slice value(reinterpret_cast<const char*>(&kvp.second), sizeof(kvp.second));

    //Put key/value pair.
    leveldb::Status status = db->Put(leveldb::WriteOptions(), key, value);

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
