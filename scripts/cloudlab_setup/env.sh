#! /bin/bash

# source this file!
echo 'n' | conda create --name mocha
conda activate mocha

conda install -c conda-forge paramiko

export PROJ_HOME=$(pwd)