#!/bin/sh

SOFTWARE_TO_INSTALL="libpython2.7-dev liblo-dev libasound2-dev libjack-jackd2-dev python-pip"
sudo apt-get update
sudo apt-get install $SOFTWARE_TO_INSTALL -y
sudo easy_install --upgrade pip
sudo pip install pgen
sudo pip install Cython --install-option="--no-cython-compile"
sudo pip install netifaces
sudo pip install enum-compat
sudo pip install touchosc2midi
echo "Done! Thank you!"
