import os
import sys
import logging
import paramiko
import subprocess as sp

import smparser

from argparse import ArgumentParser

def __init_node_tmux(node, session):
    ssh_cmd = 'ssh -oStrictHostKeyChecking=no -p {} {}@{}'.format(
        node.port, node.username, node.hostname
    )
    script = '''\

tmux new-window -d -t {0} -n {1}
tmux select-window -t '={1}'
tmux send-keys -t 0 "{2}" Enter
    '''.format(session, node.name, ssh_cmd)
    
    return script

def generate_tmux_script(details, session, filename):
    logging.info('Generating TMUX Script: {}'.format(filename))
    
    with open(filename, 'w') as ff:
        ff.write('''\
#! /bin/bash
echo Session={0}
tmux kill-session -t {0}
tmux new -d -s {0}
tmux rename-window -t {0}:0 main
        '''.format(session))

        # create node windows
        for node in details:
            ff.write(__init_node_tmux(node, session))
        
        # attach final tmux setup
        ff.write("""\

tmux select-window -t '=main'
tmux attach -t {0}
        """.format(session))

    sp.check_call('chmod +x {}'.format(filename), shell=True)

def main():
    logging.basicConfig(level=logging.INFO)

    parser = ArgumentParser()

    parser.add_argument("--manifest", help="manifest for cloudlab")
    parser.add_argument("--session", help="(hopefully) unique session identifier")
    parser.add_argument("--output", help="output file path", default="tmux_script.sh")

    args = parser.parse_args()

    details = smparser.parse_manifest(args.manifest)
    logging.info('Discovered N={} Nodes: {}'.format(len(details), details))

    generate_tmux_script(details, args.session, args.output)

if __name__ == '__main__':
    main()
