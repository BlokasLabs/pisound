#!/bin/sh

SOFTWARE_TO_INSTALL="pisound pisound-btn pisound-ctl"
apt-get update
apt-get install $SOFTWARE_TO_INSTALL -y
cd /usr/local/pisound && git pull
