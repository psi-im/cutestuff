#include"xmlenc.h"

#include"../util/bytestream.h"
#include"keyops.h"

static QByteArray nodeToArray(const QDomNode &e)
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


using namespace XmlEnc;

//----------------------------------------------------------------------------
// Encrypted
//----------------------------------------------------------------------------
Encrypted::Encrypted()
{
	baseNS = "http://www.w3.org/2001/04/xmlenc#";
	clear();
}

Encrypted::~Encrypted()
{
	clear();
}

void Encrypted::clear()
{
	v_method = None;
	v_id = "";
	v_dataType = Arbitrary;
	v_type = Data;
	v_mimeType = "";
	v_keyInfo = KeyInfo();

	v_cval = "";
	v_cref = "";
	v_creftrans = QDomElement();
}

Method Encrypted::cipherTypeToMethod(Cipher::Type t) const
{
	if(t == Cipher::TripleDES)
		return TripleDES;
	else if(t == Cipher::AES_128)
		return AES_128;
	else if(t == Cipher::AES_256)
		return AES_256;
	else
		return None;
}

void Encrypted::setDataReference(const QString &uri, Method m, const QDomElement &transforms)
{
	v_method = m;
	v_cref = uri;
	v_creftrans = transforms;
}

bool Encrypted::encryptData(const QByteArray &data, const Cipher::Key &key)
{
	QByteArray iv = Cipher::generateIV(key.type());
	sym_encrypt(data, key, iv, &v_cval);
	v_dataType = Arbitrary;
	v_method = cipherTypeToMethod(key.type());
	return true;
}

bool Encrypted::encryptElement(const QDomElement &data, const Cipher::Key &key)
{
	QByteArray iv = Cipher::generateIV(key.type());
	sym_encrypt(nodeToArray(data), key, iv, &v_cval);
	v_type = Data;
	v_dataType = Element;
	v_method = cipherTypeToMethod(key.type());
	return true;
}

bool Encrypted::encryptContent(const QDomElement &data, const Cipher::Key &key)
{
	// convert children to raw data
	QByteArray a;
	for(QDomNode n = data.firstChild(); !n.isNull(); n = n.nextSibling())
		ByteStream::appendArray(&a, nodeToArray(n));

	QByteArray iv = Cipher::generateIV(key.type());
	sym_encrypt(a, key, iv, &v_cval);
	v_type = Data;
	v_dataType = Content;
	v_method = cipherTypeToMethod(key.type());
	return true;
}

bool Encrypted::encryptKey(const Cipher::Key &data, const Cipher::Key &key)
{
	sym_keywrap(data.data(), key, &v_cval);
	v_type = Key;
	v_dataType = Arbitrary;
	v_method = cipherTypeToMethod(key.type());
	return true;
}

QByteArray Encrypted::decryptData(const Cipher::Key &key) const
{
	QByteArray result;
	sym_decrypt(v_cval, key, &result);
	return result;
}

QDomElement Encrypted::decryptElement(QDomDocument *doc, const Cipher::Key &key) const
{
	QByteArray result;
	sym_decrypt(v_cval, key, &result);

	QDomDocument d;
	if(!d.setContent(result))
		return QDomElement();
	return doc->importNode(d.documentElement(), true).toElement();
}

QDomNodeList Encrypted::decryptContent(QDomDocument *doc, const Cipher::Key &key) const
{
	QByteArray result;
	sym_decrypt(v_cval, key, &result);

	// nul-terminate
	result.resize(result.size()+1);
	result[result.size()-1] = 0;

	QCString cs = "<dummy>";
	cs += (char *)result.data();
	cs += "</dummy>";

	QDomDocument d;
	if(!d.setContent(cs))
		return QDomNodeList();
	QDomElement e = d.documentElement().firstChild().toElement();
	if(e.isNull() || e.tagName() != "dummy")
		return QDomNodeList();

	return doc->importNode(e, true).childNodes();
}

Cipher::Key Encrypted::decryptKey(const Cipher::Key &key) const
{
	QByteArray result;
	if(!sym_keyunwrap(v_cval, key, &result))
		printf("error unwrapping\n");

	// TODO: key type?

	Cipher::Key k;
	k.setType(key.type());
	k.setData(result);
	return k;
}

QDomElement Encrypted::toXml(QDomDocument *doc) const
{
	QString baseNS = "http://www.w3.org/2001/04/xmlenc#";
	QDomElement enc;

	// base xml
	if(v_type == Data)
		enc = doc->createElement("EncryptedData");
	else
		enc = doc->createElement("EncryptedKey");
	enc.setAttribute("xmlns", baseNS);
	if(v_type == Data)
		enc.setAttribute("Type", baseNS + "Element");

	// method
	QDomElement meth = doc->createElement("EncryptionMethod");
	meth.setAttribute("Algorithm", methodToAlgorithm(v_method, v_type));
	enc.appendChild(meth);

	// cipherdata
	QDomElement cd = doc->createElement("CipherData");
	if(!v_cref.isEmpty()) {
		QDomElement cr = doc->createElement("CipherReference");
		cr.setAttribute("URI", v_cref);
		if(!v_creftrans.isNull())
			cr.appendChild(v_creftrans);
		cd.appendChild(cr);
	}
	else {
		QDomElement cv = doc->createElement("CipherValue");
		cv.appendChild(doc->createTextNode(v_cval));
		cd.appendChild(cv);
	}
	enc.appendChild(cd);

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

	Type t;
	if(e.tagName() == "EncryptedData")
		t = Data;
	else if(e.tagName() == "EncryptedKey")
		t = Key;
	else
		return false;

	// method
	Method m = None;
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

	m = algorithmToMethod(str);

	// cipherdata
	QString cval, cref;
	QDomElement creftrans;
	i = findSubTag(e, "CipherData", &found);
	if(!found)
		return false;
	i = findSubTag(i, "CipherValue", &found);
	if(found)
		cval = i.text();
	else {
		i = findSubTag(i, "CipherReference", &found);
		if(!found)
			return false;
		cref = i.attribute("URI");
		creftrans = i.firstChild().toElement();
	}

	// looks good, let's take it
	clear();
	v_type = t;
	v_dataType = dt;
	v_method = m;
	v_cval = cval;
	v_cref = cref;
	v_creftrans = creftrans;

	return true;
}

QString Encrypted::methodToAlgorithm(Method m, Type t) const
{
	QString s;
	if(m == TripleDES)
		s = (t == Key ? "kw-tripledes": "tripledes-cbc");
	else if(m == AES_128)
		s = (t == Key ? "kw-aes128": "aes128-cbc");
	else if(m == AES_256)
		s = (t == Key ? "kw-aes256": "aes256-cbc");
	else
		return "";

	return (baseNS + s);
}

Method Encrypted::algorithmToMethod(const QString &s) const
{
	Method m;
	if(s == "tripledes-cbc" || s == "kw-tripledes")
		m = TripleDES;
	else if(s == "aes128-cbc" || s == "kw-aes128")
		m = AES_128;
	else if(s == "aes256-cbc" || s == "kw-aes256")
		m = AES_256;
	else
		return None;

	return m;
}
