/*
 * Copyright (c) 2012 Stanford University
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

#ifndef __KEY_H__
#define __KEY_H__

#include <string>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#ifdef OPENSSL_NO_SHA256
#error "SHA256 not supported!"
#endif

class PublicKey
{
public:
    PublicKey();
    ~PublicKey();
    void open(const std::string &keyfile);
    std::string getName();
    std::string getEmail();
    std::string computeDigest();
    bool verify(const std::string &blob, const std::string &digest);
private:
    X509 *x509;
    EVP_PKEY *key;
};

class PrivateKey
{
public:
    PrivateKey();
    ~PrivateKey();
    void open(const std::string &keyfile);
    std::string sign(const std::string &blob);
private:
    EVP_PKEY *key;
};

class KeyType
{
public:
    enum KeyType_t
    {
	Invalid,
	Public,
	Private,
    };
};

KeyType::KeyType_t Key_GetType(const std::string &keyfile);

#endif /* __KEY_H__ */

