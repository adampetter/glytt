
#include "api/security/aes256.h"

AES256::AES256(Byte *key, Byte *iv)
{
    this->Key(key);
    this->IV(iv);
}

AES256::~AES256()
{
    this->Key(NULL);
    this->IV(NULL);
}

void AES256::Key(Byte *key)
{
    if (key)
        memcpy((Byte *)this->key, key, AES256_KEY_SIZE);
    else
        memset(this->key, 0, AES256_KEY_SIZE);

    this->ready = configured();
}

void AES256::IV(Byte *iv)
{
    if (iv)
        memcpy((Byte *)this->iv, iv, AES256_IV_SIZE);
    else
        memset(this->iv, 0, AES256_IV_SIZE);

    this->ready = configured();
}

int AES256::Encrypt(Byte *data, int length, Byte *output)
{
    Byte *encrypted = aes.EncryptCBC(data, length, this->key, this->iv);
    memcpy(output, encrypted, length);
    delete[] encrypted;
    return length;
}

int AES256::Decrypt(Byte *data, int length, Byte *output)
{
    Byte *decrypted = aes.DecryptCBC(data, length, this->key, this->iv);
    memcpy(output, decrypted, length);
    delete[] decrypted;
    return length;
}

bool AES256::Ready()
{
    return this->ready;
}

bool AES256::configured()
{
    for (Byte i = 0; i < AES256_KEY_SIZE; i++)
        if (this->key[i] != 0)
            for (Byte k = 0; k < AES256_IV_SIZE; k++)
                if (this->iv[k] != 0)
                    return true;

    return false;
}