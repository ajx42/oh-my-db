import os
import logging
import sys
import paramiko
import subprocess as sp
import pandas as pd

from smparser import parse_manifest
from smutils import consts, get_connections
from argparse import ArgumentParser

def create_config(details, pkey, config_path, raft_port):
    config = pd.DataFrame(columns=details[0]._fields)

    for node_info in details:
        node_info = node_info._replace(port=raft_port)
        config = pd.concat([config, pd.DataFrame(node_info._asdict(), index=[0])], ignore_index=True)
    
    for node, client in zip(details, get_connections(details, pkey)):
        _, out, err = client.exec_command('echo "{}" > {}'.format(config.to_csv(index=False).replace('\n', '\\n'), config_path))

def main():
    logging.basicConfig(level=logging.INFO)

    parser = ArgumentParser()
    parser.add_argument("--manifest", help="manifest for cloudlab")
    parser.add_argument("--pvt-key", help="private key (file)")
    parser.add_argument("--raft-port", help="raft port", default=consts.values.raft_port)
    
    args = parser.parse_args()

    pkey = paramiko.Ed25519Key.from_private_key_file(args.pvt_key)
    raft_port = args.raft_port
    details = parse_manifest(args.manifest)

    logging.info('Discovered N={} Nodes: {}'.format(len(details), details))
    logging.info('Authentication enabled via ssh keys only')    
    
    create_config(details, pkey, consts.locations.CONFIGPATH, raft_port)

if __name__ == '__main__':
    main()
