import os
import logging
import sys
import paramiko
import subprocess as sp
import json

from smparser import parse_manifest
from smutils import consts, get_connections
from argparse import ArgumentParser

def create_config(details, pkey, config_path):
    # create json string for details
    config = {}
    for node in details:
        config[node.name] = {}
        for k, v in node._asdict().items():
            if k == "name": continue
            config[node.name][k] = v
    
    for node, client in zip(details, get_connections(details, pkey)):
        # write config file
        _, out, err = client.exec_command("echo \'{}\' > {}".format(json.dumps(config), config_path))


def main():
    logging.basicConfig(level=logging.INFO)

    parser = ArgumentParser()
    parser.add_argument("--manifest", help="manifest for cloudlab")
    parser.add_argument("--pvt-key", help="private key (file)")
    
    args = parser.parse_args()

    pkey = paramiko.Ed25519Key.from_private_key_file(args.pvt_key)
    
    details = parse_manifest(args.manifest)

    logging.info('Discovered N={} Nodes: {}'.format(len(details), details))
    logging.info('Authentication enabled via ssh keys only')    
    
    create_config(details, pkey, consts.locations.CONFIGPATH)

if __name__ == '__main__':
    main()
