while true; do echo "$(date '+%Y-%m-%d %H:%M:%S') $(uptime | awk '{print $9,$10,$11}')"; sleep 1; done

