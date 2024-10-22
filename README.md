# cNEAR

cNEAR is a C library for interacting with the NEAR Protocol blockchain. It provides a simple interface to interact with NEAR's JSON-RPC API, allowing you to query account information, contract state, and call contract methods.

## Requirements

- [libcurl](https://curl.se/libcurl/)

## Building

To build the library run:

```sh
make
```

## Tests

Build and run the [test application](https://github.com/bucanero/cnear-poc/blob/main/tests/main.c):

```sh
make tests

./cnear-test
```

# Library Functions

## General

- [`near_rpc_init`](#initialization) - Initialize the RPC client.
- [`near_rpc_cleanup`](#cleanup) - Cleanup the RPC client.

### Initialization

Initialize the RPC client with the NEAR RPC URL. Can also initialize the libcurl library.

```c
bool near_rpc_init(const char* rpc_url, bool curl_init);
```

### Cleanup

Cleanup the RPC client. Can also cleanup the libcurl library.

```c
void near_rpc_cleanup(bool curl_cleanup);
```

## Signer Account

- [`near_account_init`](#set-account-keys) - Set account keys.
- [`near_account_init_json`](#set-account-keys-from-json) - Set account keys from JSON.

### Set Account keys

Initialize the signer account with the account ID, private key, and public key.

```c
bool near_account_init(const char* account, const char* b58_priv, const char* b58_pub);

near_account_init("ACCOUNT.testnet", "ed25519:FYwA...", "ed25519:gU3a...");
```

### Set Account keys from JSON

Initialize the signer account (account ID, private key, and public key) from a JSON string.

```c
bool near_account_init_json(const char* credentials_json);

near_account_init_json("{\"account_id\":\"dev...\",\"public_k...}");
```

## RPC

- [`near_rpc_view_account`](#rpc-view-account) - View account information.
- [`near_rpc_view_access_key`](#rpc-view-access-key) - View access key information.
- [`near_rpc_view_state`](#rpc-view-contract-state) - View contract state.
- [`near_rpc_call_function`](#rpc-call-contract-method-read-only) - Call contract method (read-only).
- [`near_rpc_send_tx`](#rpc-send-transaction) - Send transaction.
- [`near_contract_call`](#call-contract-method-change-state) - Call contract method (change state).

### RPC View Account

Returns basic account information.
[Reference](https://docs.near.org/api/rpc/contracts#view-account)

```c
cnearResponse near_rpc_view_account(const char* account);

// View basic account info for account 'demo-devhub-vid100.testnet'
result = near_rpc_view_account("demo-devhub-vid100.testnet");
```

### RPC View Access Key

Returns information about a single access key for given account.
[Reference](https://docs.near.org/api/rpc/access-keys#view-access-key)

```c
cnearResponse near_rpc_view_access_key(const char* account, const char* pub_key);
```

### RPC View Contract State

Returns the state (key value pairs) of a contract based on the key prefix. Please note that the returned state will be base64 encoded.
[Reference](https://docs.near.org/api/rpc/contracts#view-contract-state)

```c
cnearResponse near_rpc_view_state(const char* account, const char* prefix);

// View contract state for contract 'guest-book.testnet'
// and filter key prefix 'm::999'
result = near_rpc_view_state("guest-book.testnet", "m::999");
```

### RPC Call Contract Method (Read only)

Allows you to call a contract method as a view function.
[Reference](https://docs.near.org/api/rpc/contracts#call-a-contract-function)

```c
cnearResponse near_rpc_call_function(const char* account, const char* method, const char* args);

// Call a contract function 'get_greeting' on contract 'demo-devhub-vid102.testnet'
// with empty JSON arguments
// Note: this is a view function (read-only), so it doesn't require gas
result = near_rpc_call_function("demo-devhub-vid102.testnet", "get_greeting", "{}");
```

### RPC Send Transaction

Sends a transaction. Returns the guaranteed execution status and the results the blockchain can provide at the moment.
[Reference](https://docs.near.org/api/rpc/transactions#send-tx)

```c
cnearResponse near_rpc_send_tx(nearTransaction* near_tx, const char* status);

// Send a transaction to the network
// Note: requires gas, and a signer account
result = near_rpc_send_tx(&test_tx, NEAR_TX_STATUS_EXEC_OPTIMISTIC);
```

### Call Contract Method (Change state)

Executes a contract method as a call function. Changing the contract's state requires a signer account to pay for gas.

```c
cnearResponse near_contract_call(const char* contract, const char* method, const char* args, uint64_t gas, uint64_t deposit);

// Call a contract function 'set_greeting' on contract 'demo-devhub-vid102.testnet'
// with JSON arguments '{"greeting":"Hello cNEAR!"}'
// Note: this is a change method that modifies the contract's state,
// so it's a signed transaction (require gas, and a signer account)
result = near_contract_call("demo-devhub-vid102.testnet", 
            "set_greeting", 
            "{\"greeting\":\"Hello cNEAR!\"}", 
            NEAR_DEFAULT_100_TGAS, 0);
```

## Misc

- [`near_decode_result`](#decode-result) - Decode the JSON result from a smart contract RPC call.

### Decode Result

Decodes the JSON `result` from a smart contract RPC call into a byte array.

```c
uint8_t* near_decode_result(const cnearResponse* response, size_t* out_size);
```

# Credits

* [Damian Parrino](https://twitter.com/dparrino): Project developer

# License

This library is released under the [MIT License](LICENSE).
