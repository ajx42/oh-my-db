#pragma once
#include <iostream>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <cstring>

struct ServerInfo {
    int id;
    char ip[20];
    int raft_port;
    int db_port;
    char name[25]; // truncating name to 20 chars, see below

    std::string str() const;
} __attribute__((__packed__));

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

inline std::map<int32_t, ServerInfo> ParseConfig(std::string filename)
{
    std::map<int32_t, ServerInfo> servers;
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
    size_t cntr = 0;

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
        
        if ( header.find("id") == header.end() ) {
          server.id = cntr++;
        } else {    
          server.id = stoi(tokens[header["id"]]);
        }

        strcpy(server.ip, tokens[header["intf_ip"]].c_str());
        strcpy(server.name, tokens[header["name"]].substr(0, 20).c_str());
        server.raft_port = stoi(tokens[header["raft_port"]]);
        server.db_port = stoi(tokens[header["db_port"]]);
        servers[server.id] = server;
    }
    return servers;
}

