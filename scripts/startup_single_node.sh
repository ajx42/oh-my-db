set -x

start_replica()
{
    SSH_HOST=$1
    id=$2

    ssh ${SSH_HOST} << EOF
    cd oh-my-db
    ./scripts/monitor/monitor.sh &
    cd ..
    ./oh-my-db/build/ohmyserver/replica --config ./config.csv --id $2 --db_path /tmp/db &
    ./oh-my-db/build/ohmyserver/client --config ./config.csv --iter 20 --numkeys 50 &
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


# here assuming the 4-th column is the hostname, and 5-th column is the username
SSH_HOSTS=($(parse_ssh_hosts config.sh \$5\"@\"\$4))

# Get the length of the array
#size=${#SSH_HOSTS[@]}

#for SSH_HOST in "${SSH_HOSTS[@]}"
#for ((i=0; i<$size; i++)); do

SSH_HOST=${SSH_HOSTS[$1]}

start_replica ${SSH_HOST} $1 &

#done

wait
