#!/bin/bash

# Run mem.sh and load.sh in background
./scripts/monitor/mem.sh >> mem.log.dat &
PID_MEM=$!

./scripts/monitor/load.sh >> load.log.dat &
PID_LOAD=$!

./scripts/monitor/net.sh >> net.log.dat &
PID_NET=$!

# Trap SIGINT signal (Ctrl-C) and kill background processes
trap "kill $PID_MEM; kill $PID_LOAD; kill $PID_NET; exit" INT

# Wait for background processes to finish
wait
