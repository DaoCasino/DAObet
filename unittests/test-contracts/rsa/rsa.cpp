#include <rsa.hpp>

void rsa::verify(const eosio::checksum256& digest,
                 const std::string& sig,
                 const std::string& pubkey)
{
   eosio::check( daobet::rsa_verify(digest, sig, pubkey), "invalid signature" );
}
