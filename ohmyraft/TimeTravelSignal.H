// TimeTravelSignal
// made up stuff, this might be buggy
// "notify a thread that is not waiting for you, yet"

#pragma once

#include "WowLogger.H"

#include <condition_variable>
#include <mutex>

namespace raft {

// This is signaling channel. The notifying thread is never blocked.
// Even if there is no thread waiting for signal, the notifying thread
// can just register the signal (for the future - and so time travel) 
// and keep going. In case a thread is waiting, it is notified and unblocked.
// When the receiving thread waits, it checks if a notification already
// exists in which case it doesn't need to wait. Otherwise it blocks using
// a condition variable.

// In RAFT, such a signal can be used to notify the executer when new entries
// are committed.
// This should reduce a lot of busy waiting while the executer waits for stuff
// to consume or RAFT waits for input to be fed.

class TimeTravelSignal {
public:
  void reset();
  void signal();
  void wait();
private:
  std::condition_variable cvar;
  std::mutex miniLock;
  bool noBlock = false; // sender is fast
  bool waiting = false; // receiver is fast
};

inline void TimeTravelSignal::reset()
{
  cvar.notify_all();
  noBlock = false;
  waiting = false;
}

// the sending thread never blocks
inline void TimeTravelSignal::signal()
{
  std::lock_guard<std::mutex> lock( miniLock );
  if ( noBlock ) {
    return; // we already have a signal waiting to be consumed
  }
  if ( waiting ) {
    waiting = false;
    cvar.notify_all();
  } else {
    noBlock = true;
  }
}

// this is a potentially blocking call used by the receiver
inline void TimeTravelSignal::wait()
{
  std::unique_lock lock { miniLock };
  if ( waiting ) {
    // this shouldn't happen
    LogError("Attempt to read TimeTravelSignal while already waiting");
    return;
  }
  if ( noBlock ) {
    noBlock = false; // we already have signal
  } else {
    // we will block now
    waiting = true;
    // This atomically releases lock, adds thread to listeners
    // of cvar and blocks.
    cvar.wait( lock );
  }
}

}
