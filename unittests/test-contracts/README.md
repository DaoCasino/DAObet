# BUILD
 - Install EOSIO.CDT v1.6.1
 - ```EOSIO_CDT_CMAKE=<PATH_TO_EOSIO_CDT_CMAKE_MODULE_DIR> ./build.sh```

`EOSIO_CDT_CMAKE` is path to `eosio.cdt` cmake module on your host, usually located at `/usr/lib/cmake/eosio.cdt` after install EOSIO.CDT from package manager.
If you don't specify `EOSIO_CDT_CMAKE` then script will use `/usr/lib/cmake/eosio.cdt` path. If your cmake module located at another dir than `/usr/lib/cmake/eosio.cdt` you have to explicitly set path to script.

test_ram_limit contract was compiled with eosio.cdt v1.4.1

That contract was ported to compile with eosio.cdt v1.5.0, but the test that uses it is very sensitive to stdlib/eosiolib changes, compilation flags and linker flags.

deferred_test, get_sender_test, proxy, reject_all, and test_api contracts were compiled with eosio.cdt v1.6.1

The remaining contracts have been ported to compile with eosio.cdt v1.6.x. They were compiled with a patched version of eosio.cdt v1.6.0-rc1 (commit 1c9180ff5a1e431385180ce459e11e6a1255c1a4).
