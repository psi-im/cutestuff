#ifndef CS_KEYOPS_H
#define CS_KEYOPS_H

#include<qstring.h>
#include<qcstring.h>
#include"../util/cipher.h"

// sym_encrypt - encrypt 'data' and return a base64 string of the result
bool sym_encrypt(const QByteArray &data, const Cipher::Key &key, const QByteArray &iv, QString *out);

// sym_decrypt - take a base64 string, decode and decrypt it, return the result
bool sym_decrypt(const QString &str, const Cipher::Key &key, QByteArray *out);

// sym_keywrap - encrypt key 'data' and return a base64 string of the result
bool sym_keywrap(const QByteArray &data, const Cipher::Key &key, QString *out);

// sym_keyunwrap - take a base64 string, decode and decrypt it, return the result (key data)
bool sym_keyunwrap(const QString &str, const Cipher::Key &key, QByteArray *out);

#endif
