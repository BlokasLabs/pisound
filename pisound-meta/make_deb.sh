#!/bin/sh

#gzip --best -n ./debian/usr/share/doc/pisound-ctl/changelog ./debian/usr/share/doc/pisound-ctl/changelog.Debian ./debian/usr/share/man/man1/pisound-ctl.1
fakeroot dpkg --build debian
mv debian.deb pisound.deb
#gunzip `find . | grep gz`
