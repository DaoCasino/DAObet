#!/bin/sh
cd /opt/daobet/bin

if [ ! -d "/opt/daobet/bin/data-dir" ]; then
    mkdir /opt/daobet/bin/data-dir
fi

if [ -f '/opt/daobet/bin/data-dir/config.ini' ]; then
    echo
  else
    cp /config.ini /opt/daobet/bin/data-dir
fi

if [ -d '/opt/daobet/bin/data-dir/contracts' ]; then
    echo
  else
    cp -r /contracts /opt/daobet/bin/data-dir
fi

while :; do
    case $1 in
        --config-dir=?*)
            CONFIG_DIR=${1#*=}
            ;;
        *)
            break
    esac
    shift
done

if [ ! "$CONFIG_DIR" ]; then
    CONFIG_DIR="--config-dir=/opt/daobet/bin/data-dir"
else
    CONFIG_DIR=""
fi

exec /opt/daobet/bin/daobet-node $CONFIG_DIR "$@"
