#!/bin/bash
./build/ohmyserver/replica --config ./config.csv --id 5 --addedNode --db_path /tmp/leveldb5 --ip 10.10.1.1  --raft_port 8085 --db_port 12345
# ./build/ohmyserver/replica --config ./config.csv --id 6 --addedNode --db_path /tmp/leveldb6 --ip 10.10.1.1  --raft_port 8086 --db_port 12346