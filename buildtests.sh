#!/bin/bash

scons -c

echo "Debug Build"
scons BUILDTYPE=DEBUG
scons -c

echo "Debug Build w/o FUSE"
scons WITH_FUSE=0 BUILDTYPE=DEBUG
scons -c

echo "Debug Build w/o HTTPD"
scons WITH_HTTPD=0 BUILDTYPE=DEBUG
scons -c

echo "Debug Build w/o mDNS"
scons WITH_MDNS=0 BUILDTYPE=DEBUG
scons -c

echo "Release Build"
scons BUILDTYPE=RELEASE
scons -c

echo "Release Build w/o FUSE"
scons WITH_FUSE=0 BUILDTYPE=RELEASE
scons -c

echo "Release Build w/o HTTPD"
scons WITH_HTTPD=0 BUILDTYPE=RELEASE
scons -c

echo "Release Build w/o mDNS"
scons WITH_MDNS=0 BUILDTYPE=RELEASE
scons -c

