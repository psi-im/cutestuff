#ifndef CS_XMLENC_H
#define CS_XMLENC_H

/*
[s3]   <ds:KeyInfo xmlns:ds='http://www.w3.org/2000/09/xmldsig#'>
[s4]     <ds:KeyName>John Smith</ds:KeyName>
[s5]   </ds:KeyInfo>

    [s4] The symmetric key has an associated name "John Smith".

[t03]   <ds:KeyInfo xmlns:ds='http://www.w3.org/2000/09/xmldsig#'>
[t04]     <ds:RetrievalMethod URI='#EK' Type="http://www.w3.org/2001/04/xmlenc#EncryptedKey"/>
[t05]     <ds:KeyName>Sally Doe</ds:KeyName>
[t06]   </ds:KeyInfo>

[t04] ds:RetrievalMethod is used to indicate the location of a key with type &xenc;EncryptedKey.
      The (AES) key is located at '#EK'.
[t05] ds:KeyName provides an alternative method of identifying the key needed to decrypt the CipherData.
      Either or both the ds:KeyName and ds:KeyRetrievalMethod could be used to identify the same key.

Within the same XML document, there existed an EncryptedKey structure that was referenced within [t04]:

  [t09] <EncryptedKey Id='EK' xmlns='http://www.w3.org/2001/04/xmlenc#'>
  [t10]   <EncryptionMethod 
           Algorithm="http://www.w3.org/2001/04/xmlenc#rsa-1_5"/>
  [t11]   <ds:KeyInfo xmlns:ds='http://www.w3.org/2000/09/xmldsig#'>
  [t12]     <ds:KeyName>John Smith</ds:KeyName>
  [t13]   </ds:KeyInfo>
  [t14]   <CipherData><CipherValue>xyzabc</CipherValue></CipherData>
  [t15]   <ReferenceList>
  [t16]     <DataReference URI='#ED'/>
  [t17]   </ReferenceList>
  [t18]   <CarriedKeyName>Sally Doe</CarriedKeyName>
  [t19] </EncryptedKey>
  
[t09] The EncryptedKey element is similar to the EncryptedData element except that the data encrypted is
      always a key value.
[t10] The EncryptionMethod is the RSA public key algorithm.
[t12] ds:KeyName of "John Smith" is a property of the key necessary for decrypting (using RSA) the CipherData.
[t14] The CipherData's CipherValue is an octet sequence that is processed (serialized, encrypted,
      and encoded) by a referring encrypted object's EncryptionMethod. (Note, an EncryptedKey's
      EncryptionMethod is the algorithm used to encrypt these octets and does not speak about what type of
      octets they are.)
[t15-17] A ReferenceList identifies the encrypted objects (DataReference and KeyReference) encrypted with
      this key. The ReferenceList contains a list of references to data encrypted by the symmetric key
      carried within this structure.
[t18] The CarriedKeyName element is used to identify the encrypted key value which may be referenced by
      the KeyName element in ds:KeyInfo. (Since ID attribute values must be unique to a document,
      CarriedKeyName can indicate that several EncryptedKey structures contain the same key value encrypted
      for different recipients.)
*/

// TODO: other properties: ReferenceList, EncryptionProperties
// TODO: other key properties: CarriedKeyName, etc
// TODO: RSA key stuff
// TODO: error checking
// TODO: random generation of data (to replace the static "get this man an iv" and other stupid strings)

#include<qstring.h>
#include<qcstring.h>
#include<qdom.h>
#include"../util/cipher.h"

namespace XmlEnc
{
	enum Method { None, TripleDES, AES_128, AES_256, RSA_15 };
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
		Encrypted();
		~Encrypted();

		void clear();

		Method method() const { return v_method; }
		QString id() const { return v_id; }
		DataType dataType() const { return v_dataType; }
		Type type() const { return v_type; }
		QString mimeType() const { return v_mimeType; }
		KeyInfo keyInfo() const { return v_keyInfo; }
		bool isReference() const { return (!v_cref.isEmpty()); }
		QString dataReference() const { return v_cref; }
		QDomElement dataReferenceTransforms() const { return v_creftrans; }

		void setId(const QString &id) { v_id = id; }
		void setDataType(DataType t) { v_dataType = t; }
		void setType(Type t) { v_type = t; }
		void setMimeType(const QString &mime) { v_mimeType = mime; }
		void setKeyInfo(const KeyInfo &info) { v_keyInfo = info; }
		void setDataReference(const QString &uri, Method m, const QDomElement &transforms=QDomElement());

		bool encryptData(const QByteArray &data, const Cipher::Key &key);
		bool encryptElement(const QDomElement &data, const Cipher::Key &key);
		bool encryptContent(const QDomElement &data, const Cipher::Key &key);
		bool encryptKey(const Cipher::Key &data, const Cipher::Key &key);

		QByteArray decryptData(const Cipher::Key &key) const;
		QDomElement decryptElement(QDomDocument *, const Cipher::Key &key) const;
		QDomNodeList decryptContent(QDomDocument *, const Cipher::Key &key) const;
		Cipher::Key decryptKey(const Cipher::Key &key) const;

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
		QString v_cref;
		QDomElement v_creftrans;

		QString baseNS;
		Method cipherTypeToMethod(Cipher::Type) const;
		QString methodToAlgorithm(Method, Type) const;
		Method algorithmToMethod(const QString &) const;
	};
};

#endif
