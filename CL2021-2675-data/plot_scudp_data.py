#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Apr 26 10:13:55 2021

@author: yeli
"""

import sys
import math
import numpy as np
from scipy import constants
import matplotlib.pyplot as plt
import subprocess
import pandas as pd

# Call awk script to process log
subprocess.run(["./process_scudp_data.sh", 
                "./SimData-RL-FEC/rlfec-dynamic-RngRun1000.log"])
               


estfname = "/tmp/Estimation.dat"
fecfname = "/tmp/Fec.dat"
winfname = "/tmp/Win.dat"
gptfname = "/tmp/Goodput.dat"
timfname = "/tmp/Time.dat"
losfname = "/tmp/Loss.dat"
appPktSize = 1440 # in bytes

est = np.loadtxt(estfname)
fec = np.loadtxt(fecfname)
win = np.loadtxt(winfname)
gpt = np.loadtxt(gptfname)
#los = np.loadtxt(losfname)
tim = pd.read_csv(timfname, delim_whitespace=True, names=['hostID', 'pktType', 'pktID', 'sendTime', 'recvTime', 'ackTime', 'deliverTime'])

hostid = 1;


fig, axs = plt.subplots(3, 3, figsize=(20, 15))
est1 = est[est[:,0]==hostid, :]
axs[0][0].plot(est1[:,1], est1[:,2]* appPktSize * 8 / 1e6)
axs[0][0].set_xlabel('Time (s)')
axs[0][0].set_ylabel('Bandwidth (Mbps)')
axs[0][0].grid(True)

axs[1][0].plot(est1[:,1], est1[:,6])
axs[1][0].set_xlabel('Time (s)')
axs[1][0].set_ylabel('Instant RTT (ms)')
axs[1][0].grid(True)

axs[2][0].plot(est1[:,1], est1[:,4])
axs[2][0].set_xlabel('Time (s)')
axs[2][0].set_ylabel('Loss Rate')
axs[2][0].grid(True)

fec1 = fec[fec[:,0]==hostid, :]
axs[2][1].plot(fec1[:,1], fec1[:,2])
axs[2][1].set_xlabel('Time (s)')
axs[2][1].set_ylabel('FEC Code Rate')
axs[2][1].grid(True)


gpt1 = gpt[gpt[:,0]==hostid, :]
axs[0][1].plot(gpt1[:,1], gpt1[:,2] *  8 / 1e6, gpt1[:,1], gpt1[:,3] * 8 / 1e6)
axs[0][1].set_xlabel('Time (s)')
axs[0][1].set_ylabel('Goodput (Mbps)')
axs[0][1].legend(['Goodput', 'Throughput'])
axs[0][1].grid(True)

win1 = win[win[:,0]==hostid, :]
# Fetch average decoding window size of the host from Time.dat
avgwin = float(tim[ (tim["hostID"]==hostid) & tim["pktID"].isnull() ]["pktType"].values[0])
axs[1][1].plot(win1[:,1], win1[:,2])
axs[1][1].axhline(y=avgwin, color='r', linestyle='--')
axs[1][1].set_xlabel('Time (s)')
axs[1][1].set_ylabel('Decoding Window (#pkts)')
axs[1][1].legend(['Window Length', 'Average Window Length'])
axs[1][1].grid(True)

gpt1 = gpt[gpt[:,0]==hostid, :]
axs[0][2].plot(gpt1[:,1], gpt1[:,2] *  gpt1[:,1]/appPktSize)
axs[0][2].set_xlabel('Time (s)')
axs[0][2].set_ylabel('# of Delivered Packets')
axs[0][2].grid(True)


tim1 = tim[ (tim["hostID"]==hostid) & tim["pktType"].str.match('S') ]
ddelay = (tim1['deliverTime'] - tim1['sendTime'])
axs[1][2].plot(tim1['pktID'], ddelay)
axs[1][2].axhline(y=ddelay.mean(), color='r', linestyle='--')
axs[1][2].set_xlabel('Source Packet ID')
axs[1][2].set_ylabel('Time (s)')
axs[1][2].set_xlim([0, tim1['pktID'].max()])
axs[1][2].legend(['Packet Delivery Time', 'Average Delivery Time'])
axs[1][2].grid(True)
