#ifndef CS_XMLENC_H
#define CS_XMLENC_H

/*
<element name='EncryptedKey' type='xenc:EncryptedKeyType'/>
  <complexType name='EncryptedKeyType'>
    <complexContent>
      <extension base='xenc:EncryptedType'>
        <sequence>
          <element ref='xenc:ReferenceList' minOccurs='0'/>
          <element name='CarriedKeyName' type='string' minOccurs='0'/>
        </sequence>
        <attribute name='Recipient' type='string' use='optional'/>
      </extension>
    </complexContent>   
  </complexType>

<EncryptedKey  CarriedKeyName="Muhammad Imran" xmlns='http://www.w3.org/2001/04/xmlenc#'>
                <ds:KeyInfo xmlns:ds='http://www.w3.org/2000/09/xmldsig#'>
                        <ds:KeyValue>1asd25fsdf2dfdsfsdfds2f1sd23</ds:KeyValue>
                </ds:KeyInfo>
        </EncryptedKey>
<EncryptedKey  CarriedKeyName="Imran Ali" xmlns='http://www.w3.org/2001/04/xmlenc#'>
                 <EncryptionMethod   Algorithm= "http://www.w3.org/2001/04/xmlenc#rsa-1_5"/>
                        <CipherData>
                                <CipherValue>xyza21212sdfdsfs7989fsdbc</CipherValue>
                        </CipherData>   
        </EncryptedKey>

*/

// NO CipherReference, ReferenceList, EncryptionProperties, RSA_OAEP
// TODO: symmetric keywrap should be 40 bytes not 56!!
// TODO: data/content encrypt and decrypt, AES128/256

#include<qstring.h>
#include<qcstring.h>
#include<qdom.h>
#include"../util/cipher.h"

namespace XmlEnc
{
	enum Method { TripleDES, AES_128, AES_256, RSA_15 };
	enum DataType { Arbitrary, Element, Content };

	struct KeyInfo
	{
		QString name;
		QString value;
	};

	class Encrypted
	{
	public:
		enum Type { Key, Data };
		Encrypted() {}
		virtual ~Encrypted() {}

		Method method() const { return v_method; }
		QString id() const { return v_id; }
		DataType dataType() const { return v_dataType; }
		Type type() const { return v_type; }
		QString mimeType() const { return v_mimeType; }
		KeyInfo keyInfo() const { return v_keyInfo; }
		void setId(const QString &id) { v_id = id; }
		void setType(Type t) { v_type = t; }
		void setKeyInfo(const KeyInfo &info) { v_keyInfo = info; }

		void setData(const QString &data, const QString &mimeType="");
		void setElement(const QDomElement &);
		void setContent(const QDomElement &);
		void setKey(const Cipher::Key &);

		QDomElement encrypt(QDomDocument *, const Cipher::Key &) const;

		bool fromXml(const QDomElement &);
		QByteArray decryptData(const Cipher::Key &key) const;
		QDomElement decryptElement(QDomDocument *, const Cipher::Key &key) const;
		QDomNodeList decryptContent(QDomDocument *, const Cipher::Key &key) const;
		Cipher::Key decryptKey(const Cipher::Key &key) const;

	private:
		Method v_method;
		QString v_id;
		DataType v_dataType;
		Type v_type;
		QString v_mimeType;
		KeyInfo v_keyInfo;
		QString v_data;
		QDomElement v_elem;
		QString v_cval;
		Cipher::Key v_key;
	};
};

#endif
