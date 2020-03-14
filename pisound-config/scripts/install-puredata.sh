#!/bin/sh

SOFTWARE_TO_INSTALL="puredata"
apt-get update
apt-get install $SOFTWARE_TO_INSTALL --no-install-recommends -y
echo "Done! Thank you!"
