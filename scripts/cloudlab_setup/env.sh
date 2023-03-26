#! /bin/bash

# source this file!
echo 'n' | conda create --name mocha
conda activate mocha

conda install -c conda-forge paramiko pandas

export PROJ_HOME=$(pwd)