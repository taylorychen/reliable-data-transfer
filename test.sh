#!/usr/bin/zsh

make all

python3 rdcc_proxy.py --test_type rd --loss_rate 0.1 & sleep 0.5
# python3 rdcc_proxy.py --test_type cc & sleep 0.5

./server & sleep 0.5
./client mini.txt

wait %2
kill %1
kill %3

