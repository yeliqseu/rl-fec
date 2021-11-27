#!/bin/bash

#estfile=$(mktemp)
estfile="Estimation.dat"
awk '$1~/Client/ {split($2,a,"."); hostid=a[3]} $6 ~ /Estimation/ {if (start[hostid]==0) {start[hostid]=1; nhost+=1}; if (complete[hostid]==0) {print hostid, $4, $8, $12, $15, $25, $28}} $6 ~ /Completion/ {complete[hostid]=1}' $1 > /tmp/$estfile

#fecfile=$(mktemp)
fecfile="Fec.dat"
awk '$1~/Client/ {split($2,a,"."); hostid=a[3]} $6 ~ /SendPacket/ {if (start[hostid]==0) {start[hostid]=1; nhost+=1}; if ($9=="SOURCE") {nsrc[hostid]+=1}; if ($9=="REPAIR") {nrep[hostid]+=1}; if (complete[hostid]==0) {print hostid, $4, nsrc[hostid]/(nsrc[hostid]+nrep[hostid])}} $6 ~ /Completion/ {complete[hostid]=1}' $1 > /tmp/$fecfile

#winfile=$(mktemp)
winfile="Win.dat"
awk '$1~/Client/ {split($2,a,"."); hostid=a[3]} $6 ~ /PacketReceive/ {if (start[hostid]==0) {start[hostid]=1; nhost+=1}; if ($13=="decoding") {decwin[hostid]=$18-$16+1} if ($20=="decoding") {decwin[hostid]=$25-$23+1} if ($13=="decoder") {decwin[hostid]=0}; if (complete[hostid]==0) {print hostid, $4, decwin[hostid]}} $6 ~ /Completion/ {complete[hostid]=1}' $1 > /tmp/$winfile

#gpfile=$(mktemp)
gptfile="Goodput.dat"
awk '$1~/Client/ {split($2,a,"."); hostid=a[3]}; $6 ~ /PacketReceive/ {if (start[hostid]==0) {start[hostid]=1; nhost+=1}; nrecv[hostid]+=1} $6 ~ /SendACK/ {print hostid, $4, ($8+1)*1440/$4, nrecv[hostid]*1440/$4}; $6 ~ /TcpPacketSink/ {tcprecv[hostid]+=$8; print hostid, $4, tcprecv[hostid]/$4, tcprecv[hostid]/$4}' $1 > /tmp/$gptfile

#timefile=$(mktemp)
timefile="Time.dat"
awk '$1~/Client/ {split($2,a,"."); hostid=a[3]; if (seen[hostid]==0) {seen[hostid]=1; nhost+=1}}; $6~/SendPacket/ {if ($9=="SOURCE") {src_stime[hostid,$11]=$4; nsrc[hostid]+=1}; if ($9=="REPAIR") {rep_stime[hostid,$11]=$4; nrep[hostid]+=1}} $6~/PacketReceive/ {if ($8=="source") {src_rtime[hostid,$11]=$4}; if ($8=="repair") {rep_rtime[hostid,$11]=$4}} $6~/DecoderSuccess/ {for (i=$14;i<=$16;i++) {delivertime[hostid,i]=$4}; delay=$16-$14; if (delay>0) {sumdelay[hostid]+=delay; nbusy[hostid]+=1}} $6~/\[ACK/ {if ($11=="SOURCE") {src_acktime[hostid,$13]=$4}; if ($11=="REPAIR") {rep_acktime[hostid,$13]=$4}} END {for (h=1;h<=nhost;h++) {for (i=0;i<nsrc[h];i++) {if (src_rtime[h,i]==0) {src_rtime[h,i]=-1; src_acktime[h,i]=-1}; print h, "S", i, src_stime[h,i], src_rtime[h,i], src_acktime[h,i], delivertime[h,i]}; for (i=0;i<nrep[h];i++) {if (rep_rtime[h,i]==0) {rep_rtime[h,i]=-1; rep_acktime[h,i]=-1}; print h, "R", i, rep_stime[h,i], rep_rtime[h,i], rep_acktime[h,i]}; if (nbusy[h]!=0) {print h, sumdelay[h]/nbusy[h]}}}' $1 > /tmp/$timefile

timefile2="Tcp-Time.dat"
awk 'BEGIN {seen=0} $6~/BulkSend/ {sent+=$8; sid=sent/1440; sendtime[sid]=$4} $6~/PacketSink/ {for (i=1;i<=$8/1440;i++) {seen+=1;recvtime[seen]=$4}} END {for (i=1;i<=seen;i++) {print i, sendtime[i], recvtime[i], recvtime[i]-sendtime[i]}}' $1 > /tmp/${timefile2}

lossfile="Loss.dat"
awk '$1~/Client/ {split($2,a,"."); hostid=a[3]; if (seen[hostid]==0) {seen[hostid]=1; nem[hostid]=0; nqdsender[hostid]=0; nqdbtnk[hostid]=0; nhost+=1}} $6~/PhyRxDrop/ {if ($11!=-1) {seesrc[hostid,$11]=1} else {seerep[hostid,$13]=1}; nloss[hostid]+=1; nem[hostid]+=1} $6~/QueueDrop/ {if ($11!=-1) {seesrc[hostid,$11]=1} else {seerep[hostid,$13]=1}; nloss[hostid]+=1; nqd[hostid]+=1; split($14,a,"/"); if (a[3]==0) {nqdbtnk[hostid]+=1} else {nqdsender[hostid]+=1}} $6~/PacketReceive/ {if ($8=="source") {seesrc[hostid,$11]=1; lastseesrc[hostid]=$11} else {seerep[hostid,$11]=1; lastseerep[hostid]=$11}; nrecv[hostid]+=1} $6~/Completion/ {print hostid, nem[hostid], nqdsender[hostid], nqdbtnk[hostid], nrecv[hostid], lastseesrc[hostid], lastseerep[hostid]; for (i=0;i<=lastseesrc[hostid];i++) {if (seesrc[hostid,i]==0) {print i}}; for (i=0;i<=lastseerep[hostid];i++) {if (seerep[hostid,i]==0) {print i}}}' $1 | sort -n -k1 > /tmp/$lossfile
