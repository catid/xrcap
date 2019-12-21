/*
    A SPAKE2+EE (SPAKE2+ Elligator Edition) implementation for libsodium 1.0.17+

    ## Blurb

    SPAKE2 is a password-authenticated key agreement protocol, allowing two parties
    that share a password to securely authenticate each other and derive ephemeral
    session keys. It is secure and computationally efficient.

    This is an implementation of the
    [SPAKE2+EE](https://moderncrypto.org/mail-archive/curves/2015/000424.html)
    variant. It's slightly faster than the original SPAKE2 and has better security
    assumptions. It is also augmented, meaning that even if the credentials stored
    on the server ever get leaked, this would not be sufficient to log in.
*/

/*
    Comments on security of this scheme:
    https://gist.github.com/Sc00bz/4353f0efd68ef456679372b5cbe4527e
    It seems to be the most secure option, better than SRP etc?

    I had actually implemented this same idea 5 years ago on a different,
    faster, curve: https://github.com/catid/tabby/

    This being said, I believe the sodium implementation is far more trustworthy
    and I would prefer to use their code since I am not trying to innovate here,
    and my application is not performance constrained in the cryptography so
    there is no reason to take any risks.

    The one advantage I have here is that I understand pretty deeply what is
    going on and I would use this to secure my own data with confidence.
*/

#ifndef crypto_spake_H
#define crypto_spake_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define crypto_spake_DUMMYKEYBYTES    32
#define crypto_spake_PUBLICDATABYTES  36
#define crypto_spake_RESPONSE1BYTES   32
#define crypto_spake_RESPONSE2BYTES   64
#define crypto_spake_RESPONSE3BYTES   32
#define crypto_spake_SHAREDKEYBYTES   32
#define crypto_spake_STOREDBYTES     164

typedef struct crypto_spake_shared_keys_ {
    unsigned char client_sk[crypto_spake_SHAREDKEYBYTES];
    unsigned char server_sk[crypto_spake_SHAREDKEYBYTES];
} crypto_spake_shared_keys;

typedef struct crypto_spake_client_state_ {
    unsigned char h_K[32];
    unsigned char h_L[32];
    unsigned char N[32];
    unsigned char x[32];
    unsigned char X[32];
} crypto_spake_client_state;

typedef struct crypto_spake_server_state_ {
    unsigned char server_validator[32];
    crypto_spake_shared_keys shared_keys;
} crypto_spake_server_state;

int crypto_spake_server_store(unsigned char stored_data[crypto_spake_STOREDBYTES],
                              const char * const passwd, unsigned long long passwdlen,
                              unsigned long long opslimit, size_t memlimit);

int crypto_spake_validate_public_data(const unsigned char public_data[crypto_spake_PUBLICDATABYTES],
                                      const int expected_alg,
                                      unsigned long long expected_opslimit,
                                      unsigned long long expected_memlimit);

int crypto_spake_step0_dummy(crypto_spake_server_state *st,
                             unsigned char public_data[crypto_spake_PUBLICDATABYTES],
                             const char *client_id, size_t client_id_len,
                             const char *server_id, size_t server_id_len,
                             unsigned long long opslimit, size_t memlimit,
                             const unsigned char key[crypto_spake_DUMMYKEYBYTES]);

int crypto_spake_step0(crypto_spake_server_state *st,
                       unsigned char public_data[crypto_spake_PUBLICDATABYTES],
                       const unsigned char stored_data[crypto_spake_STOREDBYTES]);

int crypto_spake_step1(crypto_spake_client_state *st, unsigned char response1[crypto_spake_RESPONSE1BYTES],
                       const unsigned char public_data[crypto_spake_PUBLICDATABYTES],
                       const char * const passwd, unsigned long long passwdlen);

int crypto_spake_step2(crypto_spake_server_state *st,
                       unsigned char response2[crypto_spake_RESPONSE2BYTES],
                       const char *client_id, size_t client_id_len,
                       const char *server_id, size_t server_id_len,
                       const unsigned char stored_data[crypto_spake_STOREDBYTES],
                       const unsigned char response1[crypto_spake_RESPONSE1BYTES]);

int crypto_spake_step3(crypto_spake_client_state *st,
                       unsigned char response3[crypto_spake_RESPONSE3BYTES],
                       crypto_spake_shared_keys *shared_keys,
                       const char *client_id, size_t client_id_len,
                       const char *server_id, size_t server_id_len,
                       const unsigned char response2[crypto_spake_RESPONSE2BYTES]);

int crypto_spake_step4(crypto_spake_server_state *st,
                       crypto_spake_shared_keys *shared_keys,
                       const unsigned char response3[crypto_spake_RESPONSE3BYTES]);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
