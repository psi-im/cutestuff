#include<qdom.h>
#include<qtextstream.h>
#include<stdio.h>

#include"util/cipher.h"
#include"xmlsec/xmlenc.h"

static QCString elemToString(const QDomElement &e)
{
	QString out;
	QTextStream ts(&out, IO_WriteOnly);
	e.save(ts, 1);
	return out.utf8();
}

char txt[]=
	"flen must be less than RSA_size(rsa) - 11 for the PKCS #1\n"
	"v1.5 based padding modes, and less than RSA_size(rsa) - 41\n"
	"for RSA_PKCS1_OAEP_PADDING. The random number generator\n"
	"must be seeded prior to calling RSA_public_encrypt().\n"
	"\n"
	"RSA_private_decrypt() decrypts the flen bytes at from\n"
	"using the private key rsa and stores the plaintext in to.\n"
	"to must point to a memory section large enough to hold the\n"
	"decrypted data (which is smaller than RSA_size(rsa)).\n"
	"padding is the padding mode that was used to encrypt the\n"
	"data.\n";

int main()
{
	QDomDocument doc;
	QDomElement order = doc.createElement("order");
	doc.appendChild(order);

	QDomElement e = doc.createElement("creditcard");
	e.appendChild(doc.createTextNode("1234 5678 9012 3456"));
	order.appendChild(e);
	printf("---- Original\n%s----\n\n", elemToString(order).data());

	Cipher::Type mainKeyType = Cipher::AES_256;
	Cipher::Type subKeyType = Cipher::AES_256;

	RSAKey rsa = generateRSAKey();
	if(rsa.isNull()) {
		printf("RSA key generation failed!\n");
		return 0;
	}
	bool ok;
	QCString cs = "Hello.";
	printf("cs[%d]\n", cs.size());
	QByteArray result = encryptRSA(cs, rsa, &ok);
	if(!ok) {
		printf("Unable to RSA encrypt!\n");
		return 0;
	}
	printf("result[%d]\n", result.size());
	QByteArray result2 = decryptRSA(result, rsa, &ok);
	if(!ok) {
		printf("Unable to RSA decrypt!\n");
		return 0;
	}
	printf("result2[%d]\n", result2.size());
	printf("{%s}\n", result2.data());

	Cipher::Key key1 = Cipher::generateKey(mainKeyType);
	Cipher::Key key = Cipher::generateKey(subKeyType);
	printf("Key: { ");
	for(int n = 0; n < (int)key.data().size(); ++n)
		printf("%02x", (unsigned char)key.data()[n]);
	printf(" }\n\n");

	XmlEnc::Encrypted dat;
	dat.encryptKey(key, key1);
	QDomElement encKey = dat.toXml(&doc);

	//printf("---- Encrypted Key\n%s----\n\n", elemToString(encKey).data());

	XmlEnc::KeyInfo ki;
	ki.setName("My Key");
	//ki.attachEncryptedKey(encKey);
	ki.setValue(key.data());

	dat.setKeyInfo(ki);
	dat.encryptElement(e, key);
	QDomElement enc = dat.toXml(&doc);
	order.replaceChild(enc, e);
	printf("---- Encrypt\n%s----\n\n", elemToString(order).data());

	XmlEnc::Encrypted de;
	if(!de.fromXml(encKey))
		printf("Error: could not read encrypted xml!\n");
	QByteArray r = de.decryptKey(key1);

	Cipher::Key realKey;
	realKey.setData(r);
	realKey.setType(subKeyType);

	if(!de.fromXml(enc))
		printf("Error: could not read encrypted xml!\n");

	QDomElement dec = de.decryptElement(&doc, realKey);
	order.replaceChild(dec, enc);
	printf("---- Decrypt\n%s----\n\n", elemToString(order).data());
}
