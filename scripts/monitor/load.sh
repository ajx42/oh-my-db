while true; do echo "$(date '+%Y-%m-%d %H:%M:%S') $(uptime | awk '{print $8,$9,$10}')"; sleep 1; done

