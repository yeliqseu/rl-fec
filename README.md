# RL-FEC
## Introduction
This project implements a reinforcement learning (RL) based forward-erasure correction (FEC) scheme in ns-3. FEC is effective for achieving low-latency transmissions over lossy links with long propagation delay. Such links will be commonly seen in non-terrestrial networks (NTNs) in the forthcoming 6G, including satellite links, UAV/HAP links, etc. This RL-FEC scheme learns to dynamically send FEC repair packets for the streaming codes (https://github.com/yeliqseu/streamc) based on delayed feedback. The objective is to achieve fast in-order data delivery without retransmissions. SARSA with linear approximation (tile coding) is used for learning.

The repository already contains a trained agent (table1lambda0.000000) for out-of-box simulations. The agent was trained in environments with random packet loss rates between [0%-10%] and one-way propagation delay (300ms, i.e., RTT 600ms).

The work has been submiited to _IEEE Communications Letters_ for possible publication.

> F. Zhang, Y. Li*, J. Wang, T. Q. Quek, "Learning Based FEC for Non-Terrestrial Networks with Delayed Feedback", _IEEE Communications Letters_, **under review**.

## Installation
The codes have been tested in ns-3.35. Assume the installation path of ns-3.35 is `/home/username/ns-3.35/`.

1. Download RL-FEC

> cd /home/username/ns-3.35/examples/

> git clone https://github.com/yeliqseu/rl-fec.git

> mv rl-fec/table1lambda0.000000 /home/username/ns-3.35/

2. Download and compile the streaming code library
> cd some_folder_outside_ns-3.35

> git clone https://github.com/yeliqseu/streamc.git

> cd streamc && make libstreamc.a

> mv libstreamc.a /home/username/ns-3.35/examples/rl-fec/

3. Change the following paths of the wscript in examples/rl-fec, so that ns-3 can link to the streaming code library to perform coding/decoding:
> obj.env.append_value('CXXFLAGS','-I/home/username/ns-3.35/examples/rl-fec')
>
> obj.env.append_value('LINKFLAGS',['-L/home/username/ns-3.35/examples/rl-fec'])

## Simulation
Run the following command under `/home/username/ns-3.35/` to simulate RL-FEC:

>./waf --run "rlfec-stream-server-client-sim --nPackets=5000 --lossRate=0.05 --training=false --cbrMode=true --cbrRate=10Mbps"

This command sends at a constant bit rate 10Mbps over a $p_e=0.05$ lossy link with 600ms RTT. The simulation completes when 5000 source packets are successfully delivered to the remote node. There are several arugments can be passed to the simulation script. For example, `--dynamicPe=true` will change loss rate to a random value in [0,0.1] every one second. Please refer to `rlfec-stream-server-client-sim.cc` for details. 

The codes also contain a reference scheme proposed in 

> P Garrido Ortiz, DJ Leith, R Agüero Calvo, “Joint scheduling and coding for low in-order delivery delay over lossy paths with delayed feedback”, _IEEE/ACM Trans. Netw._, vol. 27, no. 5, pp. 1987–2000, 2019.

The scheme can be simulated by running
>./waf --run "rlfec-stream-server-client-sim --nPackets=5000 --lossRate=0.05 --garrido=true --cbrMode=true --cbrRate=10Mbps"

## Training
Training is performed by passing `--training=true` to the script. In this scenario, `table1lambda0.000000` under `/home/username/ns-3.35/` is loaded and updated from run to run. In each run, a random $p_e$ is chosen. 

As an example, the following script runs 5000 simulations for training using $\epsilon$-greedy. To balance explorationa and exploitation, $\epsilon=1.0,0.95,\ldots,0.05$ are successively used in the first $2000$ simulations, $100$ simulations for each $\epsilon$, respectively. Thereafter, constant $\epsilon=0.05$ is used.

```
cd /home/username/ns-3.35/
for i in $(seq 1 1 5000); do
    epsilon=$(printf "%0.2f\n" $(echo "scale=0; n=${i}/100; scale=4; ep=1-n*0.05; if (ep<0.05) {ep=0.05}; ep" | bc -q))
    logfile=/tmp/rlfec-training-RngRun${i}-epsilon${epsilon}.log
    ./waf --run "rlfec-stream-server-client-sim --nPackets=5000 --egreedy=${epsilon} --RngRun=${i} --training=true --cbrMode=true --cbrRate=10Mbps" &> ${logfile}
done
```