#include"xmlenc.h"

#include"../util/base64.h"
#include"../util/sha1.h"
#include"../util/bytestream.h"

static QByteArray elemToArray(const QDomElement &e)
{
	QString out;
	QTextStream ts(&out, IO_WriteOnly);
	e.save(ts, 1);
	QCString xmlToEnc = out.utf8();
	int len = xmlToEnc.length();
	QByteArray b(len);
	memcpy(b.data(), xmlToEnc.data(), len);
	return b;
}

static QDomElement findSubTag(const QDomElement &e, const QString &name, bool *found)
{
	if(found)
		*found = false;

	for(QDomNode n = e.firstChild(); !n.isNull(); n = n.nextSibling()) {
		QDomElement i = n.toElement();
		if(i.isNull())
			continue;
		if(i.tagName() == name) {
			if(found)
				*found = true;
			return i;
		}
	}

	QDomElement tmp;
	return tmp;
}

// crypto functions
static QString sym_encrypt(const QByteArray &data, const Cipher::Key &key, const QByteArray &iv)
{
	QByteArray encData = iv.copy();
	ByteStream::appendArray(&encData, Cipher::encrypt(data, key, iv));
	return Base64::arrayToString(encData);
}

static QByteArray sym_decrypt(const QString &str, const Cipher::Key &key)
{
	QByteArray data = Base64::stringToArray(str);
	QByteArray iv = ByteStream::takeArray(&data, 8);
	return Cipher::decrypt(data, key, iv);
}

static QByteArray calcCMS(const QByteArray &key)
{
	QByteArray a = SHA1::hash(key);
	a.resize(8);
	return a;
}

unsigned char sym_3des_fixed_iv[8] = { 0x4a, 0xdd, 0xa2, 0x2c, 0x79, 0xe8, 0x21, 0x05 };

static QString sym_keywrap(const QByteArray &data, const Cipher::Key &key)
{
	if(key.type() == Cipher::TripleDES) {
		QByteArray cks = calcCMS(data);
		QByteArray wkcks = data.copy();
		ByteStream::appendArray(&wkcks, cks);
		//printf("wkcks: %d bytes\n", wkcks.size());
		QByteArray iv = Cipher::generateIV(key.type(), QCString("Key wrap is my job!"), QByteArray()); // TODO: this should be random
		QByteArray temp1 = Cipher::encrypt(wkcks, key, iv, false);
		//printf("temp1: %d bytes\n", temp1.size());
		QByteArray temp2 = iv;
		ByteStream::appendArray(&temp2, temp1);
		//printf("temp2: %d bytes\n", temp2.size());
		QByteArray temp3(temp2.size());
		int n2 = (int)temp2.size()-1;
		for(int n = 0; n < (int)temp2.size(); ++n)
			temp3[n2--] = temp2[n];
		//printf("temp3: %d bytes\n", temp3.size());
		memcpy(iv.data(), sym_3des_fixed_iv, 8);
		QByteArray final = Cipher::encrypt(temp3, key, iv, false);
		//printf("wrapped to %d bytes\n", final.size());
		return Base64::arrayToString(final);
	}
	else
		return "";
}

static QByteArray sym_keyunwrap(const QString &str, const Cipher::Key &key)
{
	if(key.type() == Cipher::TripleDES) {
		QByteArray data = Base64::stringToArray(str);
		//printf("unwrapping %d bytes\n", data.size());
		QByteArray iv(8);
		memcpy(iv.data(), sym_3des_fixed_iv, 8);
		QByteArray temp3 = Cipher::decrypt(data, key, iv, false);
		//printf("temp3: %d bytes\n", temp3.size());
		QByteArray temp2(temp3.size());
		int n2 = (int)temp3.size()-1;
		for(int n = 0; n < (int)temp3.size(); ++n)
			temp2[n2--] = temp3[n];
		iv = ByteStream::takeArray(&temp2, 8);
		QByteArray temp1 = temp2;
		QByteArray wkcks = Cipher::decrypt(temp1, key, iv, false);
		QByteArray wk = ByteStream::takeArray(&wkcks, wkcks.size() - 8);
		QByteArray cks = wkcks;
		QByteArray t = calcCMS(wk);
		// TODO: compare 't' with 'cks'
		return wk;
	}
	else
		return QByteArray();
}

using namespace XmlEnc;

//----------------------------------------------------------------------------
// Encrypted
//----------------------------------------------------------------------------
void Encrypted::setData(const QString &data, const QString &mimeType)
{
	v_type = Data;
	v_dataType = Arbitrary;
	v_data = data;
	v_mimeType = mimeType;
}

void Encrypted::setElement(const QDomElement &e)
{
	v_type = Data;
	v_dataType = Element;
	v_elem = e;
}

void Encrypted::setContent(const QDomElement &e)
{
	v_type = Data;
	v_dataType = Content;
	v_elem = e;
}

void Encrypted::setKey(const Cipher::Key &c)
{
	v_type = Key;
	v_dataType = Arbitrary;
	v_key = c;
}

QDomElement Encrypted::encrypt(QDomDocument *doc, const Cipher::Key &key) const
{
	QString baseNS = "http://www.w3.org/2001/04/xmlenc#";
	QDomElement enc;

	if(v_type == Data) {
		if(v_dataType == Element) {
			QByteArray iv = Cipher::generateIV(key.type(), QCString("Get this man an iv!"), QByteArray());
			QString encString = sym_encrypt(elemToArray(v_elem), key, iv);

			enc = doc->createElement("EncryptedData");
			enc.setAttribute("xmlns", baseNS);
			enc.setAttribute("Type", baseNS + "Element");
			QDomElement meth = doc->createElement("EncryptionMethod");
			meth.setAttribute("Algorithm", baseNS + "tripledes-cbc");
			enc.appendChild(meth);
			QDomElement cd = doc->createElement("CipherData");
			QDomElement cv = doc->createElement("CipherValue");
			cv.appendChild(doc->createTextNode(encString));
			cd.appendChild(cv);
			enc.appendChild(cd);
		}
	}
	else {
		QString encString = sym_keywrap(v_key.data(), key);

		enc = doc->createElement("EncryptedKey");
		enc.setAttribute("xmlns", baseNS);
		QDomElement meth = doc->createElement("EncryptionMethod");
		meth.setAttribute("Algorithm", baseNS + "kw-tripledes");
		enc.appendChild(meth);
		QDomElement cd = doc->createElement("CipherData");
		QDomElement cv = doc->createElement("CipherValue");
		cv.appendChild(doc->createTextNode(encString));
		cd.appendChild(cv);
		enc.appendChild(cd);
	}

	return enc;
}

bool Encrypted::fromXml(const QDomElement &e)
{
	QString baseNS = "http://www.w3.org/2001/04/xmlenc#";
	if(e.attribute("xmlns") != baseNS)
		return false;
	DataType dt = Arbitrary;
	if(e.hasAttribute("Type")) {
		QString str = e.attribute("Type");
		int n = str.find('#');
		if(n == -1)
			return false;
		++n;
		if(str.mid(0, n) != baseNS)
			return false;
		str = str.mid(n);
		if(str == "Element")
			dt = Element;
		else if(str == "Content")
			dt = Content;
	}

	if(e.tagName() == "EncryptedData") {
		Type t = Data;
		Method m = TripleDES;

		bool found;
		QDomElement i = findSubTag(e, "EncryptionMethod", &found);
		if(!found)
			return false;
		QString str = i.attribute("Algorithm");
		int n = str.find('#');
		if(n == -1)
			return false;
		++n;
		if(str.mid(0, n) != baseNS)
			return false;
		str = str.mid(n);
		if(str == "tripledes-cbc")
			m = TripleDES;
		else
			return false;

		QString cval;
		i = findSubTag(e, "CipherData", &found);
		if(!found)
			return false;
		i = findSubTag(i, "CipherValue", &found);
		if(!found)
			return false;
		cval = i.text();

		// take it
		v_type = t;
		v_data = dt;
		v_method = m;
		v_cval = cval;

		return true;
	}
	else if(e.tagName() == "EncryptedKey") {
		Type t = Key;
		Method m = TripleDES;

		bool found;
		QDomElement i = findSubTag(e, "EncryptionMethod", &found);
		if(!found)
			return false;
		QString str = i.attribute("Algorithm");
		int n = str.find('#');
		if(n == -1)
			return false;
		++n;
		if(str.mid(0, n) != baseNS)
			return false;
		str = str.mid(n);
		if(str == "kw-tripledes")
			m = TripleDES;
		else
			return false;

		QString cval;
		i = findSubTag(e, "CipherData", &found);
		if(!found)
			return false;
		i = findSubTag(i, "CipherValue", &found);
		if(!found)
			return false;
		cval = i.text();

		// take it
		v_type = t;
		v_data = dt;
		v_method = m;
		v_cval = cval;

		return true;
	}

	return false;
}

QByteArray Encrypted::decryptData(const Cipher::Key &) const
{
	return QByteArray();
}

QDomElement Encrypted::decryptElement(QDomDocument *doc, const Cipher::Key &key) const
{
	QByteArray result = sym_decrypt(v_cval, key);

	QDomDocument d;
	if(!d.setContent(result))
		return QDomElement();
	return doc->importNode(d.documentElement(), true).toElement();
}

QDomNodeList Encrypted::decryptContent(QDomDocument *, const Cipher::Key &) const
{
	return QDomNodeList();
}

Cipher::Key Encrypted::decryptKey(const Cipher::Key &key) const
{
	QByteArray result = sym_keyunwrap(v_cval, key);

	// TODO: key type?

	Cipher::Key k;
	k.setType(key.type());
	k.setData(result);
	return k;
}
