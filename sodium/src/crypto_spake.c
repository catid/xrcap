
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sodium.h>

#include "pushpop.h"
#include "crypto_spake.h"

typedef struct spake_keys_ {
    unsigned char M[32];
    unsigned char N[32];
    unsigned char L[32];
    unsigned char h_K[32];
    unsigned char h_L[32];
} spake_keys;

typedef struct spake_validators_ {
    unsigned char client_validator[32];
    unsigned char server_validator[32];
} spake_validators;

#define H_VERSION   0x01
#define SER_VERSION 0x0001

static void
_random_scalar(unsigned char n[32])
{
    do {
        randombytes_buf(n, 32);
        n[0] &= 248;
        n[31] &= 127;
    } while (sodium_is_zero(n, 32));
}

static int
_create_keys(spake_keys *keys, unsigned char salt[crypto_pwhash_SALTBYTES],
             const char * const passwd, unsigned long long passwdlen,
             unsigned long long opslimit, size_t memlimit)
{
    unsigned char  h_MNKL[32 * 4];
    unsigned char *h_M = &h_MNKL[32 * 0];
    unsigned char *h_N = &h_MNKL[32 * 1];
    unsigned char *h_K = &h_MNKL[32 * 2];
    unsigned char *h_L = &h_MNKL[32 * 3];

    if (crypto_pwhash(h_MNKL, sizeof h_MNKL, passwd, passwdlen, salt,
                      opslimit, memlimit, crypto_pwhash_alg_default()) != 0) {
        return -1;
    }
    crypto_core_ed25519_from_uniform(keys->M, h_M);
    crypto_core_ed25519_from_uniform(keys->N, h_N);
    memcpy(keys->h_K, h_K, 32);
    memcpy(keys->h_L, h_L, 32);
    crypto_scalarmult_ed25519_base(keys->L, keys->h_L);

    return 0;
}

static int
_shared_keys_and_validators(crypto_spake_shared_keys *shared_keys,
                            spake_validators *validators,
                            const char *client_id, size_t client_id_len,
                            const char *server_id, size_t server_id_len,
                            const unsigned char X[32],
                            const unsigned char Y[32],
                            const unsigned char Z[32],
                            const unsigned char h_K[32],
                            const unsigned char V[32])
{
    crypto_generichash_state hst;
    unsigned char            k0[crypto_kdf_KEYBYTES];
    unsigned char            len;
    unsigned char            h_version;

    if (client_id_len > 255 || server_id_len > 255) {
        return -1;
    }
    crypto_generichash_init(&hst, NULL, 0, sizeof k0);

    h_version = H_VERSION;
    crypto_generichash_update(&hst, &h_version, 1);

    len = (unsigned char) client_id_len;
    crypto_generichash_update(&hst, &len, 1);
    crypto_generichash_update(&hst, (const unsigned char *) client_id, len);

    len = (unsigned char) server_id_len;
    crypto_generichash_update(&hst, &len, 1);
    crypto_generichash_update(&hst, (const unsigned char *) server_id, len);

    len = 32;
    crypto_generichash_update(&hst, X, len);
    crypto_generichash_update(&hst, Y, len);
    crypto_generichash_update(&hst, Z, len);
    crypto_generichash_update(&hst, h_K, len);
    crypto_generichash_update(&hst, V, len);

    crypto_generichash_final(&hst, k0, sizeof k0);

    crypto_kdf_derive_from_key(shared_keys->client_sk,
                               crypto_spake_SHAREDKEYBYTES, 0, "PAKE2+EE", k0);
    crypto_kdf_derive_from_key(shared_keys->server_sk,
                               crypto_spake_SHAREDKEYBYTES, 1, "PAKE2+EE", k0);
    crypto_kdf_derive_from_key(validators->client_validator,
                               32, 2, "PAKE2+EE", k0);
    crypto_kdf_derive_from_key(validators->server_validator,
                               32, 3, "PAKE2+EE", k0);

    sodium_memzero(k0, sizeof k0);

    return 0;
}

int
crypto_spake_server_store(unsigned char stored_data[crypto_spake_STOREDBYTES],
                          const char * const passwd,
                          unsigned long long passwdlen,
                          unsigned long long opslimit, size_t memlimit)
{
    spake_keys    keys;
    unsigned char salt[crypto_pwhash_SALTBYTES];
    size_t        i;

    randombytes_buf(salt, sizeof salt);
    if (_create_keys(&keys, salt, passwd, passwdlen, opslimit, memlimit) != 0) {
        return -1;
    }
    i = 0;
    _push16 (stored_data, &i, SER_VERSION);
    _push16 (stored_data, &i, (uint16_t) crypto_pwhash_alg_default());
    _push64 (stored_data, &i, (uint64_t) opslimit);
    _push64 (stored_data, &i, (uint64_t) memlimit);
    _push128(stored_data, &i, salt);
    _push256(stored_data, &i, keys.M);
    _push256(stored_data, &i, keys.N);
    _push256(stored_data, &i, keys.h_K);
    _push256(stored_data, &i, keys.L);
    assert(i == crypto_spake_STOREDBYTES);

    return 0;
}

int
crypto_spake_validate_public_data(const unsigned char public_data[crypto_spake_PUBLICDATABYTES],
                                  const int expected_alg,
                                  unsigned long long expected_opslimit,
                                  unsigned long long expected_memlimit)
{
    int                 alg;
    unsigned long long  opslimit;
    size_t              memlimit;
    size_t              i;
    uint16_t            v16;
    uint64_t            v64;

    i = 0;
    _pop16 (&v16, public_data, &i);
    _pop16 (&v16, public_data, &i); /* alg */
    alg = (int) v16;
    _pop64 (&v64, public_data, &i); /* opslimit */
    opslimit = (unsigned long long) v64;
    _pop64 (&v64, public_data, &i); /* memlimit */
    memlimit = (size_t) v64;

    if (alg != expected_alg ||
        opslimit != expected_opslimit || memlimit != expected_memlimit) {
        return -1;
    }
    return 0;
}

int
crypto_spake_step0_dummy(crypto_spake_server_state *st,
                         unsigned char public_data[crypto_spake_PUBLICDATABYTES],
                         const char *client_id, size_t client_id_len,
                         const char *server_id, size_t server_id_len,
                         unsigned long long opslimit, size_t memlimit,
                         const unsigned char key[crypto_spake_DUMMYKEYBYTES])
{
    crypto_generichash_state hst;
    unsigned char            salt[crypto_pwhash_SALTBYTES];
    size_t                   i;
    unsigned char            len;

    memset(st, 0, sizeof *st);
    if (client_id_len > 255 || server_id_len > 255) {
        return -1;
    }
    crypto_generichash_init(&hst, key, crypto_spake_DUMMYKEYBYTES, sizeof salt);
    len = (unsigned char) client_id_len;
    crypto_generichash_update(&hst, &len, 1);
    crypto_generichash_update(&hst, (const unsigned char *) client_id, len);
    len = (unsigned char) server_id_len;
    crypto_generichash_update(&hst, &len, 1);
    crypto_generichash_update(&hst, (const unsigned char *) server_id, len);

    i = 0;
    _push16 (public_data, &i, SER_VERSION);
    _push16 (public_data, &i, (uint16_t) crypto_pwhash_alg_default()); /* alg */
    _push64 (public_data, &i, (uint64_t) opslimit); /* opslimit */
    _push64 (public_data, &i, (uint64_t) memlimit); /* memlimit */

    crypto_generichash_update(&hst, public_data, i);
    crypto_generichash_final(&hst, salt, sizeof salt);

    _push128(public_data, &i, salt);                /* salt */
    assert(i == crypto_spake_PUBLICDATABYTES);

    return 0;
}

int
crypto_spake_step0(crypto_spake_server_state *st,
                   unsigned char public_data[crypto_spake_PUBLICDATABYTES],
                   const unsigned char stored_data[crypto_spake_STOREDBYTES])
{
    unsigned char salt[crypto_pwhash_SALTBYTES];
    size_t        i, j;
    uint16_t      v16;
    uint64_t      v64;

    memset(st, 0, sizeof *st);
    i = 0;
    j = 0;
    _pop16 (&v16, stored_data, &i); /* version */
    if (v16 != SER_VERSION) {
        return -1;
    }
    _push16(public_data, &j, v16);
    _pop16 (&v16, stored_data, &i); /* alg */
    _push16(public_data, &j, v16);
    _pop64 (&v64, stored_data, &i); /* opslimit */
    _push64(public_data, &j, v64);
    _pop64 (&v64, stored_data, &i); /* memlimit */
    _push64(public_data, &j, v64);
    _pop128(salt, stored_data, &i); /* salt */
    _push128(public_data, &j, salt);
    assert(j == crypto_spake_PUBLICDATABYTES);

    return 0;
}

int
crypto_spake_step1(crypto_spake_client_state *st,
                   unsigned char response1[crypto_spake_RESPONSE1BYTES],
                   const unsigned char public_data[crypto_spake_PUBLICDATABYTES],
                   const char * const passwd, unsigned long long passwdlen)
{
    spake_keys          keys;
    unsigned char       gx[32];
    unsigned char       salt[crypto_pwhash_SALTBYTES];
    unsigned char       x[32];
    unsigned char      *X = response1;
    int                 alg;
    unsigned long long  opslimit;
    size_t              memlimit;
    size_t              i;
    uint16_t            v16;
    uint64_t            v64;

    memset(st, 0, sizeof *st);
    i = 0;
    _pop16 (&v16, public_data, &i);
    if (v16 != SER_VERSION) {
        return -1;
    }
    _pop16 (&v16, public_data, &i); /* alg */
    alg = (int) v16;
    _pop64 (&v64, public_data, &i); /* opslimit */
    opslimit = (unsigned long long) v64;
    _pop64 (&v64, public_data, &i); /* memlimit */
    memlimit = (size_t) v64;
    _pop128(salt, public_data, &i); /* salt */
    if (_create_keys(&keys, salt, passwd, passwdlen, opslimit, memlimit) != 0) {
        sodium_memzero(st, sizeof *st);
        return -1;
    }
    _random_scalar(x);
    crypto_scalarmult_ed25519_base_noclamp(gx, x);
    crypto_core_ed25519_add(X, gx, keys.M);

    memcpy(st->h_K, keys.h_K, 32);
    memcpy(st->h_L, keys.h_L, 32);
    memcpy(st->N, keys.N, 32);
    memcpy(st->x, x, 32);
    memcpy(st->X, X, 32);

    return 0;
}

int
crypto_spake_step2(crypto_spake_server_state *st,
                   unsigned char response2[crypto_spake_RESPONSE2BYTES],
                   const char *client_id, size_t client_id_len,
                   const char *server_id, size_t server_id_len,
                   const unsigned char stored_data[crypto_spake_STOREDBYTES],
                   const unsigned char response1[crypto_spake_RESPONSE1BYTES])
{
    spake_validators     validators;
    spake_keys           keys;
    unsigned char        V[32];
    unsigned char        Z[32];
    unsigned char        gx[32];
    unsigned char        gy[32];
    unsigned char        salt[crypto_pwhash_SALTBYTES];
    unsigned char        y[32];
    unsigned char       *Y = response2;
    unsigned char       *client_validator = response2 + 32;
    const unsigned char *X = response1;
    size_t               i;
    uint16_t             v16;
    uint64_t             v64;

    i = 0;
    _pop16 (&v16, stored_data, &i); /* version */
    if (v16 != SER_VERSION) {
        return -1;
    }
    _pop16 (&v16, stored_data, &i); /* alg */
    _pop64 (&v64, stored_data, &i); /* opslimit */
    _pop64 (&v64, stored_data, &i); /* memlimit */
    _pop128(salt, stored_data, &i); /* salt */
    _pop256(keys.M,   stored_data, &i);
    _pop256(keys.N,   stored_data, &i);
    _pop256(keys.h_K, stored_data, &i);
    _pop256(keys.L,   stored_data, &i);

    _random_scalar(y);
    crypto_scalarmult_ed25519_base_noclamp(gy, y);
    crypto_core_ed25519_add(Y, gy, keys.N);

    crypto_core_ed25519_sub(gx, X, keys.M);
    if (crypto_scalarmult_ed25519_noclamp(Z, y, gx) != 0 ||
        crypto_scalarmult_ed25519_noclamp(V, y, keys.L) != 0 ||
        _shared_keys_and_validators(&st->shared_keys, &validators, client_id,
                                    client_id_len, server_id, server_id_len,
                                    X, Y, Z, keys.h_K, V) != 0) {
        sodium_memzero(st, sizeof *st);
        return -1;
    }
    memcpy(client_validator, validators.client_validator, 32);
    memcpy(st->server_validator, validators.server_validator, 32);

    return 0;
}

/* C -> S */

int
crypto_spake_step3(crypto_spake_client_state *st,
                   unsigned char response3[crypto_spake_RESPONSE3BYTES],
                   crypto_spake_shared_keys *shared_keys,
                   const char *client_id, size_t client_id_len,
                   const char *server_id, size_t server_id_len,
                   const unsigned char response2[crypto_spake_RESPONSE2BYTES])
{
    spake_validators     validators;
    unsigned char        V[32];
    unsigned char        Z[32];
    unsigned char        gy[32];
    unsigned char       *server_validator = response3;
    const unsigned char *Y = response2;
    const unsigned char *client_validator = response2 + 32;

    crypto_core_ed25519_sub(gy, Y, st->N);
    if (crypto_scalarmult_ed25519_noclamp(Z, st->x, gy) != 0 ||
        crypto_scalarmult_ed25519(V, st->h_L, gy) != 0 ||
        _shared_keys_and_validators(shared_keys, &validators, client_id,
                                    client_id_len, server_id, server_id_len,
                                    st->X, Y, Z, st->h_K, V) != 0 ||
        sodium_memcmp(client_validator, validators.client_validator, 32) != 0) {
        sodium_memzero(st, sizeof *st);
        return -1;
    }
    memcpy(server_validator, validators.server_validator, 32);
    sodium_memzero(st, sizeof *st);

    return 0;
}

int
crypto_spake_step4(crypto_spake_server_state *st,
                   crypto_spake_shared_keys *shared_keys,
                   const unsigned char response3[crypto_spake_RESPONSE3BYTES])
{
    const unsigned char *server_validator = response3;

    if (sodium_memcmp(server_validator, st->server_validator, 32) != 0) {
        sodium_memzero(st, sizeof *st);
        return -1;
    }
    memcpy(shared_keys, &st->shared_keys, sizeof *shared_keys);
    sodium_memzero(st, sizeof *st);

    return 0;
}
