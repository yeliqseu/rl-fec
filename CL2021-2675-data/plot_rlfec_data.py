#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sun Oct  3 08:56:56 2021

@author: yeli
"""


import sys
import math
import numpy as np
from scipy import constants
import matplotlib.pyplot as plt
import subprocess
import pandas as pd

appPktSize = 1440   # bytes


basestring1 = "./SimData-RL-FEC/highRTT-rlfec-testing-greedy-pe"
fig, axs = plt.subplots(2, 2, figsize=(11, 10))

pes = np.arange(0, 0.11,0.01)
fecrates = np.zeros([2, pes.shape[0]])
basestring2 = "./SimData-RL-FEC/processed/fecrate_and_ddelay_testing_trained_agent_pe"
basestring3 = "./SimData-RL-FEC/processed/fecrate_and_ddelay_Garrido_gamma1.0_pe"
fecrates = np.zeros([4, pes.shape[0]])
for pe in pes:
    datfile = basestring2 + "{:.2f}".format(pe) + '.dat'
    x = np.loadtxt(datfile)
    datfile = basestring3 + "{:.2f}".format(pe) + '.dat'
    y = np.loadtxt(datfile)
    fecrates[0, int(pe/0.01)] = np.mean(x[:,2])
    fecrates[1, int(pe/0.01)] = np.std(x[:,2])
    fecrates[2, int(pe/0.01)] = np.mean(y[:,2])
    fecrates[3, int(pe/0.01)] = np.std(y[:,2])

axs[0][0].errorbar(pes, fecrates[0,:], yerr=fecrates[1,:], fmt='o', capsize=3)
axs[0][0].errorbar(pes, fecrates[2,:], yerr=fecrates[3,:], fmt='x', capsize=3)
axs[0][0].grid(True)
axs[0][0].set_xlabel('$p_e$',fontsize=12)
axs[0][0].set_ylabel('Achieved FEC Code Rate',fontsize=12)
axs[0][0].set_ylim([0.75, 1.01])
axs[0][0].set_title('(a)',fontsize=10)
axs[0][0].legend(['RL-FEC', 'Threshold-Based'], frameon=True, fontsize=12)

RL_CoV = np.loadtxt("./SimData-RL-FEC/processed/rlfec_CoV_testing_trained_agent.dat")
VQ_CoV = np.loadtxt("./SimData-RL-FEC/processed/Garrido_CoV.dat")
axs[0][1].plot(RL_CoV[:,0], RL_CoV[:,1],'o-')
axs[0][1].plot(VQ_CoV[:,0], VQ_CoV[:,1],'x-')
axs[0][1].grid(True)
axs[0][1].set_xlabel('$p_e$',fontsize=12)
axs[0][1].set_ylabel('Short-Term Goodput CoV',fontsize=12)
axs[0][1].set_title('(b)',fontsize=10)
axs[0][1].legend(['RL-FEC', 'Threshold-Based'], frameon=True, fontsize=12)

pes = [0.05]
basestring11 = "./SimData-RL-FEC/highRTT-Garrido-gamma1.0-pe"
for pe in pes:
    rawfile = basestring1 + "{:.2f}".format(pe) + '-RngRun10001.log'
    datfile = '/tmp/rlfec-' + "{:.2f}".format(pe) + '-ddelay.dat'
    subprocess.run(["./process_scudp_ddelay.sh", rawfile, datfile])
    ddelay = np.loadtxt(datfile)
    axs[1][0].plot(ddelay[:,4], ddelay[:,0])
    
    datfile = '/tmp/rlfec-' + "{:.2f}".format(pe) + '.dat'
    subprocess.run(["./process_scudp_goodput.sh", rawfile, datfile])
    gpt = np.loadtxt(datfile)
    axs[1][1].plot(gpt[:,1], gpt[:,2] * 8 / 1e6)
    
    rawfile = basestring11 + "{:.2f}".format(pe) + '-RngRun10001.log'
    datfile = '/tmp/garrido-' + "{:.2f}".format(pe) + '-ddelay.dat'
    subprocess.run(["./process_scudp_ddelay.sh", rawfile, datfile])
    ddelay = np.loadtxt(datfile)
    axs[1][0].plot(ddelay[:,4], ddelay[:,0])
    
    datfile = '/tmp/garrido-' + "{:.2f}".format(pe) + '.dat'
    subprocess.run(["./process_scudp_goodput.sh", rawfile, datfile])
    gpt = np.loadtxt(datfile)
    axs[1][1].plot(gpt[:,1], gpt[:,2] * 8 / 1e6)
    
axs[1][0].set_xlabel("Time (s)",fontsize=12)
axs[1][0].set_ylabel("Delivered Packet ID",fontsize=12)    
axs[1][0].grid(True)
axs[1][0].set_xticks(np.arange(0,9,1))
axs[1][0].legend(['RL-FEC, $p_e=0.05$', 'Threshold-Based, $p_e=0.05$'], frameon=True, fontsize=12)
axs[1][1].set_xlabel("Time (s)",fontsize=12)
axs[1][1].set_ylabel("Cumulative Goodput (Mbps)",fontsize=12)    
axs[1][1].grid(True)
axs[1][1].set_xticks(np.arange(0,9,1))
axs[1][0].set_title('(c)',fontsize=10)
axs[1][1].set_title('(d)',fontsize=10)
axs[1][1].legend(['RL-FEC, $p_e=0.05$', 'Threshold-Based, $p_e=0.05$'], frameon=True, fontsize=12)


#plt.savefig('/Users/yeli/Dropbox/Work/Papers/rlfec/figures/greedy_testing_example.pdf',bbox_inches='tight')

basestring = "./SimData-RL-FEC/processed/fecrate_and_ddelay_modified_testing_trained_agent_pe"
pes = np.arange(0, 0.101,0.01)
fecrates = np.zeros([2, pes.shape[0]])
maxdelay = np.zeros([2, pes.shape[0]])
avgdelay = np.zeros([2, pes.shape[0]])
maxdww = np.zeros([2, pes.shape[0]])
avgdww = np.zeros([2, pes.shape[0]])
for pe in pes:
    datfile = basestring + "{:.2f}".format(pe) + '.dat'
    x = np.loadtxt(datfile)
    fecrates[0, int(pe/0.01)] = np.mean(x[:,2])
    fecrates[1, int(pe/0.01)] = np.std(x[:,2])
    maxdelay[0, int(pe/0.01)] = np.mean(x[:,3])
    maxdelay[1, int(pe/0.01)] = np.std(x[:,3])
    avgdelay[0, int(pe/0.01)] = np.mean(x[:,4])
    avgdelay[1, int(pe/0.01)] = np.std(x[:,4])
    maxdww[0, int(pe/0.01)] = np.mean(x[:,5])
    maxdww[1, int(pe/0.01)] = np.std(x[:,5])
    avgdww[0, int(pe/0.01)] = np.mean(x[:,6]/x[:,7])
    avgdww[1, int(pe/0.01)] = np.std(x[:,6]/x[:,7])
fig, axs = plt.subplots(2, 2, figsize=(9,8))
gs = axs[0,0].get_gridspec()
axs[0,0].remove()
axs[0,1].remove()
axbig = fig.add_subplot(gs[0,0:])

axbig.errorbar(pes, fecrates[0,:], yerr=fecrates[1,:], fmt='o', capsize=3)
axbig.set_xlabel("$p_e$")
axbig.set_ylabel("FEC Code Rate")
axbig.set_ylim([0.8, 1.0])
axbig.set_xticks(pes)
axbig.grid(True)
axs[1][0].errorbar(pes, maxdelay[0,:], yerr=maxdelay[1,:], fmt='o', capsize=3)
axs[1][0].set_xlabel("$p_e$")
axs[1][0].set_ylabel("Max Delivery Delay (s)")
axs[1][0].grid(True)  
axs[1][1].errorbar(pes, avgdelay[0,:], yerr=avgdelay[1,:], fmt='o', capsize=3)
axs[1][1].set_xlabel("$p_e$")
axs[1][1].set_ylabel("Average Delivery Delay (s)")
axs[1][1].grid(True) 
axbig.set_title('(a)')
axs[1][0].set_title('(b)')
axs[1][1].set_title('(c)')
    
#plt.savefig('/Users/yeli/Dropbox/Work/Papers/rlfec/figures/mod_greedy_testing_summary.pdf',bbox_inches='tight')