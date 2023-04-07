#pragma once

#include <cstdio>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "WowLogger.H"

namespace raft {

// This impl is barely complete -- massively incomplete actually.
// Note that this will only work if the only ops being done on the
// vector are "resize" and "push_back". Updating any existing item
// in the vector is not supported.
template <class T>
class PersistentVector : public std::vector<T>
{
public:
  using Base_t = std::vector<T>;
  
  template <class ...Args>
  PersistentVector( Args... args );
  ~PersistentVector();

  // When we resize down, we will truncate our persistent store file
  void resize( size_t newSize );

  // When persist is called, the items in the vector that we haven't
  // persisted get stored.
  void persist();

  bool bootstrap( std::string filename );
  void setup( std::string filename, bool withBootstrap,
              std::function<T(T)> preproc = [](T val) { return val; } );

private:
  int fd;
  size_t persistedItems_ = 0;
  std::string filename_;
  bool initialised_ = false;
  std::function<T(T)> preproc_ = [](T val) { return val; };

};

template <class T>
template <class ...Args>
PersistentVector<T>::PersistentVector( Args... args )
  : Base_t( args... )
{}

template <class T>
void PersistentVector<T>::setup( std::string filename, bool withBootstrap, std::function<T(T)> preproc )
{
  if ( initialised_ ) {
    return;
  }

  preproc_ = preproc;
  filename_ = filename;
  if ( withBootstrap ) {
    persistedItems_ = bootstrap( filename ) ? Base_t::size() : 0;
    fd = open( filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0777 );
  } else {
    fd = open( filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0777 );
  }
  
  initialised_ = true;
}

template <class T>
PersistentVector<T>::~PersistentVector()
{
  close( fd );
}

template <class T>
void PersistentVector<T>::resize( size_t newSize )
{
  if ( newSize <= persistedItems_ ) {
    ftruncate( fd, newSize * sizeof(T) );  
    close( fd );
    persistedItems_ = newSize; 
    fd = open( filename_.c_str(), O_WRONLY | O_CREAT | O_APPEND );
  }

  Base_t::resize( newSize );
}

template <class T>
void PersistentVector<T>::persist()
{
  auto curSize = Base_t::size();
  if ( curSize == persistedItems_ ) {
    return;
  }

  resize( curSize );
  
  for ( size_t i = persistedItems_ ; i < curSize ; ++i ) {
    auto copy = preproc_( *( Base_t::data() + i ) );
    write( fd, &copy, sizeof(T) );
  }

  fsync( fd );
  persistedItems_ = curSize;
}

template <class T>
bool PersistentVector<T>::bootstrap( std::string filename )
{
  auto readFd = open( filename.c_str(), O_RDONLY );
  if ( readFd <= 0 ) {
    LogWarn( "Could not open file to bootstrap!" );
    return false;
  }

  lseek( readFd, 0, SEEK_SET );
  auto fileSize = lseek( readFd, 0, SEEK_END );
  
  if ( fileSize == -1 ) {
    return false;
  }

  std::vector<uint8_t> buf;
  buf.resize( fileSize + 50 );

  lseek( readFd, 0, SEEK_SET );
  pread( readFd, buf.data(), fileSize, 0 );

  close( readFd );

  // IMP: Hopefully the below approach works and we won't need atomic
  // temp file write and swap. Why avoid? We'd have to either maintain
  // two copies of the file, or write the entire log to the temp file
  // before making an incremental change.
  // Assuming that the file may have some incomplete writes at the end
  // because maybe we crashed while writing.
  // This is why we have an extra check that the amount of data we need
  // must be present before casting.
  for ( auto i = 0; i + sizeof(T) <= fileSize; i += sizeof(T) ) {
    Base_t::push_back( *reinterpret_cast<const T*>( &buf[i] ) );
  }

  // Doing a resize to the same size here. This will truncate the
  // persistent storage to appropriate length.
  resize( Base_t::size() );
  return true;
}

} // end namespace raft
