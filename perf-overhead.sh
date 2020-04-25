#!/bin/bash/

mkdir perf-num

TIME_SETUP="systemctl stop systemd-timesyncd.service; systemctl daemon-reload; systemctl stop ptp4l; systemctl stop phc2sys; systemctl start ptp4l; systemctl start phc2sys"

$TIME_SETUP

sleep 10

SERVER_IP="192.168.100.120"

for i in {1..12}
do
    mkdir perf-num/cyc-$i/
    for j in {1..10}
    do
        systemctl start phc2sys
	    sleep 30
        SERVER_CMD="perf record -e intel_pt/cyc=1,cyc_thresh=$i/ -m1G,1G --switch-events -a -C 14 taskset -a -c 14 ./memcached -l $SERVER_IP:11211 -u root -N enp94s0f0 -H &"
        CLIENT_CMD="/root/rubik-github/memcached/mem_setup -s $SERVER_IP -k 1500 -t 6 && /root/rubik-github/memcached/mem_client -s $SERVER_IP -k 1500 -r 10000"
        $SERVER_CMD &
	    sleep 30
        ssh sc2-hs2-b1542 $CLIENT_CMD
        cp memcached-kernel-tcp01.csv perf-num/cyc-$i/recv$j.csv
    done
done



mkdir perf-num/none/
for j in {1..10}
do
    systemctl start phc2sys
    sleep 30
	SERVER_CMD="taskset -a -c 14 ./memcached -l $SERVER_IP:11211 -u root -N enp94s0f0 -H &"
	CLIENT_CMD="/root/rubik-github/memcached/mem_setup -s $SERVER_IP -k 1500 -t 6 && /root/rubik-github/memcached/mem_client -s $SERVER_IP -k 1500 -r 10000"
	$SERVER_CMD &
	sleep 30
	ssh sc2-hs2-b1542 $CLIENT_CMD
	cp memcached-kernel-tcp01.csv perf-num/none/recv$j.csv
done
