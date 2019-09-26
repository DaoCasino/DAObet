#! /bin/bash

OPT_LOCATION=$HOME/opt

binaries=(
   cleos
   eosio-abigen
   eosio-launcher
   eosio-s2wasm
   eosio-wast2wasm
   eosiocpp
   eosio-applesdemo
   daobet-wallet
   daobet-node
)

if [ -d $OPT_LOCATION/daobet ]; then
   printf "Do you wish to remove this install? (requires sudo)\n"
   select yn in "Yes" "No"; do
      case $yn in
         [Yy]* )
            if [ "$(id -u)" -ne 0 ]; then
               printf "\nThis requires sudo, please run ./daobet_uninstall.sh with sudo\n\n"
               exit -1
            fi

            pushd $HOME &> /dev/null
            pushd opt &> /dev/null
            rm -rf daobet
            # Handle cleanup of directories created from installation
            if [ "$1" == "--full" ]; then
               if [ -d ~/Library/Application\ Support/daobet ]; then rm -rf ~/Library/Application\ Support/daobet; fi # Mac OS
               if [ -d ~/.local/share/daobet ]; then rm -rf ~/.local/share/daobet; fi # Linux
            fi
            popd &> /dev/null
            pushd bin &> /dev/null
            for binary in ${binaries[@]}; do
               rm ${binary}
            done
            popd &> /dev/null
            pushd lib/cmake &> /dev/null
            rm -rf daobet
            popd &> /dev/null

            break;;
         [Nn]* )
            printf "Aborting uninstall\n\n"
            exit -1;;
      esac
   done
fi

if [ -d "/usr/local/daobet" ]; then
   printf "Do you wish to remove this install? (requires sudo)\n"
   select yn in "Yes" "No"; do
      case $yn in
         [Yy]* )
            if [ "$(id -u)" -ne 0 ]; then
               printf "\nThis requires sudo, please run ./daobet_uninstall.sh with sudo\n\n"
               exit -1
            fi

            pushd /usr/local &> /dev/null
            pushd opt &> /dev/null
            rm -rf daobet
            # Handle cleanup of directories created from installation
            if [ "$1" == "--full" ]; then
               if [ -d ~/Library/Application\ Support/daobet ]; then rm -rf ~/Library/Application\ Support/daobet; fi # Mac OS
               if [ -d ~/.local/share/daobet ]; then rm -rf ~/.local/share/daobet; fi # Linux
            fi
            popd &> /dev/null
            pushd bin &> /dev/null
            for binary in ${binaries[@]}; do
               rm ${binary}
            done
            popd &> /dev/null
            pushd lib/cmake &> /dev/null
            rm -rf daobet
            popd &> /dev/null

            break;;
         [Nn]* )
            printf "Aborting uninstall\n\n"
            exit -1;;
      esac
   done
fi
