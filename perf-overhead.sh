#!/bin/bash/

mkdir perf-num


for i in {1..12}
do
    mkdir perf-num/cyc-$i/
    for j in {1..10}
    do
        SERVER_CMD="perf record -e intel_pt/cyc=1,cyc_thresh=$i/ -m1G,1G --switch-events -a -C 14 taskset -a -c 14 ./memcached -l 192.168.100.121:11211 -u root -N enp94s0f0 -H &"
        CLIENT_CMD="/root/rubik-github/memcached/mem_setup -s 192.168.100.121 -k 1500 -t 6 && /root/rubik-github/memcached/mem_client -s 192.168.100.121 -k 1500 -r 10000"
        $SERVER_CMD &
	sleep 5
        ssh sc2-hs2-b1541 $CLIENT_CMD
        cp memcached-kernel-tcp01.csv perf-num/cyc-$i/recv$j.csv
	sleep 2
    done
done



mkdir perf-num/none/
for j in {1..10}
do
	SERVER_CMD="taskset -a -c 14 ./memcached -l 192.168.100.121:11211 -u root -N enp94s0f0 -H &"
	CLIENT_CMD="/root/rubik-github/memcached/mem_setup -s 192.168.100.121 -k 1500 -t 6 && /root/rubik-github/memcached/mem_client -s 192.168.100.121 -k 1500 -r 10000"
	$SERVER_CMD &
	sleep 5
	ssh sc2-hs2-b1541 $CLIENT_CMD
	cp memcached-kernel-tcp01.csv perf-num/none/recv$j.csv
	sleep 2
done
