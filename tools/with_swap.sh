#!/usr/bin/env bash

group="alaska_$(whoami)"

# export DBUS_SESSION_BUS_ADDRESS=$(dbus-launch --sh-syntax --exit-with-session)
# export XDG_RUNTIME_DIR=$(eval echo ~$USER)/.runtime-dir

sudo cgcreate -g memory:/${group}
sudo chown -R $(whoami):$(whoami) /sys/fs/cgroup/${group}
sudo cgset -r memory.max=128M ${group}
sudo -E cgexec -g memory:${group} $@
