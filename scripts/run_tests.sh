set -x

start_test()
{
    SSH_HOST=$1
    iters=$2
    numPair=$3
    id=$4

    ssh ${SSH_HOST} << EOF
    ./oh-my-db/build/ohmyserver/client --config ./config.csv --iter $2 --numkeys $3
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

#./scripts/startup_cluster_db.sh &
./scripts/monitor/deploy_monitor.sh &

sleep 5

# here assuming the 4-th column is the hostname, and 5-th column is the username
SSH_HOSTS=($(parse_ssh_hosts config.sh \$5\"@\"\$4))

## Get the length of the array
size=${#SSH_HOSTS[@]}

for ((i=0; i<$size; i++)); do
    SSH_HOST=${SSH_HOSTS[i]}
    start_test ${SSH_HOST} 20 50 $i &
done

wait
