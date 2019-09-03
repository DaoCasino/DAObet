NODE_PATH=$HOME/.dao-node
NODE_IMAGE='daocasino/blockchain:v0.2.1'
CONFIG_URL='https://explorer.dao.casino/config'


mkdir -p $NODE_PATH/config
mkdir -p $NODE_PATH/state


curl -sk $CONFIG_URL/config.ini -o $NODE_PATH/config/config.ini
curl -sk $CONFIG_URL/genesis.json -o $NODE_PATH/config/genesis.json
curl -sk $CONFIG_URL/logger.json -o $NODE_PATH/config/logger.json


docker run -d --cap-add IPC_LOCK \
    --env EOSIO_ROOT=/opt/haya \
    --env LD_LIBRARY_PATH=/usr/local/lib \
    --env PATH=/opt/haya/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    --name dao-node \
    --network host \
    --volume $NODE_PATH:/opt/dao:rw \
    $NODE_IMAGE \
    /opt/haya/bin/haya-node \
    --config-dir=/opt/dao/config \
    --genesis-json=/opt/dao/config/genesis.json \
    --logconf=/opt/dao/config/logger.json \
    -d /opt/dao/state

docker run -d --network host \
    --volume $NODE_PATH:/opt/dao:rw \
    --name dao-wallet $NODE_IMAGE \
    /opt/haya/bin/haya-wallet \
    --http-server-address 127.0.0.1:8899
