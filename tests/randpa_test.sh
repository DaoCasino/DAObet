#!/bin/bash

set -eu

PROGNAME="$( basename -- "$0" )"
PROGPATH="$( readlink -f "$( dirname "$0" )" )"

PRODUCT_NAME=daobet

usage() {
  echo >&2 "Usage example for topology 11--12--13, where 11 is a block producer, 12 and 13"
  echo >&2 "are full nodes:"
  echo >&2
  echo >&2 "  $PROGNAME bp 11 12"
  echo >&2 "  $PROGNAME bp 12 '11 13'"
  echo >&2 "  $PROGNAME bp 13 12"
  echo >&2
  echo >&2 "Used TCP ports:"
  echo >&2 "  80xx"
  echo >&2 "  90xx"
  echo >&2 "(for each 'xx' among node indexes)."
}

die() {
  echo >&2 "$1"
  exit 1
}

die_with_usage() {
  echo >&2 "$1"
  echo
  usage
  exit 1
}

###

bp="${1:-}"               # "bp", or anything else
node_index="${2:-}"       # "11"
connected_nodes="${3:-}"  # "12 13"

[[ -n "$bp" ]] || \
  die_with_usage "1st argument should be 'bp' for block producer, or any non-empty string for full node."

[[ "$node_index" =~ ^[1-9][0-9]$ ]] || \
  die_with_usage "2nd argument should be a valid node index -- two-digit number"

[[ -n "$connected_nodes" ]] || \
  die_with_usage "3rd argument should be a non-empty space-separated list of connected node indexes"

###

root_dir="$HOME"/test-randpa.tmp
data_dir="$root_dir/data-$node_index"

#rm -rf "$root_dir" "$HOME"/.local/share/eosio/
mkdir -p "$root_dir" "$data_dir"

peers="$(echo "$connected_nodes" | tr ' ' '\n' | sed 's,^,--p2p-peer-address localhost:90,' | tr '\n' ' ')"

bp_1_sp="EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV=KEY:5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"
declare -A full_node_sp
full_node_sp[11]="EOS6fw9SRYdpPZLBPS4XD4vYfVXRzWNczrGekCtLnBzSMB7Ctc18w=KEY:5Ja7pgQcm4z8BYyJ8dhpRsfLZCAVa42WBG8SQJYHBRcwmx2B588"
full_node_sp[12]="EOS7ggDNF9XYftsXKJqAo5G5Y5BBrPPhpu4T5wGGknyMKgTzuX97w=KEY:5Hw51hm6ug9XZhpsVmxLnfWVHLKiMsanzNFYaYXk4rjzw2dHUHs"
full_node_sp[13]="EOS7rDCnQARkmZitasv6JNgyRtEZ5BGBGWsa2gkLnHN5SuDYR6Cya=KEY:5Jn1mhrr3LcfpE7UQG7WJtruGdEJx7kS1ZEiQUmgPP3Dq21oLXY"
# ... TODO: add more keys or generate them on fly

bp_or_full=
prod_name=
signature_provider="--signature-provider ${full_node_sp["$node_index"]}"
if [[ "$bp" == bp ]]; then
  bp_or_full=--enable-stale-production
  #prod_name="--producer-name producer$node_index" // FIXME!!!
  prod_name="--producer-name eosio"
  signature_provider="--signature-provider $bp_1_sp"
fi

###

set -x

"$PROGPATH/../build/bin/$PRODUCT_NAME-node" \
  --blocks-dir blocks \
  --chain-state-db-size-mb 1024 \
  --reversible-blocks-db-size-mb 100 \
  --contracts-console \
  --data-dir "$data_dir" \
  --delete-all-blocks \
  --keosd-provider-timeout 100000000 \
  --max-clients 10 \
  --max-irreversible-block-age -1 \
  --max-transaction-time 1000   \
  $bp_or_full \
  --p2p-max-nodes-per-host 10 \
  --http-server-address "127.0.0.1:80$node_index" \
  --p2p-listen-endpoint "127.0.0.1:90$node_index" \
  $peers \
  $prod_name \
  $signature_provider \
  --plugin eosio::chain_api_plugin \
  --plugin eosio::net_api_plugin \
  --plugin eosio::producer_api_plugin \
  --plugin eosio::randpa_plugin \
  -l "$root_dir"/logging.json

#  --genesis-json "$root_dir"/genesis.json \
