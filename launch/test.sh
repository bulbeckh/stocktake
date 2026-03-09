
for n in $(ros2 node list); do
  echo "== $n =="
  timeout 5s ros2 param get $n use_sim_time 2>/dev/null || echo "  (no response)"
done
