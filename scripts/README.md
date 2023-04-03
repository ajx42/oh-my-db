## `node_setup.sh`
To be ran on a CloudLab node. Installs and updates dependent packages.

## `cluster_setup.sh`
To be ran on your local machine. 
### Requirement
* Copy this script to your local machine.
* After creating a CloudLab cluster, copy the manifest and put it in `manifest.xml`. 
* Run `./cluster_setup.sh`. 

### What it does
* Clones this repo.
* Checkout the specified branch.
* Compress this repo.
* Creates `config.csv` by parsing `manifest.xml`.
* Sends the compressed repo and `config.csv` to each of the node in the cluster.
* Run `node_setup.sh` and build everything on each of the node in the cluster.
* Generate a tmux script that opens a window for each node + your local machine.
* Run the tmux script.