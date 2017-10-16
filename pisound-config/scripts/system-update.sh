#!/bin/sh

SOFTWARE_TO_INSTALL="pisound-btn pisound-ctl"
sudo apt-get update
sudo apt-get install $SOFTWARE_TO_INSTALL -y
cd /usr/local/pisound && git pull
