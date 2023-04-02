#!/bin/bash

replica="./build/ohmyserver/replica"

#./scripts/cleanup.sh
#rm /tmp/logs.unreliable.txt

tmux new-session -d -s my_session
tmux split-window -h

tmux send-keys -t 0 "${replica} --config ./config.csv --db_port 12345 --id 0" C-m

tmux split-window -v -t 0

#tmux send-keys -t 1 "./scripts/setupFS.sh" C-m
tmux send-keys -t 1 "${replica} --config ./config.csv --db_port 12345 --id 1" C-m

#sleep 3

#tmux send-keys -t 2 "cd /tmp/wowfs/durability" C-m
tmux send-keys -t 2 "${replica} --config ./config.csv --db_port 12345 --id 2" C-m

tmux select-pane -t 2

tmux attach -t my_session
