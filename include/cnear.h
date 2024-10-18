/***************************************************************************
 *
 *  Project                    c |\| E A R
 *
 * Copyright (C) 2024, Damian Parrino, <https://github.com/bucanero>.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the LICENSE file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/
#include <stdlib.h>
#include <stdbool.h>


#define NEAR_TESTNET_RPC_SERVER_URL     "https://rpc.testnet.near.org"
#define NEAR_MAINNET_RPC_SERVER_URL     "https://rpc.mainnet.near.org"
#define NEAR_DEFAULT_100_TGAS           100000000000000

  /// TxExecutionStatus
  /// Transaction is waiting to be included into the block
#define NEAR_TX_STATUS_NONE             "NONE"
  /// Transaction is included into the block. The block may be not finalized yet
#define NEAR_TX_STATUS_INCLUDED         "INCLUDED"
  /// Transaction is included into the block +
  /// All non-refund transaction receipts finished their execution.
  /// The corresponding blocks for tx and each receipt may be not finalized yet
  /// [default]
#define NEAR_TX_STATUS_EXEC_OPTIMISTIC  "EXECUTED_OPTIMISTIC"
  /// Transaction is included into finalized block
#define NEAR_TX_STATUS_INCLUDED_FINAL   "INCLUDED_FINAL"
  /// Transaction is included into finalized block +
  /// All non-refund transaction receipts finished their execution.
  /// The corresponding blocks for each receipt may be not finalized yet
#define NEAR_TX_STATUS_EXECUTED         "EXECUTED"
  /// Transaction is included into finalized block +
  /// Execution of all transaction receipts is finalized, including refund receipts
#define NEAR_TX_STATUS_FINAL            "FINAL"


typedef struct 
{
    int rpc_code;
    char* json;
} cnearResponse;

typedef struct FunctionCallAction
{
  char* method_name;
  char* args;
  uint64_t gas;
  uint64_t deposit; // note: should be u128
} nearAction;

typedef struct Transaction
{
  // An account on which behalf transaction is signed
  char* signer_id;
  // A public key of the access key which was used to sign an account.
  // Access key holds permissions for calling certain kinds of actions.
  uint8_t key_type;
  uint8_t public_key[32];
  // Nonce is used to determine order of transaction in the pool.
  // It increments for a combination of `signer_id` and `public_key`
  uint64_t nonce;
  // Receiver account for this transaction
  char* receiver_id;
  // The hash of the block in the blockchain on top of which the given
  // transaction is valid
  uint8_t block_hash[32];
  // A list of actions to be applied
  nearAction* actions;
} nearTransaction;

enum nearKeyType
{
    nearKeyTypeED25519 = 0,
    nearKeyTypeSecp256k1 = 1,
    nearKeyTypeKeyTypeNotSet = 255
};

/***************************************************************************
 * RPC API
 ***************************************************************************/

bool near_rpc_init(const char* rpc_url);
void near_rpc_cleanup(void);

bool near_account_init(const char* account, const char* b58_priv, const char* b58_pub);
bool near_account_init_json(const char* credentials_json);

cnearResponse near_rpc_view_account(const char* account);
cnearResponse near_rpc_view_access_key(const char* account, const char* pub_key);
cnearResponse near_rpc_view_state(const char* account, const char* prefix);
cnearResponse near_rpc_call_function(const char* account, const char* method, const char* args);
cnearResponse near_rpc_send_tx(nearTransaction* near_tx, const char* status);

uint8_t* near_decode_result(const cnearResponse* response, size_t* out_size);
