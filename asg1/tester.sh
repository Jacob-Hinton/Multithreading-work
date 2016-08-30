#!/bin/bash
i="0"

while [ $i -lt 100 ]
do
    taskset 0x000001 ./montecarloasg/xmcint -s 64923145
    i=$[$i+1]
done
