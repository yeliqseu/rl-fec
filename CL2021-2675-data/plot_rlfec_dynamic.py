#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Fri Nov  5 19:32:04 2021

@author: yeli
"""

import sys
import math
import numpy as np
from scipy import constants
import matplotlib.pyplot as plt
import subprocess
import pandas as pd

appPktSize = 1440 # in bytes

# Call awk script to process log
subprocess.run(["./process_scudp_data.sh", 
                "./SimData-RL-FEC/rlfec-dynamic-RngRun1000.log"])

est_rlfec = np.loadtxt("/tmp/Estimation.dat")
fec_rlfec = np.loadtxt("/tmp/Fec.dat")
win_rlfec = np.loadtxt("/tmp/Win.dat")
gpt_rlfec = np.loadtxt("/tmp/Goodput.dat")
tim_rlfec = pd.read_csv("/tmp/Time.dat", delim_whitespace=True, names=['hostID', 'pktType', 'pktID', 'sendTime', 'recvTime', 'ackTime', 'deliverTime'])


# Call awk script to process log
subprocess.run(["./process_scudp_data.sh", 
                "./SimData-RL-FEC/garrido-dynamic-RngRun1000.log"])

est_vq = np.loadtxt("/tmp/Estimation.dat")
fec_vq = np.loadtxt("/tmp/Fec.dat")
win_vq = np.loadtxt("/tmp/Win.dat")
gpt_vq = np.loadtxt("/tmp/Goodput.dat")
tim_vq = pd.read_csv("/tmp/Time.dat", delim_whitespace=True, names=['hostID', 'pktType', 'pktID', 'sendTime', 'recvTime', 'ackTime', 'deliverTime'])

hostid = 1;
fig, axs = plt.subplots(2, 2, figsize=(11, 10))

est1 = est_rlfec[est_rlfec[:,0]==hostid, :]
est2 = est_vq[est_vq[:,0]==hostid, :]
axs[0][0].plot(est1[:,1], est1[:,4])
axs[0][0].set_xlabel('Time (s)')
axs[0][0].set_ylabel('Loss Rate')
axs[0][0].grid(True)
axs[0][0].set_title('(a)')

fec1 = fec_rlfec[fec_rlfec[:,0]==hostid, :]
fec2 = fec_vq[fec_vq[:,0]==hostid, :]
axs[0][1].plot(fec1[:,1], fec1[:,2], fec2[:,1], fec2[:,2])
axs[0][1].set_xlabel('Time (s)')
axs[0][1].set_ylabel('FEC Code Rate')
axs[0][1].grid(True)
axs[0][1].legend(['RL-FEC', 'Threshold-Based'])
axs[0][1].set_title('(b)')

gpt1 = gpt_rlfec[gpt_rlfec[:,0]==hostid, :]
gpt2 = gpt_vq[gpt_vq[:,0]==hostid, :]
axs[1][0].plot(gpt1[:,1], gpt1[:,2] *  8 / 1e6, gpt2[:,1], gpt2[:,2] * 8 / 1e6)
axs[1][0].set_xlabel('Time (s)')
axs[1][0].set_ylabel('Cumulative Goodput (Mbps)')
axs[1][0].legend(['RL-FEC', 'Threshold-Based'])
axs[1][0].grid(True)
axs[1][0].set_title('(c)')

tim1 = tim_rlfec[ (tim_rlfec["hostID"]==hostid) & tim_rlfec["pktType"].str.match('S') ]
tim2 = tim_vq[ (tim_vq["hostID"]==hostid) & tim_vq["pktType"].str.match('S') ]
ddelay1 = (tim1['deliverTime'] - tim1['sendTime'])
ddelay2 = (tim2['deliverTime'] - tim2['sendTime'])
axs[1][1].plot(tim1['pktID'], ddelay1, tim2['pktID'], ddelay2)
axs[1][1].axhline(y=ddelay1.mean(), color='g', linestyle='--')
axs[1][1].axhline(y=ddelay2.mean(), color='r', linestyle='--')
axs[1][1].set_xlabel('Source Packet ID')
axs[1][1].set_ylabel('Delivery Time (s)')
axs[1][1].set_xlim([0, tim1['pktID'].max()])
axs[1][1].legend(['RL-FEC', 'Threshold-Based', 'RL-FEC Average', 'Threshold-Based Average'])
axs[1][1].grid(True)
axs[1][1].set_title('(d)')
#plt.savefig('/Users/yeli/Dropbox/Work/Papers/rlfec/figures/rlfec_garrido_dynamic_comp.pdf',bbox_inches='tight')