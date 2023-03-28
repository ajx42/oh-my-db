# oh-my-db
Oh My Database - for CS739 P2

## Build Instructions
```shell
mkdir build
cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=.
cmake --build . --parallel 16
```

## To Run Server
```shell
cd build/ohmyserver
./server --config ~/config.csv --db_port 12345 --raft_port 8080
```
To generate and distribute config to cloudlab nodes, look at `scripts/cloudlab_setup`.