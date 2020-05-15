#
# Product-specific variables that vary in EOS, Haya, and DAOBet.
#

# get product name from the root directory name
PRODUCT_NAME="$( basename "$( readlink -f "$( dirname "$0" )/.." )" )"
PRODUCT_NAME="${PRODUCT_NAME,,}" # to lower case

# official product name (including right case of letters)
# core symbol name
case "$PRODUCT_NAME" in
(*daobet*)
  PRODUCT_NAME_OFFICIAL="DAOBet"
  CORE_SYMBOL_NAME=BET
  ;;
(*haya*)
  PRODUCT_NAME_OFFICIAL="Haya"
  CORE_SYMBOL_NAME=SYS
  ;;
esac
