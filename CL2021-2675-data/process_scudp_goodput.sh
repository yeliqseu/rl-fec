#!/bin/bash
awk '$1~/Client/ {split($2,a,"."); hostid=a[3]}; $6 ~ /PacketReceive/ {if (start[hostid]==0) {start[hostid]=1; nhost+=1}; nrecv[hostid]+=1} $6 ~ /SendACK/ {print hostid, $4, ($8+1)*1440/$4, nrecv[hostid]*1440/$4}; $6 ~ /TcpPacketSink/ {tcprecv[hostid]+=$8; print hostid, $4, tcprecv[hostid]/$4, tcprecv[hostid]/$4}' $1 > $2
