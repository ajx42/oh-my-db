## Correctness Test FAQs

The tests are in no way complete. Passing these does not imply 100% correctness. But they can help as preliminary quick tests while developing. Furthermore, they serve well as examples of how to use existing features to setup automated tests. Note that these tests run locally -- no cloudlab deployment is needed.

### How to run?

```bash
cd ..
source env.sh
cd tests
python run_correctness_test.py --code correctness_1 --timeout 25   

```

The `--code` param determines the test directory to run. `--timeout` is how long (in seconds) the test will run, after which the system is brought down and checks are performed to determine if the test was successful or now.

### What's inside a test directory?
The most basic test directory contains description and configuration of a single test. The following files are included:

1. `config.csv`: The usual cluster configuration file which is used everywhere in OhMyDB.
2. `repX.csv`: These files are used to generate the initial log configuration of a replica. Since we are using the persistent RAFT state as a way to launch the system in a variety of scenarios and let it play out. These files are inputs to the `writestore` tool.
3. `state.csv`: While `repX.csv` files define the starting state of logs for each replica, this file contains the starting values of `VotedFor` and `CurrentTerm` for the replicas.
4. `winner.csv`: This file is once again used to generate logs so it is in the same format as `repX.csv`. These are logs as expected in all replicas at end of the test run.

### I want to work on the tests, how can I help?
- We'd like to expand this test framework to support more intricate and larger tests. We might as well use a proper python testing framework.
- We have already done Network Parition tests, we would like to build those into this kind of test framework. For this please look at the `updatemask` tool.

### Who are the star developers behind this project?
This project was built with love by Tommy, Hayden, and Aditya.
