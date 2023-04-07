while true; do echo "$(date '+%Y-%m-%d %H:%M:%S') $(free -m | awk 'NR==2{print $2,$3}')"; sleep 1; done

