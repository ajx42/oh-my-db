#!/bin/bash

# ./build/ohmyserver/admin --id 2 --config ./config.csv --op rm
./build/ohmyserver/admin --id 4 --config ./config.csv --op add --ip 10.10.1.1 --db_port 12349 --raft_port 8084 
