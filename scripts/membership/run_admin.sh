#!/bin/bash

./build/ohmyserver/admin --id 2 --config ./config.csv --op rm
# Note: Node3 should be up and running before adding it to the cluster
# ./build/ohmyserver/admin --id 5 --config ./config.csv --op add --ip 10.10.1.1 --db_port 12345 --raft_port 8085 --name "Node5" 
# ./build/ohmyserver/admin --id 6 --config ./config.csv --op add --ip 10.10.1.1 --db_port 12346 --raft_port 8086 --name "Node6" 

