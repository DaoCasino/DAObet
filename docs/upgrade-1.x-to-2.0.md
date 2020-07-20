# Guide to upgrade daobet node from 1.x to 2.0

### !! Please, read this guide carefully before performing any actions.

#### In order to upgrade DAOBet blockchain we should take following general steps:
1) Validators deploy DAOBet 2.0.1 non-producing (seed) node and configure feature activation time as coordinated with other Validators.
2) Wait for syncing from the network or run with the snapshot from eossweden's storage: [mainnet](https://snapshots.daobet.eossweden.org/)/[testnet](https://snapshots.daobet-test.eossweden.org/).
3) Ensure that the node is fully synced and recieves new blocks.
4) Turn off 1.x production node and switch DAOBet 2.0.1 node to production mode at least for a couple of days before scheduled feature activation date and closely monitor block production and finality.
5) After the scheduled feature activation date DAOBet team will trigger features activation and the blockchain will be successfuly upgraded.

#### Please see an Example of an upgrade process below:

Here, `data_dir` variable is a path to you `data` directory (contains `blocks`, `state`, and other files and
directories).

* Stop running node (binary or container):
  ```
  kill daobet-node
  # OR
  docker stop daobet-producer
  ```

* Backup `blocks/reversible`, `state-history` and `state` directories located in node data directory:
  ```
  ( cd "$data_dir" && rsync -avR --delete blocks/reversible state-history state ~/backup.daobet-1.x )
  ```

* If you used default values for `data-dir` and/or `config-dir`, you may need to rename `nodeos` directory to the
  `daobet-node`.

* Delete the following directories within the node data directory: `blocks/reversible`, `state-history`, and `state` (be
  sure they were backed up earlier):
  ```
  rm -rf /path/to/daobet-node/data/{blocks/reversible,state-history,state}/
  ```

* Create file `protocol_features/BUILTIN-PREACTIVATE_FEATURE.json` in your config directory with the following contents:
  ```
  {
    "protocol_feature_type": "builtin",
    "dependencies": [],
    "description_digest": "64fe7df32e9b86be2b296b3f81dfd527f84e82b98e363bc97e40bc7a83733310",
    "subjective_restrictions": {
      "earliest_allowed_activation_time": "2020-06-29T13:00:00.000",
      "preactivation_required": false,
      "enabled": true
    },
    "builtin_feature_codename": "PREACTIVATE_FEATURE"
  }
  ```

  In the above JSON-file, `earliest_allowed_activation_time` is set to the earliest time that block producers agreed to
  activate the first protocol feature.

  It is important that this configuration change happens prior to allowing that node to produce blocks on the network.
  As long as more than two-thirds of the active block producers have set the same future time in the configuration file
  for the `PREACTIVATE_FEATURE` on their BP nodes, the network will be safe from any attempts at premature activation by
  some other active BPs.

* Update the daobet software installation either by your system package manager, or by pulling a docker image:
  ```
  dpkg -i daobet_2.0.1-1_amd64.deb
  # OR
  docker pull daocasino/daobet:v2.0.1
  ```

* Don't forget to enable block production either by setting `pause-on-startup` config option to `false` (before node
  run), or by performing a HTTP GET-request to http://localhost:8888/v1/producer/resume (after node run).

* Run node (verson 2.0.x).
