/*
 * xmlenc.cpp - XML Encryption
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

#include"xmlenc.h"

#include"../util/bytestream.h"
#include"../util/base64.h"
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
// KeyInfo
//----------------------------------------------------------------------------
KeyInfo::KeyInfo()
{
}

KeyInfo::~KeyInfo()
{
}

bool KeyInfo::isEmpty() const
{
	if(v_name.isEmpty() && v_value.isEmpty() && v_rmethods.isEmpty() && v_key.isNull())
		return true;
	return false;
}

QString KeyInfo::name() const
{
	return v_name;
}

QByteArray KeyInfo::value () const
{
	return v_value;
}

QStringList KeyInfo::retrievalMethods() const
{
	return v_rmethods;
}

QDomElement KeyInfo::encryptedKey() const
{
	return v_key;
}

void KeyInfo::setName(const QString &s)
{
	v_name = s;
}

void KeyInfo::setValue(const QByteArray &d)
{
	v_value = d;
}

void KeyInfo::setRetrievalMethods(const QStringList &s)
{
	v_rmethods = s;
}

void KeyInfo::attachEncryptedKey(const QDomElement &e)
{
	v_key = e;
}

QDomElement KeyInfo::toXml(QDomDocument *doc) const
{
	QDomElement e = doc->createElement("ds:KeyInfo");
	e.setAttribute("xmlns:ds", "http://www.w3.org/2000/09/xmldsig#");

	if(!v_value.isEmpty()) {
		QDomElement v = doc->createElement("ds:KeyValue");
		v.appendChild(doc->createTextNode(Base64::arrayToString(v_value)));
		e.appendChild(v);
	}

	if(!v_name.isEmpty()) {
		QDomElement n = doc->createElement("ds:KeyName");
		n.appendChild(doc->createTextNode(v_name));
		e.appendChild(n);
	}

	for(QStringList::ConstIterator it = v_rmethods.begin(); it != v_rmethods.end(); ++it) {
		QDomElement r = doc->createElement("ds:RetrievalMethod");
		r.setAttribute("Type", "http://www.w3.org/2001/04/xmlenc#EncryptedKey");
		r.setAttribute("URI", *it);
		e.appendChild(r);
	}

	if(!v_key.isNull())
		e.appendChild(v_key);

	return e;
}

bool KeyInfo::fromXml(const QDomElement &e)
{
	if(e.tagName() != "ds:KeyInfo" || e.attribute("xmlns:ds") != "http://www.w3.org/2000/09/xmldsig#")
		return false;

	QByteArray val;
	QString n;
	QStringList rml;
	QDomElement ek;

	bool found;
	QDomElement i;

	i = findSubTag(e, "ds:KeyValue", &found);
	if(found)
		val = Base64::stringToArray(i.text());
	i = findSubTag(e, "ds:KeyName", &found);
	if(found)
		n = i.text();
	QDomNodeList l = e.elementsByTagName("ds:RetrievalMethod");
	for(int n = 0; n < (int)l.count(); ++n) {
		QDomElement r = l.item(n).toElement();
		if(r.attribute("Type") == "http://www.w3.org/2001/04/xmlenc#EncryptedKey")
			rml += r.attribute("URI");
	}
	i = findSubTag(e, "EncryptedKey", &found);
	if(found)
		ek = i;

	// all good
	v_value = val;
	v_name = n;
	v_rmethods = rml;
	v_key = ek;

	return true;
}


//----------------------------------------------------------------------------
// EncryptionProperty
//----------------------------------------------------------------------------
EncryptionProperty::EncryptionProperty(const QString &target, const QString &id)
{
	v_target = target;
	v_id = id;
}

QString EncryptionProperty::target() const
{
	return v_target;
}

QString EncryptionProperty::id() const
{
	return v_id;
}

QString EncryptionProperty::property(const QString &var) const
{
	QStringList::ConstIterator it = vars.begin();
	QStringList::ConstIterator it2 = vals.begin();
	while(it != vars.end()) {
		if((*it) == var)
			return *it2;
		++it;
		++it2;
	}
	return "";
}

void EncryptionProperty::setTarget(const QString &target)
{
	v_target = target;
}

void EncryptionProperty::setId(const QString &id)
{
	v_id = id;
}

void EncryptionProperty::setProperty(const QString &var, const QString &val)
{
	if(var == "Target" || var == "Id")
		return;

	// see if we have it already
	QStringList::Iterator it = vars.begin();
	QStringList::Iterator it2 = vals.begin();
	while(it != vars.end()) {
		if((*it) == var) {
			*it2 = val;
			return;
		}
		++it;
		++it2;
	}

	vars += var;
	vals += val;
}

QDomElement EncryptionProperty::toXml(QDomDocument *doc) const
{
	QDomElement e = doc->createElement("EncryptionProperty");
	if(!v_target.isEmpty())
		e.setAttribute("Target", v_target);
	if(!v_id.isEmpty())
		e.setAttribute("Id", v_id);

	QStringList::ConstIterator it = vars.begin();
	QStringList::ConstIterator it2 = vals.begin();
	while(it != vars.end()) {
		e.setAttribute(*it, *it2);
		++it;
		++it2;
	}

	return e;
}

bool EncryptionProperty::fromXml(QDomElement &e)
{
	if(e.tagName() != "EncryptionProperty")
		return false;

	v_target = e.attribute("Target");
	v_id = e.attribute("Id");
	vars.clear();
	vals.clear();
	QDomNamedNodeMap map = e.attributes();
	for(int n = 0; n < (int)map.count(); ++n) {
		QDomAttr a = map.item(n).toAttr();
		QString n = a.name();
		if(n == "Target" || n == "Id")
			continue;
		vars += n;
		vals += a.value();
	}

	return true;
}


//----------------------------------------------------------------------------
// EncryptionProperties
//----------------------------------------------------------------------------
EncryptionProperties::EncryptionProperties(const QString &s)
:QValueList<EncryptionProperty>()
{
	v_id = s;
}

QString EncryptionProperties::id() const
{
	return v_id;
}

void EncryptionProperties::setId(const QString &s)
{
	v_id = s;
}

QDomElement EncryptionProperties::toXml(QDomDocument *doc) const
{
	QDomElement e = doc->createElement("EncryptionProperties");
	if(!v_id.isEmpty())
		e.setAttribute("Id", v_id);
	for(QValueList<EncryptionProperty>::ConstIterator it = begin(); it != end(); ++it) {
		const EncryptionProperty &p = *it;
		e.appendChild(p.toXml(doc));
	}
	return e;
}

bool EncryptionProperties::fromXml(QDomElement &e)
{
	if(e.tagName() != "EncryptionProperties")
		return false;

	clear();
	v_id = e.attribute("Id");
	QDomNodeList l = e.elementsByTagName("EncryptionProperty");
	for(int n = 0; n < (int)l.count(); ++n) {
		QDomElement r = l.item(n).toElement();
		EncryptionProperty p;
		if(!p.fromXml(r))
			return false;
		append(p);
	}

	return true;
}


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
	v_cref = Reference();
	v_carrykeyname = "";
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

void Encrypted::setDataReference(const Reference &cref, Method m)
{
	v_method = m;
	v_cref = cref;
}

bool Encrypted::encryptData(const QByteArray &data, const Cipher::Key &key)
{
	QByteArray iv = Cipher::generateIV(key.type());
	if(!sym_encrypt(data, key, iv, &v_cval))
		return false;
	v_dataType = Arbitrary;
	v_method = cipherTypeToMethod(key.type());
	return true;
}

bool Encrypted::encryptElement(const QDomElement &data, const Cipher::Key &key)
{
	QByteArray iv = Cipher::generateIV(key.type());
	if(!sym_encrypt(nodeToArray(data), key, iv, &v_cval))
		return false;
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
	if(!sym_encrypt(a, key, iv, &v_cval))
		return false;
	v_type = Data;
	v_dataType = Content;
	v_method = cipherTypeToMethod(key.type());
	return true;
}

bool Encrypted::encryptKey(const Cipher::Key &data, const Cipher::Key &key)
{
	if(!sym_keywrap(data.data(), key, &v_cval))
		return false;
	v_type = Key;
	v_dataType = Arbitrary;
	v_method = cipherTypeToMethod(key.type());
	return true;
}

QByteArray Encrypted::decryptData(const Cipher::Key &key) const
{
	QByteArray result;
	if(!sym_decrypt(v_cval, key, &result))
		return QByteArray();
	return result;
}

QDomElement Encrypted::decryptElement(QDomDocument *doc, const Cipher::Key &key) const
{
	QByteArray result;
	if(!sym_decrypt(v_cval, key, &result))
		return QDomElement();

	QDomDocument d;
	if(!d.setContent(result))
		return QDomElement();
	return doc->importNode(d.documentElement(), true).toElement();
}

QDomNodeList Encrypted::decryptContent(QDomDocument *doc, const Cipher::Key &key) const
{
	QByteArray result;
	if(!sym_decrypt(v_cval, key, &result))
		return QDomNodeList();

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

QByteArray Encrypted::decryptKey(const Cipher::Key &key) const
{
	QByteArray result;
	if(!sym_keyunwrap(v_cval, key, &result))
		return QByteArray();

	return result;
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
	if(!v_id.isEmpty())
		enc.setAttribute("Id", v_id);
	if(v_type == Data)
		enc.setAttribute("Type", baseNS + "Element");

	// method
	QDomElement meth = doc->createElement("EncryptionMethod");
	meth.setAttribute("Algorithm", methodToAlgorithm(v_method, v_type));
	enc.appendChild(meth);

	// keyinfo
	if(!v_keyInfo.isEmpty())
		enc.appendChild(v_keyInfo.toXml(doc));

	// cipherdata
	QDomElement cd = doc->createElement("CipherData");
	if(!v_cref.uri().isEmpty()) {
		QDomElement cr = doc->createElement("CipherReference");
		cr.setAttribute("URI", v_cref.uri());
		if(!v_cref.transforms().isNull())
			cr.appendChild(v_cref.transforms());
		cd.appendChild(cr);
	}
	else {
		QDomElement cv = doc->createElement("CipherValue");
		cv.appendChild(doc->createTextNode(v_cval));
		cd.appendChild(cv);
	}
	enc.appendChild(cd);

	// encryption properties
	if(!v_props.isEmpty())
		enc.appendChild(v_props.toXml(doc));

	// reference list
	if(!v_reflist.isEmpty()) {
		QDomElement e = doc->createElement("ReferenceList");
		for(QValueList<Reference>::ConstIterator it = v_reflist.begin(); it != v_reflist.end(); ++it) {
			const Reference &r = *it;
			QDomElement df = doc->createElement("DataReference");
			df.setAttribute("URI", r.uri());
			if(!r.transforms().isNull())
				df.appendChild(r.transforms());
			e.appendChild(df);
		}
		enc.appendChild(e);
	}

	// carry key name
	if(!v_carrykeyname.isEmpty()) {
		QDomElement e = doc->createElement("CarriedKeyName");
		e.appendChild(doc->createTextNode(v_carrykeyname));
		enc.appendChild(e);
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
	if(m == None)
		return false;

	// keyinfo
	KeyInfo ki;
	i = findSubTag(e, "ds:KeyInfo", &found);
	if(found) {
		if(!ki.fromXml(i))
			return false;
	}

	// cipherdata
	QString cval;
	Reference cref;
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
		cref.setURI(i.attribute("URI"));
		QDomElement tf = i.firstChild().toElement();
		if(!tf.isNull())
			cref.setTransforms(tf);
	}

	// encryption properties
	EncryptionProperties props;
	i = findSubTag(e, "EncryptionProperties", &found);
	if(found) {
		if(!props.fromXml(i))
			return false;
	}

	// reference list
	ReferenceList rl;
	i = findSubTag(e, "ReferenceList", &found);
	if(found) {
		QDomNodeList l = e.elementsByTagName("DataReference");
		for(int n = 0; n < (int)l.count(); ++n) {
			QDomElement dr = l.item(n).toElement();
			Reference r;
			r.setURI(dr.attribute("URI"));
			QDomElement tf = i.firstChild().toElement();
			if(!tf.isNull())
				r.setTransforms(tf);
			rl += r;
		}
	}

	// carry key name
	QString carrykey;
	i = findSubTag(e, "CarriedKeyName", &found);
	if(found)
		carrykey = i.text();

	// looks good, let's take it
	clear();
	v_type = t;
	v_dataType = dt;
	v_method = m;
	v_cval = cval;
	v_cref = cref;
	v_keyInfo = ki;
	v_props = props;
	v_reflist = rl;
	v_carrykeyname = carrykey;

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
