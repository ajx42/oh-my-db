while true; do echo "$(date '+%Y-%m-%d %H:%M:%S') $(free -m | awk 'NR==2{print $2,$3,$4,$5,$6,$7}')"; sleep 1; done

