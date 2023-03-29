#pragma once
#include <iostream>
#include <fstream>
#include <string>

#ifdef __FILENAME__
#define MYFILE __FILENAME__
#else
#define MYFILE __FILE__
#endif

namespace  WowLogger
{
  inline void LogBasic( std::string kind, const char* filename, int line, std::string str )
  {
    std::ofstream off("/tmp/logs.unreliable.txt", std::ios_base::app);
    off << std::string(kind) << " [" << std::string(filename) 
        << ":" << line << "] " << std::string(str) << std::endl; 
    std::cout << std::string(kind) << " [" << std::string(filename) 
        << ":" << line << "] " << std::string(str) << std::endl; 
    
  }

  constexpr const char* filename( const char* path )
  {
    const char* file = path;
    while ( *path ) {
      if ( *path == '/' ) {
        file = path + 1;
      }
      path++;
    }
    return file;
  }

  inline void Info(const char* filename, int line, std::string str)
  {
    LogBasic("INFO", filename, line, str);  
  }

  inline void Warn(const char* filename, int line, std::string str)
  {
    LogBasic("WARN", filename, line, str);
  }

  inline void Error(const char* filename, int line, std::string str)
  {
    LogBasic("ERROR", filename, line, str);
  }

}

#define LogInfo(x) WowLogger::Info(WowLogger::filename(__FILE__), __LINE__, x);
#define LogWarn(x) WowLogger::Warn(WowLogger::filename(__FILE__), __LINE__, x);
#define LogError(x) WowLogger::Error(WowLogger::filename(__FILE__), __LINE__, x);
