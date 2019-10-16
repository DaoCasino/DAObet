NODE_PATH=$HOME/.dao-node
NODE_IMAGE='daocasino/blockchain:v0.3.0'
CONFIG_URL='https://explorer.daobet.org/config'

read -p "Node data dir: $NODE_PATH, do you want change it? (y/n) " resp
if [ $resp == "y" ]; then
    read -p "Enter new node data dir:"$'\n' resp
    NODE_PATH=$(realpath $resp)
    echo "New node data dir: $NODE_PATH"
fi

if [ -f $NODE_PATH/config/config.ini ]; then
    CONFIG_EXISTS=true
fi

if [ -d $NODE_PATH/state/blocks ]; then
    STATE_EXISTS=true
fi

if [ -d $NODE_PATH/wallet/ ]; then
    WALLET_EXISTS=true
fi

mkdir -p $NODE_PATH/config
mkdir -p $NODE_PATH/state
mkdir -p $NODE_PATH/wallet

function download-configs() {
    echo "Downloading configs..."
    curl -sk $CONFIG_URL/config.ini -o $NODE_PATH/config/config.ini
    curl -sk $CONFIG_URL/genesis.json -o $NODE_PATH/config/genesis.json
    curl -sk $CONFIG_URL/logger.json -o $NODE_PATH/config/logger.json
}

if [ $CONFIG_EXISTS ]; then
    read -p "Config file already exists, do you want rewrite it? (y/n) " resp
    if [ $resp == "y" ]; then
        download-configs
    fi
else
    download-configs
fi

if [ $STATE_EXISTS ]; then
    read -p "Blockchain state already exists, do you want delete it? (y/n) " resp
    if [ $resp == "y" ]; then
        echo "Deleting state..."
        sudo rm -rf $NODE_PATH/state/*
    fi
fi

if [ $WALLET_EXISTS ]; then
    read -p "Wallet file already exists, do you want delete it? (y/n) " resp
    if [ $resp == "y" ]; then
        echo "Deleting wallet files..."
        sudo rm -rf $NODE_PATH/wallet/*
    fi
fi


DAO_NODE_EXISTS=$(docker container ls -qf name=dao-node)
DAO_WALLET_EXISTS=$(docker container ls -qf name=dao-wallet)

function run-dao-node() {
    echo "Running dao-node container..."
    docker run -d --cap-add IPC_LOCK \
    --env EOSIO_ROOT=/opt/daobet \
    --env LD_LIBRARY_PATH=/usr/local/lib \
    --env PATH=/opt/daobet/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
    --name dao-node \
    --network host \
    --volume $NODE_PATH:/opt/dao:rw \
    $NODE_IMAGE \
    /opt/daobet/bin/daobet-node \
    --config-dir=/opt/dao/config \
    --genesis-json=/opt/dao/config/genesis.json \
    --logconf=/opt/dao/config/logger.json \
    -d /opt/dao/state
}

function run-dao-wallet() {
    echo "Running dao-wallet container..."
    docker run -d --network host \
    --volume $NODE_PATH:/opt/dao:rw \
    --name dao-wallet $NODE_IMAGE \
    /opt/daobet/bin/daobet-wallet \
    --http-server-address=127.0.0.1:8899 \
    --http-alias=localhost:8899 \
    --unlock-timeout=99999999 \
    --wallet-dir=/opt/dao/wallet
}

if [ $DAO_NODE_EXISTS ]; then
    read -p "Dao node docker contrainer already exists, do you want delete it? (y/n) " resp
    if [ $resp == "y" ]; then
        echo "Deleting old dao-node container..."
        docker stop dao-node
        docker rm dao-node
        run-dao-node
    else
        read -p "Do you want restart dao node container? (y/n) " resp
        if [ $resp == "y" ]; then
            echo "Restarting dao-node container..."
            docker restart dao-node
        fi
    fi
else
    run-dao-node
fi



if [ $DAO_WALLET_EXISTS ]; then
    read -p "Dao wallet docker contrainer already exists, do you want delete it? (y/n) " resp
    if [ $resp == "y" ]; then
        echo "Deleting old dao-wallet container..."
        docker stop dao-wallet
        docker rm dao-wallet
        run-dao-wallet
    else
        read -p "Do you want restart dao wallet container? (y/n) " resp
        if [ $resp == "y" ]; then
            echo "Restarting dao-wallet container..."
            docker restart dao-wallet
        fi
    fi
else
    run-dao-wallet
fi
