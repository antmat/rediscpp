redis-server --port 6360 &
redis_pid=$!
./test 6360
kill $redis_pid