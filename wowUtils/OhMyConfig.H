#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unordered_map>

struct ServerInfo {
    std::string ip;
    int raft_port;
    int db_port;
    std::string name;
};

struct TokenizedOut {
  std::unordered_map<std::string, int32_t> header;
  std::vector<std::vector<std::string>> tokensByRow;
};

inline TokenizedOut TokenizeCSV( std::string filename )
{
  TokenizedOut ret;
  auto& header = ret.header;

  std::ifstream file(filename);
  std::string line;
  // parse header
  std::getline(file, line);
  std::stringstream ss(line);
  std::string token;

  while (std::getline(ss, token, ','))
  {
      header[token] = header.size();
  }

  // parse each node
  while (std::getline(file, line))
  {
      // check if line is empty
      if (line.empty()) break;
      std::stringstream ss(line);
      std::string token;
      std::vector<std::string> tokens;
      while (getline(ss, token, ','))
      {
          tokens.push_back(token);
      }
      ret.tokensByRow.push_back(tokens);
  }
  return ret;
}

inline std::vector<ServerInfo> ParseConfig(std::string filename)
{
    std::vector<ServerInfo> servers;
    std::ifstream file(filename);
    std::string line;
    // parse header
    std::getline(file, line);
    std::stringstream ss(line);
    std::string token;
    std::unordered_map<std::string, int> header;

    while (std::getline(ss, token, ','))
    {
        header[token] = header.size();
    }

    // parse each node
    while (std::getline(file, line))
    {
        // check if line is empty
        if (line.empty()) break;
        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (getline(ss, token, ','))
        {
            tokens.push_back(token);
        }
        ServerInfo server;
        server.name = tokens[header["name"]];
        server.ip = tokens[header["intf_ip"]];
        server.raft_port = stoi(tokens[header["raft_port"]]);
        server.db_port = stoi(tokens[header["db_port"]]);
        servers.push_back(server);
    }
    return servers;
}

