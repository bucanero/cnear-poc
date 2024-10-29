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
#include <string.h>
#include <curl/curl.h>
//#include <cjson/cJSON.h>

#include "cnear.h"
#include "cJSON.h"
#include "base64.h"
#include "sha2.h"
#include "ed25519.h"

//#define DEBUGLOG 1

#ifdef DEBUGLOG
#define LOG_DBG printf
static void LOG_BUF(const char* str, const uint8_t* tmp, size_t len)
{
    printf("%s\n", str);
    while(len--)
        printf("%02X%c", *tmp++, (len % 0x10) ? ' ' : '\n');

    printf("\n\n");
}
#else
#define LOG_DBG(...)
#define LOG_BUF(...)
#endif

typedef struct
{
    uint8_t *memory;
    size_t size;
} curl_memory_t;

static char* near_rpc_url = NULL;
static char* near_account_id = NULL;
static char* near_b58_pub_key = NULL;
static uint8_t near_public_key[32] = {0};
static uint8_t near_private_key[64] = {0};

static size_t curl_write_memory(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    curl_memory_t *mem = (curl_memory_t *)userp;

    void *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr)
    {
        /* out of memory! */
        LOG_DBG("not enough memory (realloc)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static bool b58decode_ed25519_key(uint8_t* bin_key, size_t klen, const char* b58_key)
{
    if (strncmp(b58_key, "ed25519:", 8) != 0)
        return 0;

    return (base58_decode(b58_key + 8, bin_key, klen) == klen);
}

static int serialize_enum(curl_memory_t* mem_buf, uint8_t val)
{
    return (curl_write_memory(&val, 1, 1, mem_buf) == 1);
}

static int serialize_u32(curl_memory_t* mem_buf, uint32_t val)
{
    uint8_t buf[4];

    buf[3] = (val >> 24) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[0] = val & 0xFF;

    return (curl_write_memory(buf, sizeof(buf), 1, mem_buf) == sizeof(buf));
}

static int serialize_u64(curl_memory_t* mem_buf, uint64_t val)
{
    uint8_t buf[8];

    buf[7] = (val >> 56) & 0xFF;
    buf[6] = (val >> 48) & 0xFF;
    buf[5] = (val >> 40) & 0xFF;
    buf[4] = (val >> 32) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[0] = val & 0xFF;

    return (curl_write_memory(buf, sizeof(buf), 1, mem_buf) == sizeof(buf));
}

static int serialize_bytes(curl_memory_t* mem_buf, const uint8_t* data, size_t len)
{
//    serialize_u32(mem_buf, len);
    return (curl_write_memory((void*) data, len, 1, mem_buf) == len);
}

static int serialize_string(curl_memory_t* mem_buf, const char* str)
{
    size_t len = strlen(str);

    return (serialize_u32(mem_buf, len) && serialize_bytes(mem_buf, (const uint8_t*) str, len));
}

static curl_memory_t serialize_transaction(nearTransaction* near_tx)
{
    curl_memory_t chunk = {0};

    chunk.memory = malloc(1);   /* will be grown as needed by the realloc above */
    chunk.size = 0;             /* no data at this point */

    // Serialize transaction
    serialize_string(&chunk, near_tx->signer_id);
    serialize_enum(&chunk, near_tx->key_type);
    serialize_bytes(&chunk, near_tx->public_key, sizeof(near_tx->public_key));
    serialize_u64(&chunk, near_tx->nonce);
    serialize_string(&chunk, near_tx->receiver_id);
    serialize_bytes(&chunk, near_tx->block_hash, sizeof(near_tx->block_hash));

    // Serialize actions
    serialize_u32(&chunk, 1); // number of actions
    serialize_enum(&chunk, 2); // action type  (2 - function call)

    // Serialize action
    serialize_string(&chunk, near_tx->actions->method_name);
    serialize_string(&chunk, near_tx->actions->args);
    serialize_u64(&chunk, near_tx->actions->gas);
    serialize_u64(&chunk, near_tx->actions->deposit);   // this should be u128
    serialize_u64(&chunk, 0);

    return chunk;
}

static void sign_transaction(curl_memory_t* tx_data)
{
    ed25519_signature tx_sig;
    uint8_t digest[SHA256_DIGEST_LENGTH];

    sha256_Raw(tx_data->memory, tx_data->size, digest);
//	LOG_BUF(digest, sizeof(digest));

    ed25519_sign(digest, sizeof(digest), near_private_key, near_public_key, tx_sig);
//	LOG_BUF(tx_sig, sizeof(ed25519_signature));

    // Add signature to transaction
    serialize_enum(tx_data, nearKeyTypeED25519); // signature type
    serialize_bytes(tx_data, tx_sig, sizeof(ed25519_signature));
}

static cJSON* build_rpc_json(const char* method)
{
    cJSON *base = cJSON_CreateObject();

    if (!base ||
        !cJSON_AddStringToObject(base, "jsonrpc", "2.0") ||
        !cJSON_AddStringToObject(base, "id", "dontcare") ||
        !cJSON_AddStringToObject(base, "method", method))
        goto end;

    return base;

end:
    cJSON_Delete(base);
    return NULL;
}

static cJSON* build_rpc_view_account(const char* account)
{
    cJSON *req = build_rpc_json("query");

    if (!req)
        return NULL;

    cJSON *params = cJSON_CreateObject();

    if (!params ||
        !cJSON_AddStringToObject(params, "request_type", "view_account") ||
        !cJSON_AddStringToObject(params, "finality", "final") ||
        !cJSON_AddStringToObject(params, "account_id", account))
        goto end;

    cJSON_AddItemToObject(req, "params", params);
    return req;

end:
    cJSON_Delete(params);
    cJSON_Delete(req);
    return NULL;
}

static cJSON* build_rpc_view_state(const char* account, const char* b64_prefix)
{
    cJSON *req = build_rpc_json("query");

    if (!req)
        return NULL;

    cJSON *params = cJSON_CreateObject();

    if (!params ||
        !cJSON_AddStringToObject(params, "request_type", "view_state") ||
        !cJSON_AddStringToObject(params, "finality", "final") ||
        !cJSON_AddStringToObject(params, "account_id", account) ||
        !cJSON_AddStringToObject(params, "prefix_base64", b64_prefix))
        goto end;

    cJSON_AddItemToObject(req, "params", params);
    return req;

end:
    cJSON_Delete(params);
    cJSON_Delete(req);
    return NULL;
}

static cJSON* build_rpc_call_function(const char* account, const char* method, const char* b64args)
{
    cJSON *req = build_rpc_json("query");

    if (!req)
        return NULL;

    cJSON *params = cJSON_CreateObject();

    if (!params ||
        !cJSON_AddStringToObject(params, "request_type", "call_function") ||
        !cJSON_AddStringToObject(params, "finality", "final") ||
        !cJSON_AddStringToObject(params, "account_id", account) ||
        !cJSON_AddStringToObject(params, "method_name", method) ||
        !cJSON_AddStringToObject(params, "args_base64", b64args))
        goto end;

    cJSON_AddItemToObject(req, "params", params);
    return req;

end:
    cJSON_Delete(params);
    cJSON_Delete(req);
    return NULL;
}

static cJSON* build_rpc_view_access_key(const char* account, const char* pubkey)
{
    cJSON *req = build_rpc_json("query");

    if (!req)
        return NULL;

    cJSON *params = cJSON_CreateObject();

    if (!params ||
        !cJSON_AddStringToObject(params, "request_type", "view_access_key") ||
        !cJSON_AddStringToObject(params, "finality", "optimistic") ||
        !cJSON_AddStringToObject(params, "account_id", account) ||
        !cJSON_AddStringToObject(params, "public_key", pubkey))
        goto end;

    cJSON_AddItemToObject(req, "params", params);
    return req;

end:
    cJSON_Delete(params);
    cJSON_Delete(req);
    return NULL;
}

static cJSON* build_rpc_send_tx(const char* b64out, const char* status)
{
    cJSON *req = build_rpc_json("send_tx");

    if (!req)
        return NULL;

    cJSON *params = cJSON_CreateObject();

    if (!params ||
        !cJSON_AddStringToObject(params, "wait_until", status) ||
        !cJSON_AddStringToObject(params, "signed_tx_base64", b64out))
        goto end;

    cJSON_AddItemToObject(req, "params", params);
    return req;

end:
    cJSON_Delete(params);
    cJSON_Delete(req);
    return NULL;
}

static cJSON* exec_curl_rpc_call(const cJSON* data, int* ret_code)
{
    CURL *curl;
    CURLcode ret;
    cJSON *res = NULL;
    long response_code;
    char* json_out = NULL;
    curl_memory_t chunk = {0};
    struct curl_slist *headers = NULL;

    //LOG_DBG("----\n%s\n----\n", curl_version());
    json_out = cJSON_Print(data);
    LOG_DBG("++++\n%s\n++++\n", json_out);
    free(json_out);

    curl = curl_easy_init();
    if(!curl) {
        LOG_DBG("curl_easy_init() failed\n");
        return NULL;
    }

    chunk.memory = malloc(1);   /* will be grown as needed by the realloc above */
    chunk.size = 0;             /* no data at this point */

    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");

    json_out = cJSON_PrintUnformatted(data);
    curl_easy_setopt(curl, CURLOPT_URL, near_rpc_url);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_out);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "cNEAR Agent/libcurl");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
    
    // skip cert check
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,  0L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_memory);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    //    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_string);
    //    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    //    if(res == CURLE_HTTP_RETURNED_ERROR) {
        /* an HTTP response error problem */

    ret = curl_easy_perform(curl);
    if(ret != CURLE_OK)
    {
        LOG_DBG("curl_easy_perform() failed: %s\n", curl_easy_strerror(ret));
        *ret_code = ret;
        goto end;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    *ret_code = response_code;
    LOG_DBG("(%ld) %lu bytes retrieved\n",  response_code, (unsigned long)chunk.size);

    cJSON *json = cJSON_Parse((char*) chunk.memory);
    if (json)
    {
        res = cJSON_DetachItemFromObjectCaseSensitive(json, "result");
        if (!res)
            res = cJSON_DetachItemFromObjectCaseSensitive(json, "error");
        cJSON_Delete(json);
    }

end:
    free(json_out);
    free(chunk.memory);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return res;
}

/***************************************************************************
 * RPC API
 ***************************************************************************/

bool near_rpc_init(const char* rpc_url, bool curl_init)
{
    if (curl_init && curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
    {
        LOG_DBG("curl_global_init() failed\n");
        return false;
    }

    free(near_rpc_url);
    near_rpc_url = strdup(rpc_url);

    return true;
}

void near_rpc_cleanup(bool curl_cleanup)
{
    free(near_rpc_url);
    free(near_account_id);
    free(near_b58_pub_key);

    near_rpc_url = NULL;
    near_account_id = NULL;
    near_b58_pub_key = NULL;

    if (curl_cleanup)
        curl_global_cleanup();
}

cnearResponse near_rpc_view_account(const char* account)
{
    cnearResponse ret = {0};
    cJSON* rpc_res = NULL;
    cJSON* rpc_req = build_rpc_view_account(account);

    if(!rpc_req)
        return ret;

    rpc_res = exec_curl_rpc_call(rpc_req, &ret.rpc_code);
    cJSON_Delete(rpc_req);

    if (!rpc_res)
        return ret;

    ret.json = cJSON_Print(rpc_res);
    cJSON_Delete(rpc_res);

    LOG_DBG("----\n%s\n----\n", ret.json);
    return(ret);
}

cnearResponse near_rpc_view_state(const char* account, const char* prefix)
{
    size_t b64len;
    char* b64out = NULL;
    cnearResponse ret = {0};
    cJSON *rpc_req, *rpc_res = NULL;

    b64out = base64_encode((void*)prefix, strlen(prefix), &b64len);
    if (!b64out)
        return ret;

    rpc_req = build_rpc_view_state(account, b64out);
    free(b64out);

    if(!rpc_req)
        return ret;

    rpc_res = exec_curl_rpc_call(rpc_req, &ret.rpc_code);
    cJSON_Delete(rpc_req);

    if (!rpc_res)
        return ret;

    ret.json = cJSON_Print(rpc_res);
    cJSON_Delete(rpc_res);

    LOG_DBG("----\n%s\n----\n", ret.json);
    return(ret);
}

cnearResponse near_rpc_call_function(const char* account, const char* method, const char* args)
{
    size_t b64len;
    char* b64out = NULL;
    cnearResponse ret = {0};
    cJSON *rpc_req, *rpc_res = NULL;

    b64out = base64_encode((void*)args, strlen(args), &b64len);
    if (!b64out)
        return ret;

    rpc_req = build_rpc_call_function(account, method, b64out);
    free(b64out);

    if (!rpc_req)
        return ret;

    rpc_res = exec_curl_rpc_call(rpc_req, &ret.rpc_code);
    cJSON_Delete(rpc_req);

    if (!rpc_res)
        return ret;

    ret.json = cJSON_Print(rpc_res);
    cJSON_Delete(rpc_res);

    LOG_DBG("----\n%s\n----\n", ret.json);
    return ret;
}

cnearResponse near_rpc_view_access_key(const char* account, const char* pub_key)
{
    cnearResponse ret = {0};
    cJSON* rpc_res = NULL;
    cJSON* rpc_req = build_rpc_view_access_key(account, pub_key);

    if (!rpc_req)
        return ret;

    rpc_res = exec_curl_rpc_call(rpc_req, &ret.rpc_code);
    cJSON_Delete(rpc_req);

    if (!rpc_res)
        return ret;

    ret.json = cJSON_Print(rpc_res);
    cJSON_Delete(rpc_res);

    LOG_DBG("----\n%s\n----\n", ret.json);
    return(ret);
}

cnearResponse near_rpc_send_tx(nearTransaction* near_tx, const char* status)
{
    size_t len;
    char* b64out = NULL;
    cnearResponse vk, ret = {0};
    cJSON *rpc_res, *rpc_req, *item = NULL;
    curl_memory_t borsh_tx;

    near_tx->signer_id = near_account_id;
    near_tx->key_type = nearKeyTypeED25519;
    memcpy(near_tx->public_key, near_public_key, sizeof(ed25519_public_key));

    vk = near_rpc_view_access_key(near_account_id, near_b58_pub_key);
    rpc_res = cJSON_Parse(vk.json);
    free(vk.json);

    if (!rpc_res)
        return ret;

    if (vk.rpc_code != 200 || cJSON_HasObjectItem(rpc_res, "error"))
    {
        cJSON_Delete(rpc_res);
        return ret;
    }

    item = cJSON_GetObjectItemCaseSensitive(rpc_res, "block_hash");
    base58_decode(cJSON_GetStringValue(item), near_tx->block_hash, sizeof(near_tx->block_hash));

    item = cJSON_GetObjectItemCaseSensitive(rpc_res, "nonce");
    near_tx->nonce = (uint64_t) cJSON_GetNumberValue(item);
    near_tx->nonce++;
    cJSON_Delete(rpc_res);

    // Serialize transaction
    borsh_tx = serialize_transaction(near_tx);
    LOG_BUF("Tx", borsh_tx.memory, borsh_tx.size);

    // Sign serialized transaction
    sign_transaction(&borsh_tx);
    LOG_BUF("Signed Tx", borsh_tx.memory, borsh_tx.size);

    // Build JSON RPC request
    b64out = base64_encode(borsh_tx.memory, borsh_tx.size, &len);
    rpc_req = build_rpc_send_tx(b64out, status);
    free(borsh_tx.memory);
    free(b64out);

    if (!rpc_req)
        return ret;

    // Send JSON RPC request
    rpc_res = exec_curl_rpc_call(rpc_req, &ret.rpc_code);
    cJSON_Delete(rpc_req);

    if (!rpc_res)
        return ret;

    ret.json = cJSON_Print(rpc_res);
    cJSON_Delete(rpc_res);

    LOG_DBG("----\n%s\n----\n", ret.json);
    return ret;
}

bool near_account_init(const char* account, const char* b58_priv, const char* b58_pub)
{
    free(near_account_id);
    free(near_b58_pub_key);
    near_account_id = strdup(account);
    near_b58_pub_key = strdup(b58_pub);

    if (!b58decode_ed25519_key(near_public_key, sizeof(near_public_key), b58_pub))
        return false;
    LOG_BUF("Pub Key", near_public_key, sizeof(ed25519_public_key));

    if (!b58decode_ed25519_key(near_private_key, sizeof(near_private_key), b58_priv))
        return false;
    LOG_BUF("Priv Key", near_private_key, sizeof(near_private_key));

    return true;
}

bool near_account_init_json(const char* credentials_json)
{
    bool ret;
    const cJSON *item;
    char *account, *priv, *pub;
    cJSON *json = cJSON_Parse(credentials_json);

    if (!json)
        return false;

    item = cJSON_GetObjectItemCaseSensitive(json, "account_id");
    account = cJSON_GetStringValue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "private_key");
    priv = cJSON_GetStringValue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "public_key");
    pub = cJSON_GetStringValue(item);

    ret = near_account_init(account, priv, pub);
    cJSON_Delete(json);
    return ret;
}

uint8_t* near_decode_result(const cnearResponse* response, size_t* out_size)
{
    uint8_t* result_data = NULL;
    cJSON* rpc_res = cJSON_Parse(response->json);

    if (!rpc_res)
        return NULL;

    const cJSON* out = cJSON_GetObjectItemCaseSensitive(rpc_res, "result");
    if (cJSON_IsArray(out))
    {
        int i = 0;
        const cJSON *val = NULL;
        *out_size = cJSON_GetArraySize(out);
        result_data = malloc(cJSON_GetArraySize(out) +1);

        cJSON_ArrayForEach(val, out)
        {
            result_data[i++] = (cJSON_IsNumber(val) ? val->valueint : 0);
        }
        result_data[i] = 0;

        LOG_DBG("----\n%s\n----\n", result_data);
    }
    cJSON_Delete(rpc_res);

    return result_data;
}

cnearResponse near_contract_call(const char* contract, const char* method, const char* args, uint64_t gas, uint64_t deposit)
{
    nearAction call_action = {
        .method_name = (char*) method,
        .args = (char*) args,
        .gas = gas,
        .deposit = deposit
    };
    nearTransaction test_tx = {
        .signer_id = NULL,
        .nonce = 0,
        .receiver_id = (char*) contract,
        .actions = &call_action
    };

    return near_rpc_send_tx(&test_tx, NEAR_TX_STATUS_EXEC_OPTIMISTIC);
}
