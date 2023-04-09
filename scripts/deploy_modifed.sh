#!/bin/bash

deploy_to_replica()
{
    SSH_HOST=$1
    file=$2

    scp $file ${SSH_HOST}:~/oh-my-db/$file
    ssh ${SSH_HOST} << EOF
    cd oh-my-db/build
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
        arr[i-1]=$(echo $line | awk -F',' "{print ${2}}")
        ((i=i+1))
    done < config.csv
    echo ${arr[@]}
}

# Identify files that are modified and need to be copied over--------------

# Use the git command to identify modified files in the directory and store the output in a variable
output=$(git -C . status --porcelain)

# Create an empty array to store modified file paths
modified_files=()

# Iterate over the output and extract the file paths of modified files
while read line; do
    if [[ "$line" =~ ^M ]]; then
        path=$(echo "$line" | awk '{print $2}')
        modified_files+=("$path")
    fi
done <<< "$output"

# Retrieve host information and copy files over ------------
SSH_HOSTS=($(parse_ssh_hosts config.sh \$5\"@\"\$4))

for host in "${SSH_HOSTS[@]}"; do

    echo $host
    for path in "${modified_files[@]}"; do
        deploy_to_replica $host $path &
    done
done

wait
