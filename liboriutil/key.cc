/*
 * Copyright (c) 2012-2013 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stdint.h>
#include <errno.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/safestack.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

#include <oriutil/key.h>

using namespace std;

#define SIGBUF_LEN 2048

PublicKey::PublicKey()
    : x509(NULL), key(NULL)
{
}

PublicKey::~PublicKey()
{
    if (key)
        EVP_PKEY_free(key);
}

void
PublicKey::open(const string &keyfile)
{
    FILE *f = fopen(keyfile.c_str(), "r");
    if (f == NULL)
        throw exception();

    x509 = PEM_read_X509(f, NULL, NULL, NULL);
    fclose(f);
    if (x509 == NULL)
    {
        // Error while attempting to read public key!
        throw exception();
    }

    key = X509_get_pubkey(x509);
    if (key == NULL)
    {
        // Cannot get public key from X509!
        throw exception();
    }

    return;
}

string
PublicKey::getName()
{
    assert(false);

    return "";
}

string
PublicKey::getEmail()
{
    string rval;
    STACK_OF(OPENSSL_STRING) *email;

    assert(x509 != NULL);

    email = X509_get1_email(x509);
    rval = sk_OPENSSL_STRING_value(email, 0);
    X509_email_free(email);

    return rval;
}

string
PublicKey::computeDigest()
{
    stringstream rval;
    unsigned int n;
    unsigned char md[EVP_MAX_MD_SIZE];

    assert(x509 != NULL);

    if (!X509_digest(x509, EVP_sha1(), md, &n))
    {
        throw exception();
    }

    // Generate hex string
    for (unsigned int i = 0; i < n; i++)
    {
        rval << hex << setw(2) << setfill('0') << (int)md[i];
    }

    return rval.str();
}

bool
PublicKey::verify(const string &blob,
                  const string &digest) const
{
    int err;
    EVP_MD_CTX *ctx;

    assert(x509 != NULL && key != NULL);

    ctx = EVP_MD_CTX_create();
    EVP_VerifyInit(ctx, EVP_sha256());
    EVP_VerifyUpdate(ctx, blob.data(), blob.size());
    err = EVP_VerifyFinal(ctx, (const unsigned char *)digest.data(),
                          digest.length(), key);
    EVP_MD_CTX_destroy(ctx);
    if (err != 1)
    {
        ERR_print_errors_fp(stderr);
        throw exception();
        return false;
    }

    // Prepend 8-byte public key digest

    return true;
}

PrivateKey::PrivateKey()
    : key(NULL)
{
}

PrivateKey::~PrivateKey()
{
    if (key)
        EVP_PKEY_free(key);
}

void
PrivateKey::open(const string &keyfile)
{
    FILE *f = fopen(keyfile.c_str(), "r");
    if (f == NULL)
        throw exception();
    key = PEM_read_PrivateKey(f, NULL, NULL, NULL);
    fclose(f);
    if (key == NULL)
    {
        ERR_print_errors_fp(stderr);
        throw exception();
    }

    return;
}

string
PrivateKey::sign(const string &blob) const
{
    int err;
    unsigned int sigLen = SIGBUF_LEN;
    char sigBuf[SIGBUF_LEN];
    EVP_MD_CTX* ctx;

    ctx = EVP_MD_CTX_create();
    EVP_SignInit(ctx, EVP_sha256());
    EVP_SignUpdate(ctx, blob.data(), blob.size());
    err = EVP_SignFinal(ctx, (unsigned char *)sigBuf, &sigLen, key);
    EVP_MD_CTX_destroy(ctx);
    if (err != 1)
    {
        ERR_print_errors_fp(stderr);
        throw exception();
    }

    // XXX: Prepend 8-byte public key digest

    return string().assign(sigBuf, sigLen);
}

/*
 * This function returns whether the key specified is public or private.
 */
KeyType::KeyType_t
Key_GetType(const string &keyfile)
{
    EVP_PKEY *key;
    X509 *x509;
    FILE *f = fopen(keyfile.c_str(), "r");
    if (f == NULL)
    {
        // File open failed
        return KeyType::Invalid;
    }

    key = PEM_read_PrivateKey(f, NULL, NULL, NULL);
    if (key != NULL) {
        fclose(f);
        EVP_PKEY_free(key);
        return KeyType::Private;
    }

    rewind(f);

    x509 = PEM_read_X509(f, NULL, NULL, NULL);
    fclose(f);
    if (x509 == NULL)
    {
        // Error while attempting to read public key!
        return KeyType::Invalid;
    }

    key = X509_get_pubkey(x509);
    if (key != NULL)
    {
        return KeyType::Public;
    }

    // Error while attempting to extract the public key!
    return KeyType::Invalid;
}

int
Key_selfTest(void)
{
    string teststr = "test string";
    string sig;
    bool isValid;

    assert(Key_GetType("test_priv.pem") == KeyType::Private);
    assert(Key_GetType("test_pub.pem") == KeyType::Public);

    PrivateKey priv = PrivateKey();
    PublicKey pub = PublicKey();

    priv.open("test_priv.pem");
    pub.open("test_pub.pem");

    sig = priv.sign(teststr);
    isValid = pub.verify(teststr, sig);
    assert(isValid);

    cout << pub.computeDigest() << endl;

    return 0;
}

