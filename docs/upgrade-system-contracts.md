# Guide to upgrade DAOBet system contracts

Each active block producer should do the following:

1. Get current system contract for later comparison (actual hash and ABI on the mainnet blockchain will be different):
   ```
   $ daobet-cli get code \
     -c original_system_contract.wast -a original_system_contract.abi eosio
   code hash: cc0ffc30150a07c487d8247a484ce1caf9c95779521d8c230040c2cb0e2a3a60
   saving wast to original_system_contract.wast
   saving abi to original_system_contract.abi
   ```
2. Generate the unsigned transaction which upgrades the system contract:
   ```
   $ daobet-cli set contract -s -j -d eosio contracts/eosio.system \
     > upgrade_system_contract_trx.json
   ```

   The first few lines of the generated file should be something similar to
   ```
   {
     "expiration": "2020-06-23T23:23:11",
     "ref_block_num": 28283,
     "ref_block_prefix": 187419137,
     "max_net_usage_words": 0,
     "max_cpu_usage_ms": 0,
     "delay_sec": 0,
     "context_free_actions": [],
     "actions": [{
         "account": "eosio",
         "name": "setcode",
         "authorization": [{
             "actor": "eosio",
             "permission": "active"
           }
         ],
   ```
   and the last few lines should be
   ```
       }
     ],
     "transaction_extensions": [],
     "signatures": [],
     "context_free_data": []
   }
   ```

One of the top block producers should be chosen to lead the upgrade process. This lead producer should take their
generated `upgrade_system_contract_trx.json`, rename it to `upgrade_system_contract_official_trx.json`, and do the
following:

3. Modify the expiration timestamp in `upgrade_system_contract_official_trx.json` to a time that is sufficiently far in
   the future to give enough time to collect all the necessary signatures, but not more than 9 hours from the time the
   transaction was generated. Also, keep in mind that the transaction will not be accepted into the blockchain if the
   expiration is more than 1 hour from the present time.
4. Pass the `upgrade_system_contract_official_trx.json` file to all the other top block producers.

Then each of the top block producers should do the following:

5. Compare their generated `upgrade_system_contract_official_trx.json` file with the
   `upgrade_system_contract_official_trx.json` provided by the lead producer. The only difference should be in
   expiration, `ref_block_num`, `ref_block_prefix`, for example
   ```
   $ diff upgrade_system_contract_official_trx.json upgrade_system_contract_trx.json
   2,4c2,4
   <   "expiration": "2018-06-15T22:17:10",
   <   "ref_block_num": 4552,
   <   "ref_block_prefix": 511016679,
   ---
   >   "expiration": "2018-06-15T21:20:39",
   >   "ref_block_num": 4972,
   >   "ref_block_prefix": 195390844,
   ```
6. If the comparison is good, each block producer should proceed with signing the official upgrade transaction with the
   keys necessary to satisfy their active permission. If the block producer only has a single key (i.e the "active key")
   in the active permission of their block producing account, then they only need to generate one signature using that
   active key. This signing process can be done offline for extra security.

   First, the block producer should collect all the necessary information. Let us assume that the block producers active
   key pair is `(EOS5kBmh5kfo6c6pwB8j77vrznoAaygzoYvBsgLyMMmQ9B6j83i9c,
   5JjpkhxAmEfynDgSn7gmEKEVcBqJTtu6HiQFf4AVgGv5A89LfG3)`. The block producer needs their active private key
   (`5JjpkhxAmEfynDgSn7gmEKEVcBqJTtu6HiQFf4AVgGv5A89LfG3` in this example), the
   `upgrade_system_contract_official_trx.json`, and the `chain_id`
   (`d0242fb30b71b82df9966d10ff6d09e4f5eb6be7ba85fd78f796937f1959315e` in this example) which can be
   retrieved through `daobet-cli get info`.

   Then on a secure computer the producer can sign the transaction (the producer will need to paste in their private key
   when prompted):
   ```
   $ daobet-cli sign --chain-id d0242fb30b71b82df9966d10ff6d09e4f5eb6be7ba85fd78f796937f1959315e
   upgrade_system_contract_trx.json | tail -n5
     private key:   "signatures": [
       "SIG_K1_JzABB9gzDGwUHaRmox68UNcfxMVwMnEXqqS1MvtsyUX8KGTbsZ5aZQZjHD5vREQa5BkZ7ft8CceLBLAj8eZ5erZb9cHuy5"
     ],
     "context_free_data": []
   }
   ```

   Make sure to use the `chain_id` of the actual mainnet blockchain that the transaction will be submitted to and not
   the example `chain_id` provided above.

   The output should include the signature (in this case
   `"SIG_K1_JzABB9gzDGwUHaRmox68UNcfxMVwMnEXqqS1MvtsyUX8KGTbsZ5aZQZjHD5vREQa5BkZ7ft8CceLBLAj8eZ5erZb9cHuy5"`) which the
   producer should then send to the lead producer.

   When the lead producer collects (2/3\*`current_schedule_size` + 1) producer signatures, the lead producer should do
   the following:

7. Make a copy of the `upgrade_system_contract_official_trx.json` and call it
   `upgrade_system_contract_official_trx_signed.json`, and then modify the
   `upgrade_system_contract_official_trx_signed.json` so that the signatures field includes all
   (2/3\*`current_schedule_size` + 1) collected signatures. So the tail end of
   `upgrade_system_contract_official_trx_signed.json` could look something like:
   ```
   $ tail -n34 upgrade_system_contract_official_trx_signed.json
     "transaction_extensions": [],
     "signatures": [
      "SIG_K1_JzABB9gzDGwUHaRmox68UNcfxMVwMnEXqqS1MvtsyUX8KGTbsZ5aZQZjHD5vREQa5BkZ7ft8CceLBLAj8eZ5erZb9cHuy5",
      "SIG_K1_Kj7XJxnPQSxEXZhMA8uK3Q1zAxp7AExzsRd7Xaa7ywcE4iUrhbVA3B6GWre5Ctgikb4q4CeU6Bvv5qmh9uJjqKEbbjd3sX",
      "SIG_K1_KbE7qyz3A9LoQPYWzo4e6kg5ZVojQVAkDKuufUN2EwVUqtFhtjmGoC6QPQqLi8J7ftiysBp52wJBPjtNQUfZiGpGMsnZ1f",
      ...
      "SIG_K1_KjJB8jtcqpVe3r5jouFiAa9wJeYqoLMh5xrUV6kBF6UWfbYjimMWBJWz2ZPomGDsk7JtdUESVrYj1AhYbdp3X48KLm5Cev"
     ],
     "context_free_data": []
   }
   ```

8. Push the signed transaction to the blockchain:
   ```
   $ daobet-cli push transaction --skip-sign upgrade_system_contract_official_trx_signed.json
   {
     "transaction_id": "202888b32e7a0f9de1b8483befac8118188c786380f6e62ced445f93fb2b1041",
     "processed": {
       "id": "202888b32e7a0f9de1b8483befac8118188c786380f6e62ced445f93fb2b1041",
       "receipt": {
         "status": "executed",
         "cpu_usage_us": 4909,
         "net_usage_words": 15124
       },
       "elapsed": 4909,
       "net_usage": 120992,
       "scheduled": false,
       "action_traces": [{
   ...
   ```

   If you get an error message like the following:
   ```
   Error 3090003: provided keys, permissions, and delays do not satisfy declared authorizations
   Ensure that you have the related private keys inside your wallet and your wallet is unlocked.
   ```
   That means that at least one of the signatures provided were bad. This may be because a producer signed the wrong
   transaction, used the wrong private key, or used the wrong chain ID.

   If you get an error message like the following:
   ```
   Error 3090002: irrelevant signature included
   Please remove the unnecessary signature from your transaction!
   ```
   That means unnecessary signatures were included. If there are 42 active producers, only signatures from exactly 29 of
   those 42 active producers are needed.

   If you get an error message like the following:
   ```
   Error 3040006: Transaction Expiration Too Far
   Please decrease the expiration time of your transaction!
   ```
   That means that the expiration time is more than 1 hour in the future, and you need to wait some time before being
   allowed to push the transaction.

   If you get an error message like the following:
   ```
   Error 3040005: Expired Transaction
   Please increase the expiration time of your transaction!
   ```
   That means the expiration time of the signed transaction has passed, and this entire process has to restart from step
   1.

9. Assuming the transaction successfully executes, everyone can then verify that the new contract is in place:
   ```
   $ daobet-cli get code -c new_system_contract.wast -a new_system_contract.abi eosio
   code hash: 9fd195bc5a26d3cd82ae76b70bb71d8ce83dcfeb0e5e27e4e740998fdb7b98f8
   saving wast to new_system_contract.wast
   saving abi to new_system_contract.abi
   $ diff original_system_contract.abi new_system_contract.abi
   584,592d583
   <         },{
   <           "name": "deferred_trx_id",
   <           "type": "uint32"
   <         },{
   <           "name": "last_unstake_time",
   <           "type": "time_point_sec"
   <         },{
   <           "name": "unstaking",
   <           "type": "asset"
   ```
