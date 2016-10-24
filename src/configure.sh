#!/bin/bash

SYSSPEC=./common/sysspec.h

mapfile -t nic < <(ls /sys/class/net)
mapfile -t pid < <(cat /proc/sys/kernel/pid_max)

echo '#ifndef __NICNUM__' > $SYSSPEC
echo '#define __NICNUM__' >> $SYSSPEC
echo -e '\n\n#define NIC_NUM ' ${#nic[@]} >> $SYSSPEC
echo '#define PORT_NUM 65536' >> $SYSSPEC
echo -e '#define PID_NUM ' $pid '\n' >> $SYSSPEC
echo '#define MAX_TAG 255' >> $SYSSPEC
echo -e '#define NULL_TAG 0\n' >> $SYSSPEC
echo '#define WRITE_FULL_PORT_TO_PID PID_NUM+1' >> $SYSSPEC
echo '#define WRITE_FULL_PID_TO_TAG PID_NUM+2' >> $SYSSPEC
echo -e 'char *nic_ip[NIC_NUM] = {' >> $SYSSPEC


idx=0
rm -f ./common/niciplist
touch ./common/niciplist
for i in ${nic[@]}; 
do 
	ip[$idx]=$(ifconfig $i | awk -F"[: ]+" '/inet addr:/ {print $4}')
	echo ${ip[$idx]} >> ./common/niciplist
	echo -e '\t"'${ip[$idx]}'",'>> $SYSSPEC
	idx=$((idx+1))	
done

echo -e '\t};'>> $SYSSPEC
echo '#endif' >> $SYSSPEC









