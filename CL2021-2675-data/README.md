# Process RL-FEC Logs and Reproduce Plots

This folder contains `awk` scripts for procesing RL-FEC's ns-3 logs obtained by redirecting `rlfec-stream-server-client-sim`'s output. The folder also contains python scripts and some example logs for reproducing the figures in the author's IEEE Communications Letters (CL2021-2675).

1. `process_scudp_data.sh` is the main `awk` script for processing logs to obtain performance metics such as goodput, FEC code rate, delivery delay, etc. There is no need to directly call it. It is called from within python scripts using `subprocess.run()`.

2. `plot_scudp_data.py` is a tool script to conveniently plot a summary of the FEC transmission process, which calls `process_scudp_data.sh` to process the log. For example, run the script in Spyder will plot the results from ``./SimData-RL-FEC/rlfec-dynamic-RngRun1000.log``. Process and plot other logs obtained from RL-FEC simualtions only needs to change the file name accordingly.

3. `plot_rlfec_data.py` reproduces Fig. 2 and Fig. 3 of the submitted paper. The script calls `process_scudp_goodput.sh` and `process_scudp_ddelay.sh` which are clipped from `process_scudp_data.sh` for obtaining more specific data.

4. `plot_rlfec_dynamic.py` reproduces Fig. 4 of the submitted paper, which calls `process_scudp_data.sh`.

5. `SimData-RL-FEC` folder contains example raw logs and processed data of training and testing.
