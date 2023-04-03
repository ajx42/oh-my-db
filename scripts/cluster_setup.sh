#!/bin/bash
BRANCH="tc_cloudlab_setup"

setup () 
{
    SSH_HOST=$1
    scp oh-my-db.tar.gz ${SSH_HOST}:~/
    scp config.csv ${SSH_HOST}:~/
    ssh -q ${SSH_HOST} << EOF
        tar -xzf oh-my-db.tar.gz
        cd oh-my-db
        ./scripts/node_setup.sh
        mkdir build
        cd build
        cmake ../ -DCMAKE_INSTALL_PREFIX=.
        cmake --build . --parallel 16
        exit
EOF
}

parse_ssh_hosts()
{
    local filename=$1
    local i=0
    local arr=()
    while read -r line; do
      if [[ $i -eq 0 ]]; then
        ((i=i+1))
        continue
      fi
      arr[i-1]=$(echo $line | awk -F',' '{print $6"@"$4}')
      ((i=i+1))
    done < config.csv
    echo ${arr[@]}
}

git clone git@github.com:ajain365/oh-my-db.git
cd oh-my-db
git checkout ${BRANCH}
git submodule update --init --recursive
cd ..
tar -czf oh-my-db.tar.gz oh-my-db
python3 oh-my-db/scripts/cloudlab_setup/create_config.py \
    --manifest manifest.xml \
    --output config.csv

SSH_HOSTS=($(parse_ssh_hosts config.csv))
echo ${SSH_HOSTS[@]}

for SSH_HOST in "${SSH_HOSTS[@]}"
{
    echo "Setting up ${SSH_HOST}"
    setup ${SSH_HOST} &
}

echo "Waiting for setup to complete... Go grab a coffee!"
wait

python3 oh-my-db/scripts/cloudlab_setup/create_tmux_script.py \
    --manifest manifest.xml \
    --session ohmydb \
    --output tmux_script.sh

chmod +x tmux_script.sh
./tmux_script.sh
