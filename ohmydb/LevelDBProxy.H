#pragma once

#include <map>
#include <optional>
#include <utility>

namespace raft {

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

private:
  LevelDBProxy() {}
  std::map<KeyT, ValT> mpp;
};

template <class KeyT, class ValT>
using LevelDB = LevelDBProxy<KeyT, ValT>;

}
