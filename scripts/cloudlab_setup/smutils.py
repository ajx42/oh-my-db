import os
import paramiko
from types import SimpleNamespace

PROJ_HOME = os.environ['PROJ_HOME']

consts = SimpleNamespace()
consts.locations = SimpleNamespace()
consts.values = SimpleNamespace()
consts.keywords = SimpleNamespace()

consts.locations.BUILDDIR   = os.path.join(PROJ_HOME, 'build')
consts.locations.CONFIGPATH = './config.csv'

consts.values.raft_port = 8080

consts.keywords.TMUX = 'tmux'

def __get_script(script, session):
    return '.'.join([script, session, 'sh'])

def __get_generated_tmux_script(session):
    return os.path.join(consts.locations.BUILDDIR, __get_script(consts.keywords.TMUX, session))


smfiles = SimpleNamespace()
smfiles.generated = SimpleNamespace()
smfiles.target = SimpleNamespace()

smfiles.generated.tmux_script = __get_generated_tmux_script

def get_connections(details, pkey):
    cons = []
    for node in details:
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        client.connect(
            hostname=node.hostname,
            username=node.username,
            port=node.port,
            pkey=pkey
        )
        cons.append(client)
    return cons

