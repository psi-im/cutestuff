#include"keyops.h"

#include"../util/bytestream.h"
#include"../util/base64.h"
#include"../util/sha1.h"

bool sym_encrypt(const QByteArray &data, const Cipher::Key &key, const QByteArray &iv, QString *out)
{
	QByteArray encData = iv.copy();
	bool ok;
	QByteArray a = Cipher::encrypt(data, key, iv, true, &ok);
	if(!ok)
		return false;
	ByteStream::appendArray(&encData, a);

	*out = Base64::arrayToString(encData);
	return true;
}

bool sym_decrypt(const QString &str, const Cipher::Key &key, QByteArray *out)
{
	QByteArray data = Base64::stringToArray(str);
	int r = Cipher::ivSize(key.type());
	if(r == -1)
		return false;
	if((int)data.size() < r)
		return false;
	QByteArray iv = ByteStream::takeArray(&data, r);
	bool ok;
	QByteArray result = Cipher::decrypt(data, key, iv, true, &ok);
	if(!ok)
		return false;

	*out = result;
	return true;
}

static QByteArray calcCMS(const QByteArray &key)
{
	QByteArray a = SHA1::hash(key);
	a.resize(8);
	return a;
}

unsigned char sym_3des_fixed_iv[8] = { 0x4a, 0xdd, 0xa2, 0x2c, 0x79, 0xe8, 0x21, 0x05 };
unsigned char sym_aes_fixed_val[8] = { 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6 };

static QByteArray cat64(const QByteArray &a1, const QByteArray &a2)
{
	QByteArray out = a1.copy();
	ByteStream::appendArray(&out, a2);
	return out;
}

static QByteArray xor64(const QByteArray &a1, const QByteArray &a2)
{
	QByteArray out(a1.size());
	for(uint n = 0; n < a1.size(); ++n)
		out[n] = a1[n] ^ a2[n];
	return out;
}

static QByteArray msb64(const QByteArray &a)
{
	QByteArray out(8);
	for(uint n = 0; n < 8; ++n)
		out[n] = a[n];
	return out;
}

static QByteArray lsb64(const QByteArray &a)
{
	QByteArray out(8);
	for(uint n = 0; n < 8; ++n)
		out[n] = a[n+8];
	return out;
}

static QByteArray get64(const QByteArray &from, uint x)
{
	QByteArray out(8);
	int base = (x-1) * 8;
	for(uint n = 0; n < 8; ++n)
		out[n] = from[base+n];
	return out;
}

static void set64(QByteArray *from, uint x, const QByteArray &a)
{
	int base = (x-1) * 8;
	for(uint n = 0; n < 8; ++n)
		(*from)[base+n] = a[n];
}

static QByteArray uintTo64(uint x)
{
	QByteArray out(8);
	out[0] = 0x00;
	out[1] = 0x00;
	out[2] = 0x00;
	out[3] = 0x00;
	out[4] = (x >> 24) & 0xff;
	out[5] = (x >> 16) & 0xff;
	out[6] = (x >>  8) & 0xff;
	out[7] = (x >>  0) & 0xff;
	return out;
}

bool sym_keywrap(const QByteArray &data, const Cipher::Key &key, QString *out)
{
	int x = key.type();
	if(x == Cipher::TripleDES) {
		QByteArray cks = calcCMS(data);
		QByteArray wkcks = data.copy();
		ByteStream::appendArray(&wkcks, cks);
		QByteArray iv = Cipher::generateIV(key.type());
		bool ok;
		QByteArray temp1 = Cipher::encrypt(wkcks, key, iv, false, &ok);
		if(!ok)
			return false;
		QByteArray temp2 = iv;
		ByteStream::appendArray(&temp2, temp1);
		QByteArray temp3(temp2.size());
		int n2 = (int)temp2.size()-1;
		for(int n = 0; n < (int)temp2.size(); ++n)
			temp3[n2--] = temp2[n];
		memcpy(iv.data(), sym_3des_fixed_iv, 8);
		QByteArray final = Cipher::encrypt(temp3, key, iv, false, &ok);
		if(!ok)
			return false;

		*out = Base64::arrayToString(final);
		return true;
	}
	else if(x == Cipher::AES_128 || x == Cipher::AES_256) {
		QByteArray c;
		QByteArray work = data.copy();
		int n = work.size() / 8;
		if(work.size() % 8)
			return false;
		if(n < 1)
			return false;

		if(n == 1) {
			QByteArray val(8);
			memcpy(val.data(), sym_aes_fixed_val, 8);
			bool ok;
			c = Cipher::encrypt(cat64(val, work), key, QByteArray(), false, &ok);
			if(!ok)
				return false;
		}
		else {
			QByteArray r = work;
			QByteArray a(8);
			memcpy(a.data(), sym_aes_fixed_val, 8);

			for(int j = 0; j <= 5; ++j) {
				for(int i = 1; i <= n; ++i) {
					uint t = i + (j * n);
					bool ok;
					QByteArray b = Cipher::encrypt(cat64(a, get64(r, i)), key, QByteArray(), false, &ok);
					if(!ok)
						return false;
					a = xor64(uintTo64(t), msb64(b));
					set64(&r, i, lsb64(b));
				}
			}

			c = a;
			ByteStream::appendArray(&c, r);
		}

		*out = Base64::arrayToString(c);
		return true;
	}

	return false;
}

bool sym_keyunwrap(const QString &str, const Cipher::Key &key, QByteArray *out)
{
	int x = key.type();
	QByteArray data = Base64::stringToArray(str);
	if(x == Cipher::TripleDES) {
		QByteArray iv(8);
		memcpy(iv.data(), sym_3des_fixed_iv, 8);
		bool ok;
		QByteArray temp3 = Cipher::decrypt(data, key, iv, false, &ok);
		if(!ok)
			return false;
		QByteArray temp2(temp3.size());
		int n2 = (int)temp3.size()-1;
		for(int n = 0; n < (int)temp3.size(); ++n)
			temp2[n2--] = temp3[n];
		if((int)temp2.size() < 8)
			return false;
		iv = ByteStream::takeArray(&temp2, 8);
		QByteArray temp1 = temp2;
		QByteArray wkcks = Cipher::decrypt(temp1, key, iv, false, &ok);
		if(!ok)
			return false;
		if((int)wkcks.size() < 8)
			return false;
		QByteArray wk = ByteStream::takeArray(&wkcks, wkcks.size() - 8);
		QByteArray cks = wkcks;
		QByteArray t = calcCMS(wk);

		if(cks.size() != t.size())
			return false;
		for(int n = 0; n < (int)t.size(); ++n) {
			if(t[n] != cks[n])
				return false;
		}
		*out = wk;
		return true;
	}
	else if(x == Cipher::AES_128 || x == Cipher::AES_256) {
		QByteArray p;
		QByteArray work = data.copy();
		if(work.size() % 8)
			return false;
		int n = (work.size() / 8) - 1;
		if(n < 1)
			return false;

		QByteArray a;
		if(n == 1) {
			bool ok;
			QByteArray b = Cipher::decrypt(work, key, QByteArray(), false, &ok);
			if(!ok)
				return false;
			ByteStream::appendArray(&p, lsb64(b));
			a = msb64(b);
		}
		else {
			a = ByteStream::takeArray(&work, 8);
			QByteArray r = work;
			for(int j = 5; j >= 0; --j) {
				for(int i = n; i >= 1; --i) {
					uint t = i + (j * n);
					QByteArray ta = xor64(uintTo64(t), a);
					bool ok;
					QByteArray b = Cipher::decrypt(cat64(ta, get64(r, i)), key, QByteArray(), false, &ok);
					if(!ok)
						return false;
					a = msb64(b);
					set64(&r, i, lsb64(b));
				}
			}

			p = r;
		}

		for(int i = 0; i < 8; ++i) {
			if((unsigned char)a[i] != sym_aes_fixed_val[i])
				return false;
		}

		*out = p;
		return true;
	}

	return false;
}
