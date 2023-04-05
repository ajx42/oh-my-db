#pragma once
#include <string>

namespace ohmydb {

enum ErrorCode: int32_t {
  OK = 0,
  NOT_LEADER = 1,
  KEY_NOT_FOUND = 2
};

struct Ret {
  ErrorCode errorCode;
  std::string leaderAddr;
  int value;

  std::string str() const;
};

inline std::string Ret::str() const
{
  std::stringstream ss;
  ss  << "DBRet={"
      << "errorCode="   << errorCode    << " "
      << "leaderAddr="  << leaderAddr   << " "
      << "value="       << value        << "}";
  return ss.str();
}

} // namespace ohmydb