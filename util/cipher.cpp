/*
 * cipher.cpp - Simple wrapper to 3DES,AES128/256 CBC ciphers
 * Copyright (C) 2003  Justin Karneges
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include"cipher.h"

#include<openssl/evp.h>
#include"bytestream.h"

//#define HAVE_AES

using namespace Cipher;

static bool lib_encryptArray(EVP_CIPHER *type, const QByteArray &buf, const QByteArray &key, const QByteArray &iv, bool pad, QByteArray *out)
{
	QByteArray result(buf.size()+7);
	int len;
	EVP_CIPHER_CTX c;

	unsigned char *ivp = NULL;
	if(!iv.isEmpty())
		ivp = (unsigned char *)iv.data();
	EVP_CIPHER_CTX_init(&c);
	if(!EVP_EncryptInit(&c, type, (unsigned char *)key.data(), ivp))
		return false;
	if(!EVP_EncryptUpdate(&c, (unsigned char *)result.data(), &len, (unsigned char *)buf.data(), buf.size()))
		return false;
	result.resize(len);
	if(pad) {
		QByteArray last(8);
		if(!EVP_EncryptFinal(&c, (unsigned char *)last.data(), &len))
			return false;
		last.resize(len);
		ByteStream::appendArray(&result, last);
	}
	memset(&c, 0, sizeof(EVP_CIPHER_CTX));
	*out = result;
	return true;
}

static bool lib_decryptArray(EVP_CIPHER *type, const QByteArray &buf, const QByteArray &key, const QByteArray &iv, bool pad, QByteArray *out)
{
	QByteArray result(buf.size()+7);
	int len;
	EVP_CIPHER_CTX c;

	unsigned char *ivp = NULL;
	if(!iv.isEmpty())
		ivp = (unsigned char *)iv.data();
	EVP_CIPHER_CTX_init(&c);
	if(!EVP_DecryptInit(&c, type, (unsigned char *)key.data(), ivp))
		return false;
	if(!EVP_DecryptUpdate(&c, (unsigned char *)result.data(), &len, (unsigned char *)buf.data(), buf.size()))
		return false;
	result.resize(len);
	if(pad) {
		QByteArray last(8);
		if(!EVP_DecryptFinal(&c, (unsigned char *)last.data(), &len))
			return false;
		last.resize(len);
		ByteStream::appendArray(&result, last);
	}
	memset(&c, 0, sizeof(EVP_CIPHER_CTX));
	*out = result;
	return true;
}

static bool lib_generateKeyIV(EVP_CIPHER *type, const QByteArray &data, const QByteArray &salt, QByteArray *key, QByteArray *iv)
{
	QByteArray k, i;
	unsigned char *kp = 0;
	unsigned char *ip = 0;
	if(key) {
		k.resize(type->key_len);
		kp = (unsigned char *)k.data();
	}
	if(iv) {
		i.resize(type->iv_len);
		ip = (unsigned char *)i.data();
	}
	if(!EVP_BytesToKey(type, EVP_sha1(), (unsigned char *)salt.data(), (unsigned char *)data.data(), data.size(), 1, kp, ip))
		return false;
	if(key)
		*key = k;
	if(iv)
		*iv = i;
	return true;
}

static EVP_CIPHER * typeToCIPHER(Type t)
{
	if(t == TripleDES)
		return EVP_des_ede3_cbc();
#ifdef HAVE_AES
	else if(t == AES_128)
		return EVP_aes_128_cbc();
	else if(t == AES_256)
		return EVP_aes_256_cbc();
#endif
	else
		return 0;
}

Key Cipher::generateKey(Type t, const QByteArray &data, const QByteArray &salt)
{
	Key k;
	EVP_CIPHER *type = typeToCIPHER(t);
	if(!type)
		return k;
	QByteArray out;
	if(!lib_generateKeyIV(type, data, salt, &out, 0))
		return k;
	k.setType(t);
	k.setData(out);
	return k;
}

QByteArray Cipher::generateIV(Type t, const QByteArray &data, const QByteArray &salt)
{
	EVP_CIPHER *type = typeToCIPHER(t);
	if(!type)
		return QByteArray();
	QByteArray out;
	if(!lib_generateKeyIV(type, data, salt, 0, &out))
		return QByteArray();
	return out;
}

QByteArray Cipher::encrypt(const QByteArray &buf, const Key &key, const QByteArray &iv, bool pad)
{
	EVP_CIPHER *type = typeToCIPHER(key.type());
	if(!type)
		return QByteArray();
	QByteArray out;
	if(!lib_encryptArray(type, buf, key.data(), iv, pad, &out))
		return QByteArray();
	return out;
}

QByteArray Cipher::decrypt(const QByteArray &buf, const Key &key, const QByteArray &iv, bool pad)
{
	EVP_CIPHER *type = typeToCIPHER(key.type());
	if(!type)
		return QByteArray();
	QByteArray out;
	if(!lib_decryptArray(type, buf, key.data(), iv, pad, &out))
		return QByteArray();
	return out;
}
