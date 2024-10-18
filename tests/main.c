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

#include "cnear.h"

#include <string.h>
#include <stdio.h>

#define TEST_ACCOUNT    "dev-1640093409715-22205231677544"
#define TEST_PUBKEY     "ed25519:FY835wAj7g8fRMncf4tqkyT3YdoW71t1ERnt3L78R28i"
#define TEST_PRVKEY     "ed25519:36gU64pAbTLH6uuxUeGvwF8n3fUfnDRSC87Z7Q5Ez2WdhcCy2KB6KtGX1WDcym6VezUhojWN4waBiwFAvxtXXNJN"
#define TEST_CRED_JSON  "{\"account_id\":\"dev-1640093409715-22205231677544\",\"public_key\":\"ed25519:FY835wAj7g8fRMncf4tqkyT3YdoW71t1ERnt3L78R28i\",\"private_key\":\"ed25519:36gU64pAbTLH6uuxUeGvwF8n3fUfnDRSC87Z7Q5Ez2WdhcCy2KB6KtGX1WDcym6VezUhojWN4waBiwFAvxtXXNJN\"}"


int main(int argc, const char* argv[])
{
    cnearResponse result;

    nearAction test_action = {
        .method_name = "set_greeting",
        .args = "{\"greeting\":\"Hello NEAR World\"}",
        .gas = NEAR_DEFAULT_100_TGAS,
        .deposit = 0
    };

    nearTransaction test_tx = {
        .signer_id = NULL,
        .nonce = 0,
        .receiver_id = "demo-devhub-vid102.testnet",
        .actions = &test_action
    };


    if (!near_rpc_init(NEAR_TESTNET_RPC_SERVER_URL))
    {
        printf("Error initializing RPC\n");
        return 1;
    }

//    near_account_init(TEST_ACCOUNT, TEST_PRVKEY, TEST_PUBKEY);
    if (!near_account_init_json(TEST_CRED_JSON))
    {
        printf("Error initializing account\n");
        return 1;
    }

	result = near_rpc_view_state("guest-book.testnet", "m::999");
    if(result.rpc_code == 200)
    {
        printf("Response (%d):\n%s\n", result.rpc_code, result.json);
    }
    free(result.json);

	result = near_rpc_view_account("demo-devhub-vid100.testnet");
    if(result.rpc_code == 200)
    {
        printf("Response (%d):\n%s\n", result.rpc_code, result.json);
    }
    free(result.json);

    result = near_rpc_send_tx(&test_tx, NEAR_TX_STATUS_EXEC_OPTIMISTIC);
    if(result.rpc_code == 200)
    {
        printf("Response (%d):\n%s\n", result.rpc_code, result.json);
    }
    free(result.json);

    result = near_rpc_call_function("demo-devhub-vid102.testnet", "get_greeting", "{}");
    if(result.rpc_code == 200)
    {
        size_t len;
        uint8_t* result_data = near_decode_result(&result, &len);

        printf("Response (%d):\n%s\n", result.rpc_code, result.json);
        printf("---\n%s\n---\n", result_data);
    }
    free(result.json);

    near_rpc_cleanup();

    return 0;
}
