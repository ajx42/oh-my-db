#!/bin/bash
./build/ohmyserver/replica --config ./config.csv --id 4 --addedNode --db_path /tmp/leveldb4 --ip 10.10.1.1  --raft_port 8084 --db_port 12349