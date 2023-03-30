#pragma once

#include <future>
#include <thread>
#include <list>
#include <mutex>

namespace raft {

template <class T>
class PromiseStore {
public:
  using store_t = std::list<std::promise<T>>;
  using handle_t = typename store_t::iterator;

  handle_t insert( std::promise<T>&& );
  std::promise<T> getAndRemove( handle_t );

  size_t size() { return store_.size(); }

  static PromiseStore& Instance() {
    static PromiseStore obj;
    return obj;
  }

private:
  PromiseStore() {}
  std::mutex storeMutex_;   
  store_t store_;
};

template <class T>
typename PromiseStore<T>::handle_t
PromiseStore<T>::insert( std::promise<T>&& inp )
{
  std::lock_guard<std::mutex> lock( storeMutex_ );
  store_.emplace_back( std::move( inp ) );
  auto it = store_.end();
  --it;
  return it;
}

template <class T>
std::promise<T> PromiseStore<T>::getAndRemove( handle_t it )
{
  std::lock_guard<std::mutex> lock( storeMutex_ );
  auto ret = std::move(*it);
  store_.erase( it );
  return ret;
}

} // namespace end
