# valgrind --leak-check=full --show-leak-kinds=all 

echo "Lanching server."
./server.out '::1' '9090' &
sleep 1

hash1='3b64d11ff0ac5da6a262b34977bb3d29ae0a4c030947c11e1a0d9e51c848f772'
hash2='8962235e792f6b112f04f61635f30dbb886bb9d8ed6e694a7e24bbe7529872df'

echo ""
echo "Populating..."
./client.out '::1' '9090' 'put' $hash1 'client_1' | \grep '\[CLIENT\]'
./client.out '::1' '9090' 'put' $hash1 'client_2' | \grep '\[CLIENT\]'
./client.out '::1' '9090' 'put' $hash1 'client_1' | \grep '\[CLIENT\]'
./client.out '::1' '9090' 'put' $hash2 'client_1' | \grep '\[CLIENT\]'
sleep 1

echo ""
echo "Asking for hashes"
./client.out '::1' '9090' 'get' $hash1 | \grep '\[CLIENT\]'
./client.out '::1' '9090' 'get' 'inexistant' | \grep '\[CLIENT\]'
sleep 1

echo ""
echo "Asking server to share all of his hashes"
printf "plzgibhashes" | nc -q1 -u 'localhost' '9090'
sleep 1

echo ""
echo "Giving server a hash from another server"
printf "kktakethis $hash2 client_3 $(date +%s)" | nc -q1 -u 'localhost' '9090'
sleep 1

kill $(jobs -p)
