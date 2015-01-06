/***************************************************************************
 *   Copyright (C) 2005-2007 by NetSieben Technologies INC                 *
 *   Author: Andrew Useckas                                                *
 *   Email: andrew@netsieben.com                                           *
 *                                                                         *
 *   Windows Port and bugfixes: Keef Aragon <keef@netsieben.com>           *
 *                                                                         *
 *   This program may be distributed under the terms of the Q Public       *
 *   License as defined by Trolltech AS of Norway and appearing in the     *
 *   file LICENSE.QPL included in the packaging of this file.              *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
 ***************************************************************************/

#include "ne7ssh_keys.h"
#include "ne7ssh.h"
#include <botan/base64.h>
#include <botan/look_pk.h>
#include <cstdio>
#include <ctype.h>
#include <sys/stat.h>

using namespace std;
using namespace Botan;

const std::string ne7ssh_keys::headerDSA = "-----BEGIN DSA PRIVATE KEY-----\n";
const std::string ne7ssh_keys::footerDSA = "-----END DSA PRIVATE KEY-----\n";
const std::string ne7ssh_keys::headerRSA = "-----BEGIN RSA PRIVATE KEY-----\n";
const std::string ne7ssh_keys::footerRSA = "-----END RSA PRIVATE KEY-----\n";


ne7ssh_keys::ne7ssh_keys() : dsaPrivateKey(0), rsaPrivateKey(0), keyAlgo(0)
{
}

ne7ssh_keys::~ne7ssh_keys()
{
    if (dsaPrivateKey)
    {
        delete dsaPrivateKey;
    }
    if (rsaPrivateKey)
    {
        delete rsaPrivateKey;
    }
}

bool ne7ssh_keys::generateRSAKeys(const char* fqdn, const char* privKeyFileName, const char* pubKeyFileName, uint16 keySize)
{
    RSA_PrivateKey* rsaPrivKey;
    BigInt e, n, d, p, q;
    BigInt dmp1, dmq1, iqmp;
    ne7ssh_string pubKeyBlob;
    FILE* privKeyFile, * pubKeyFile;
    std::string privKeyEncoded;
    DER_Encoder encoder;

    if (keySize > MAX_KEYSIZE)
    {
        ne7ssh::errors()->push(-1, "Specified key size: '%i' is larger than allowed maximum.", keySize);
        return false;
    }

    if (keySize < 1024)
    {
        ne7ssh::errors()->push(-1, "Key Size: '%i' is too small. Use at least 1024 key size for RSA keys.", keySize);
        return false;
    }

    rsaPrivKey = new RSA_PrivateKey(*ne7ssh::s_rng, keySize);
    privKeyFile = fopen(privKeyFileName, "w");

    e = rsaPrivKey->get_e();
    n = rsaPrivKey->get_n();

    d = rsaPrivKey->get_d();
    p = rsaPrivKey->get_p();
    q = rsaPrivKey->get_q();

    dmp1 = d % (p - 1);
    dmq1 = d % (q - 1);
    iqmp = inverse_mod(q, p);

    pubKeyBlob.addString("ssh-rsa");
    pubKeyBlob.addBigInt(e);
    pubKeyBlob.addBigInt(n);

    Pipe base64it(new Base64_Encoder);
    base64it.process_msg(pubKeyBlob.value());

    SecureVector<Botan::byte> pubKeyBase64 = base64it.read_all();

    pubKeyFile = fopen(pubKeyFileName, "w");

    if (!pubKeyFile)
    {
        ne7ssh::errors()->push(-1, "Cannot open file where public key is stored. Filename: %s", pubKeyFileName);
        delete rsaPrivKey;
        return false;
    }

    if ((!fwrite("ssh-rsa ", 8, 1, pubKeyFile)) ||
        (!fwrite(pubKeyBase64.begin(), (size_t) pubKeyBase64.size(), 1, pubKeyFile)) ||
        (!fwrite(" ", 1, 1, pubKeyFile)) ||
        (!fwrite(fqdn, strlen(fqdn), 1, pubKeyFile)) ||
        (!fwrite("\n", 1, 1, pubKeyFile)))
    {
        ne7ssh::errors()->push(-1, "I/O error while writting to file: %s.", pubKeyFileName);
        delete rsaPrivKey;
        return false;
    }

    fclose(pubKeyFile);

    privKeyEncoded = PEM_Code::encode(
        DER_Encoder().start_cons(SEQUENCE)
        .encode((size_t)0U)
        .encode(n)
        .encode(e)
        .encode(d)
        .encode(p)
        .encode(q)
        .encode(dmp1)
        .encode(dmq1)
        .encode(iqmp)
        .end_cons()
        .get_contents(), "RSA PRIVATE KEY");

    if (!privKeyFile)
    {
        ne7ssh::errors()->push(-1, "Cannot open file where the private key is stored. Filename: %s.", privKeyFileName);
        delete rsaPrivKey;
        return false;
    }

    if (!fwrite(privKeyEncoded.c_str(), privKeyEncoded.length(), 1, privKeyFile))
    {
        ne7ssh::errors()->push(-1, "IO error while writting to file: %s.", privKeyFileName);
        delete rsaPrivKey;
        return false;
    }
    fclose(privKeyFile);

    delete rsaPrivKey;
    return true;
}

bool ne7ssh_keys::generateDSAKeys(const char* fqdn, const char* privKeyFileName, const char* pubKeyFileName, uint16 keySize)
{
    DER_Encoder encoder;
    BigInt p, q, g, y, x;
    ne7ssh_string pubKeyBlob;
    FILE* privKeyFile, * pubKeyFile;
    std::string privKeyEncoded;

    if (keySize != 1024)
    {
        ne7ssh::errors()->push(-1, "DSA keys must be 1024 bits.");
        return false;
    }

    DL_Group dsaGroup(*ne7ssh::s_rng, Botan::DL_Group::DSA_Kosherizer, keySize);
    DSA_PrivateKey privDsaKey(*ne7ssh::s_rng, dsaGroup);
    DSA_PublicKey pubDsaKey = privDsaKey;

    p = dsaGroup.get_p();
    q = dsaGroup.get_q();
    g = dsaGroup.get_g();
    y = pubDsaKey.get_y();
    x = privDsaKey.get_x();

    pubKeyBlob.addString("ssh-dss");
    pubKeyBlob.addBigInt(p);
    pubKeyBlob.addBigInt(q);
    pubKeyBlob.addBigInt(g);
    pubKeyBlob.addBigInt(y);

    Pipe base64it(new Base64_Encoder);
    base64it.process_msg(pubKeyBlob.value());

    SecureVector<Botan::byte> pubKeyBase64 = base64it.read_all();

    pubKeyFile = fopen(pubKeyFileName, "w");

    if (!pubKeyFile)
    {
        ne7ssh::errors()->push(-1, "Cannot open file where public key is stored. Filename: %s", pubKeyFileName);
        return false;
    }

    if ((!fwrite("ssh-dss ", 8, 1, pubKeyFile)) ||
        (!fwrite(pubKeyBase64.begin(), (size_t) pubKeyBase64.size(), 1, pubKeyFile)) ||
        (!fwrite(" ", 1, 1, pubKeyFile)) ||
        (!fwrite(fqdn, strlen(fqdn), 1, pubKeyFile)) ||
        (!fwrite("\n", 1, 1, pubKeyFile)))
    {
        ne7ssh::errors()->push(-1, "I/O error while writting to file: %s.", pubKeyFileName);
        return false;
    }
    fclose(pubKeyFile);

    encoder.start_cons(SEQUENCE)
    .encode((size_t)0U)
    .encode(p)
    .encode(q)
    .encode(g)
    .encode(y)
    .encode(x)
    .end_cons();
    privKeyEncoded = PEM_Code::encode(encoder.get_contents(), "DSA PRIVATE KEY");

    privKeyFile = fopen(privKeyFileName, "w");

    if (!privKeyFile)
    {
        ne7ssh::errors()->push(-1, "Cannot open file where private key is stored. Filename: %s", privKeyFileName);
        return false;
    }

    if (!fwrite(privKeyEncoded.c_str(), (size_t) privKeyEncoded.length(), 1, privKeyFile))
    {
        ne7ssh::errors()->push(-1, "I/O error while writting to file: %s.", privKeyFileName);
        return false;
    }
    fclose(privKeyFile);

//  delete dsaGroup;

    return true;
}

SecureVector<Botan::byte>& ne7ssh_keys::generateSignature(Botan::SecureVector<Botan::byte>& sessionID, Botan::SecureVector<Botan::byte>& signingData)
{
    this->signature.clear();
    switch (this->keyAlgo)
    {
        case DSA:
            this->signature = generateDSASignature(sessionID, signingData);
            return (signature);

        case RSA:
            this->signature = generateRSASignature(sessionID, signingData);
            return (signature);

        default:
            this->signature.clear();
            return (signature);
    }
}

SecureVector<Botan::byte> ne7ssh_keys::generateDSASignature(Botan::SecureVector<Botan::byte>& sessionID, Botan::SecureVector<Botan::byte>& signingData)
{
    SecureVector<Botan::byte> sigRaw;
    ne7ssh_string sigData, sig;

    sigData.addVectorField(sessionID);
    sigData.addVector(signingData);
    if (!dsaPrivateKey)
    {
        ne7ssh::errors()->push(-1, "Private DSA key not initialized.");
        return sig.value();
    }

    PK_Signer* DSASigner = new PK_Signer(*dsaPrivateKey, "EMSA1(SHA-1)");
    sigRaw = DSASigner->sign_message(sigData.value(), *ne7ssh::s_rng);

    if (!sigRaw.size())
    {
        ne7ssh::errors()->push(-1, "Failure to generate DSA signature.");
        delete DSASigner;
        return sig.value();
    }

    if (sigRaw.size() != 40)
    {
        ne7ssh::errors()->push(-1, "DSS signature block <> 320 bits. Make sure you are using 1024 bit keys for authentication!");
        sig.clear();
        return sig.value();
    }

    delete DSASigner;
    sig.addString("ssh-dss");
    sig.addVectorField(sigRaw);
    return (sig.value());
}

SecureVector<Botan::byte> ne7ssh_keys::generateRSASignature(Botan::SecureVector<Botan::byte>& sessionID, Botan::SecureVector<Botan::byte>& signingData)
{
    SecureVector<Botan::byte> sigRaw;
    ne7ssh_string sigData, sig;

    sigData.addVectorField(sessionID);
    sigData.addVector(signingData);
    if (!rsaPrivateKey)
    {
        ne7ssh::errors()->push(-1, "Private RSA key not initialized.");
        return sig.value();
    }

    PK_Signer* RSASigner = new PK_Signer(*rsaPrivateKey, "EMSA3(SHA-1)");
    sigRaw = RSASigner->sign_message(sigData.value(), *ne7ssh::s_rng);
    if (!sigRaw.size())
    {
        ne7ssh::errors()->push(-1, "Failure while generating RSA signature.");
        delete RSASigner;
        return sig.value();
    }

    delete RSASigner;
    sig.addString("ssh-rsa");
    sig.addVectorField(sigRaw);
    return (sig.value());
}

bool ne7ssh_keys::getKeyPairFromFile(const char* privKeyFileName)
{
    ne7ssh_string privKeyStr;
    std::string buffer;
#ifndef WIN32
    struct stat privKeyStatus;

	if (lstat(privKeyFileName, &privKeyStatus) < 0)
    {
        ne7ssh::errors()->push(-1, "Cannot read file status: '%s'.", privKeyFileName);
        return false;
    }

	if ((privKeyStatus.st_mode & (S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)) != 0)
    {
        ne7ssh::errors()->push(-1, "Private key file permissions are read/write by others: '%s'.", privKeyFileName);
        return false;
    }
#endif
    if (!privKeyStr.addFile(privKeyFileName))
    {
        ne7ssh::errors()->push(-1, "Cannot read PEM file: '%s'.", privKeyFileName);
        return false;
    }

    buffer.assign((const char*)privKeyStr.value().begin(), privKeyStr.length());
    // Find all CR-LF, and remove the CR
    buffer.erase(std::remove(buffer.begin(), buffer.end(), '\r'), buffer.end());

    if ((buffer.find(headerRSA) == 0) && (buffer.find(footerRSA) == (buffer.length() - footerRSA.length())))
    {
        this->keyAlgo = ne7ssh_keys::RSA;
    }
    else if ((buffer.find(headerDSA) == 0) && (buffer.find(footerDSA) == (buffer.length() - footerDSA.length())))
    {
        this->keyAlgo = ne7ssh_keys::DSA;
    }
    else
    {
        ne7ssh::errors()->push(-1, "Encountered unknown PEM file format. Perhaps not an SSH private key file: '%s'.", privKeyFileName);
        return false;
    }

    SecureVector<Botan::byte> keyVector((Botan::byte*)buffer.c_str(), buffer.length());
    switch (this->keyAlgo)
    {
        case DSA:
            if (!getDSAKeys((char*)keyVector.begin(), keyVector.size()))
            {
                return false;
            }
            break;

        case RSA:
            if (!getRSAKeys((char*)keyVector.begin(), keyVector.size()))
            {
                return false;
            }
            break;
    }

    return true;
}

bool ne7ssh_keys::getDSAKeys(char* buffer, uint32 size)
{
//  DataSource_Memory privKeyPEMSrc (privKeyPEMStr);
    SecureVector<Botan::byte> keyDataRaw;
    BigInt p, q, g, y, x;
    char* start;
    size_t version;

    start = buffer + headerDSA.length();
    Pipe base64dec(new Base64_Decoder);
    base64dec.process_msg((Botan::byte*)start, size - footerDSA.length() - headerDSA.length());
    keyDataRaw = base64dec.read_all();

    BER_Decoder decoder(keyDataRaw);

    BER_Decoder sequence = decoder.start_cons(SEQUENCE);
    sequence.decode(version);

    if (version)
    {
        ne7ssh::errors()->push(-1, "Encountered unknown DSA key version.");
        return false;
    }

    sequence.decode(p);
    sequence.decode(q);
    sequence.decode(g);
    sequence.decode(y);
    sequence.decode(x);

    sequence.discard_remaining();
    sequence.verify_end();

    if (p.is_zero() || q.is_zero() || g.is_zero() || y.is_zero() || x.is_zero())
    {
        ne7ssh::errors()->push(-1, "Could not decode the supplied DSA key.");
        return false;
    }

    DL_Group dsaGroup(p, q, g);

    dsaPrivateKey = new DSA_PrivateKey(*ne7ssh::s_rng, dsaGroup, x);
    publicKeyBlob.clear();
    publicKeyBlob.addString("ssh-dss");
    publicKeyBlob.addBigInt(p);
    publicKeyBlob.addBigInt(q);
    publicKeyBlob.addBigInt(g);
    publicKeyBlob.addBigInt(y);

    return true;
}

bool ne7ssh_keys::getRSAKeys(char* buffer, uint32 size)
{
    SecureVector<Botan::byte> keyDataRaw;
    BigInt p, q, e, d, n;
    char* start;
    size_t version;

    start = buffer + headerRSA.length();
    Pipe base64dec(new Base64_Decoder);
    base64dec.process_msg((Botan::byte*)start, size - footerRSA.length() - headerRSA.length());
    keyDataRaw = base64dec.read_all();

    BER_Decoder decoder(keyDataRaw);

    BER_Decoder sequence = decoder.start_cons(SEQUENCE);
    sequence.decode(version);

    if (version)
    {
        ne7ssh::errors()->push(-1, "Encountered unknown RSA key version.");
        return false;
    }

    sequence.decode(n);
    sequence.decode(e);
    sequence.decode(d);
    sequence.decode(p);
    sequence.decode(q);

    sequence.discard_remaining();
    sequence.verify_end();

    if (n.is_zero() || e.is_zero() || d.is_zero() || p.is_zero() || q.is_zero())
    {
        ne7ssh::errors()->push(-1, "Could not decode the supplied RSA key.");
        return false;
    }

    rsaPrivateKey = new RSA_PrivateKey(*ne7ssh::s_rng, p, q, e, d, n);
    publicKeyBlob.clear();
    publicKeyBlob.addString("ssh-rsa");
    publicKeyBlob.addBigInt(e);
    publicKeyBlob.addBigInt(n);

    return true;
}

SecureVector<Botan::byte>& ne7ssh_keys::getPublicKeyBlob()
{
    return publicKeyBlob.value();
}

