/*
 * xmlenc.h - XML Encryption
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

#ifndef CS_XMLENC_H
#define CS_XMLENC_H

#include<qstring.h>
#include<qcstring.h>
#include<qstringlist.h>
#include<qdom.h>
#include<qvaluelist.h>
#include"../util/cipher.h"

namespace XmlEnc
{
	enum Method { None, TripleDES, AES_128, AES_256 };
	enum DataType { Arbitrary, Element, Content };

	class KeyInfo
	{
	public:
		KeyInfo();
		~KeyInfo();

		bool isEmpty() const;
		QString name() const;
		QByteArray value () const;
		QStringList retrievalMethods() const;
		QDomElement encryptedKey() const;
		void setName(const QString &);
		void setValue(const QByteArray &);
		void setRetrievalMethods(const QStringList &);
		void attachEncryptedKey(const QDomElement &);

		QDomElement toXml(QDomDocument *) const;
		bool fromXml(const QDomElement &);

	private:
		QString v_name;
		QByteArray v_value;
		QStringList v_rmethods;
		QDomElement v_key;
	};

	class Reference
	{
	public:
		Reference() {}

		QString uri() const { return v_uri; }
		QDomElement transforms() const { return v_trans; }
		void setURI(const QString &s) { v_uri = s; }
		void setTransforms(const QDomElement &e) { v_trans = e; }

	private:
		QString v_uri;
		QDomElement v_trans;
	};
	typedef QValueList<Reference> ReferenceList;

	class EncryptionProperty
	{
	public:
		EncryptionProperty(const QString &target="", const QString &id="");

		QString target() const;
		QString id() const;
		QString property(const QString &var) const;
		void setTarget(const QString &);
		void setId(const QString &);
		void setProperty(const QString &var, const QString &val);

		QDomElement toXml(QDomDocument *) const;
		bool fromXml(QDomElement &);

	private:
		QString v_target, v_id;
		QStringList vars, vals;
	};

	class EncryptionProperties : public QValueList<EncryptionProperty>
	{
	public:
		EncryptionProperties(const QString &s="");

		QString id() const;
		void setId(const QString &);

		QDomElement toXml(QDomDocument *) const;
		bool fromXml(QDomElement &);

	private:
		QString v_id;
	};

	class Encrypted
	{
	public:
		enum Type { Key, Data };
		Encrypted();
		~Encrypted();

		void clear();

		Method method() const { return v_method; }
		QString id() const { return v_id; }
		DataType dataType() const { return v_dataType; }
		Type type() const { return v_type; }
		QString mimeType() const { return v_mimeType; }
		const KeyInfo & keyInfo() const { return v_keyInfo; }
		bool isReference() const { return (!v_cref.uri().isEmpty()); }
		const Reference & dataReference() const { return v_cref; }
		QString carriedKeyName() const { return v_carrykeyname; }
		const ReferenceList & referenceList() const { return v_reflist; }
		const EncryptionProperties & encryptionProperties() const { return v_props; }

		void setId(const QString &id) { v_id = id; }
		void setDataType(DataType t) { v_dataType = t; }
		void setType(Type t) { v_type = t; }
		void setMimeType(const QString &mime) { v_mimeType = mime; }
		void setKeyInfo(const KeyInfo &info) { v_keyInfo = info; }
		void setDataReference(const Reference &cref, Method m);
		void setCarriedKeyName(const QString &s) { v_carrykeyname = s; }
		void setReferenceList(const ReferenceList &rl) { v_reflist = rl; }
		void setEncryptionProperties(const EncryptionProperties &p) { v_props = p; }

		bool encryptData(const QByteArray &data, const Cipher::Key &key);
		bool encryptElement(const QDomElement &data, const Cipher::Key &key);
		bool encryptContent(const QDomElement &data, const Cipher::Key &key);
		bool encryptKey(const Cipher::Key &data, const Cipher::Key &key);

		QByteArray decryptData(const Cipher::Key &key) const;
		QDomElement decryptElement(QDomDocument *, const Cipher::Key &key) const;
		QDomNodeList decryptContent(QDomDocument *, const Cipher::Key &key) const;
		QByteArray decryptKey(const Cipher::Key &key) const;

		QDomElement toXml(QDomDocument *) const;
		bool fromXml(const QDomElement &);

	private:
		Method v_method;
		QString v_id;
		DataType v_dataType;
		Type v_type;
		QString v_mimeType;
		KeyInfo v_keyInfo;

		QString v_cval;
		Reference v_cref;
		QString v_carrykeyname;
		ReferenceList v_reflist;
		EncryptionProperties v_props;

		QString baseNS;
		Method cipherTypeToMethod(Cipher::Type) const;
		QString methodToAlgorithm(Method, Type) const;
		Method algorithmToMethod(const QString &) const;
	};
};

#endif
