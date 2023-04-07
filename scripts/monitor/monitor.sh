#!/bin/bash

# Run mem.sh and load.sh in background
./scripts/monitor/mem.sh >> mem.log &
PID_MEM=$!

./scripts/monitor/load.sh >> load.log &
PID_LOAD=$!

./scripts/monitor/net.sh >> net.log &
PID_NET=$!

# Trap SIGINT signal (Ctrl-C) and kill background processes
trap "kill $PID_MEM; kill $PID_LOAD; kill $PID_NET; exit" INT

# Wait for background processes to finish
wait
