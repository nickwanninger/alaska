#!/bin/bash


NPAGES=${1:-256}
echo "Configuring the system to have $NPAGES huge pages" 
# Configure a few hugepages
sudo sysctl -w vm.nr_hugepages=$NPAGES
# Tell the kernel to always use them if it's asked to
echo always | sudo tee /sys/kernel/mm/transparent_hugepage/enabled > /dev/null

