#pragma once
#include "api/security/encryption.h"
#include "api/security/aes.h"

#define AES256_KEY_SIZE 32
#define AES256_IV_SIZE 16

class AES256 : public Encryption
{
protected:
    Byte key[AES256_KEY_SIZE];
    Byte iv[AES256_IV_SIZE];
    AES aes;
    bool ready;
    bool configured();

public:
    AES256(Byte* key = NULL, Byte* iv = NULL);
    ~AES256();

    void Key(Byte *key);
    void IV(Byte *iv);

    int Encrypt(Byte *data, int length, Byte *output);
    int Decrypt(Byte *data, int length, Byte *output);
    bool Ready();
};