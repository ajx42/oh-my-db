import subprocess as sp
import argparse
import sys
import csv
import os
import datetime
import logging
import time
import signal

CFG_FNAME = 'config.csv'
STATE_FNAME = 'state.csv'
WINNER_FNAME = 'winner.csv'
VOTED_FOR = 'votedFor'
CURR_TERM = 'currentTerm'


WRITESTORE = '{0}/writestore --id {1} --outputdir {2} --input {3}/rep{1}.csv --currentterm {4} --votedfor {5}'
LAUNCH = '{0}/replica --id {1} --config {2} --db_path {3}/db{1} --storedir {3}'
READSTORE = '{0}/readstore --vec --file {1}/raft.{2}.log.persist'


def main():
    logging.basicConfig(level=logging.INFO)
    
    parser = argparse.ArgumentParser(description='I help test OhMyRaft\'s correctness')
    parser.add_argument('--code', required=True, help='which test? eg. correctness_1')
    parser.add_argument('--timeout', required=True, help='number of seconds to wait before killin\'')
    try:
        args = parser.parse_args()
    except:
        parser.print_help()
        return
    
    binBasePath     = os.path.join(os.environ['PROJ_HOME'], 'build', 'bin')
    testBasePath    = os.path.join(os.environ['PROJ_HOME'], 'tests', args.code)
    testTmpPath     = os.path.join(testBasePath, 'tmp')
    cfgFile         = os.path.join(testBasePath, CFG_FNAME)
    stateFile       = os.path.join(testBasePath, STATE_FNAME)
    winnerFile      = os.path.join(testBasePath, WINNER_FNAME)

    timeoutSecs = int(args.timeout)

    os.makedirs(testTmpPath, exist_ok=True)

    replicaDetails = {}

    with open(cfgFile) as cfgcsv:
        reader = csv.DictReader(cfgcsv)
        for row in reader:
            replicaDetails[int(row['name'][4:].strip())] = row

    with open(stateFile) as statecsv:
        reader = csv.DictReader(statecsv)
        for row in reader:
            repId = int(row['id'].strip())
            if repId not in replicaDetails:
                continue
            else:
                replicaDetails[repId][VOTED_FOR] = int(row[VOTED_FOR].strip())
                replicaDetails[repId][CURR_TERM] = int(row[CURR_TERM].strip())
    
    logging.info('Discovered NumReplicas={}'.format(len(replicaDetails)))
    for repId in replicaDetails:
        bootstrapDisabled = VOTED_FOR not in replicaDetails[repId] or CURR_TERM not in replicaDetails[repId]
        logging.info('RepId={} BootstrapEnabled={}'.format(repId, not bootstrapDisabled))
        replicaDetails[repId]['BootstrapEnabled'] = not bootstrapDisabled

    logging.info('Generating Bootstrap Store @ Loc=[{}]'.format(testTmpPath))
    for repId in replicaDetails:
        repCfg = replicaDetails[repId]
        if not repCfg['BootstrapEnabled']:
            continue
        CMD = WRITESTORE.format(
            binBasePath, repId, testTmpPath, testBasePath,
            repCfg[VOTED_FOR], repCfg[CURR_TERM]
        )
        
        sp.check_call(CMD, shell=True)

    repPROs = []

    logging.info('Launching Replicas')
    for repId in replicaDetails:
        CMD = LAUNCH.format(
            binBasePath, repId, cfgFile, testTmpPath )
        if replicaDetails[repId]['BootstrapEnabled']:
            CMD += ' --bootstrap'
        pro = sp.Popen(CMD, shell=True)
        repPROs.append(pro)

    time.sleep(timeoutSecs)
    sp.check_call("pkill -f replica", shell=True)

    repLogs = []
    for repId in replicaDetails:
        CMD = READSTORE.format(
            binBasePath, testTmpPath, repId
        )
        result = sp.run(CMD, shell=True, stdout=sp.PIPE, text=True)
        print(result.stdout)
        lines = [ x.strip()  for x in result.stdout.split('\n') if 'LogEntry' in x ]
        terms = [ int(x.split('=', 1)[1][1:-1].split(' ')[0].split('=')[1].strip()) for x in lines ]
        repLogs.append(terms)

    winnerLog = []

    with open(winnerFile) as wincsv:
        reader = csv.DictReader(wincsv)
        for row in reader:
            for _ in range(int(row['count'])):
                winnerLog.append(int(row['term']))

    for i, logs in enumerate(repLogs):
        if logs != winnerLog:
            logging.error("Log Mismatched RepId={}".format(i))
        else:
            logging.info("Log OK RepId={}".format(i))


if __name__ == '__main__':
    main()
