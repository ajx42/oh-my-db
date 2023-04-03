#!/bin/bash

# change shell to bash
sudo usermod -s /bin/bash $USER

# update gcc
yes '' | sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt-get -y update
sudo apt-get -y install gcc-9 g++-9
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-9

# update git
yes '' | sudo add-apt-repository ppa:git-core/ppa
sudo apt -y update
sudo apt -y install git

# install cmake
sudo apt update -y && \
sudo apt install -y software-properties-common lsb-release && \
sudo apt clean all
wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | sudo tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
sudo apt-add-repository "deb https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main"
sudo apt update -y
sudo apt install -y kitware-archive-keyring
sudo rm /etc/apt/trusted.gpg.d/kitware.gpg
sudo apt update -y
sudo apt install -y cmake
