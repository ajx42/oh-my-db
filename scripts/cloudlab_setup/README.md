# pysoymocha
### Bunch of tools to help with setup

## Cloudlab

To generate a csv config and put it on every node. Note that the location of the config can be modified in `smutils.py`. By default Raft port is set to 8080 on all machines.

```bash
python sminstaller.py --manifest manifest.xml --pvt-key ~/.ssh/id_ed25519
python sminstaller.py --manifest manifest.xml --pvt-key ~/.ssh/id_ed25519 --raft-port 8081 // to modify raft port
```
Generate a TMUX script under `$PROJ_HOME/build`: `tmux.<session_name>.sh`.

```bash
# Local
python smtmuxgen.py --manifest manifest.xml --session ohmydb
``` 

Run this script to build out a tmux session. 