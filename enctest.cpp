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

int main()
{
	QDomDocument doc;
	QDomElement order = doc.createElement("order");
	doc.appendChild(order);

	QDomElement e = doc.createElement("creditcard");
	e.appendChild(doc.createTextNode("1234 5678 9012 3456"));
	order.appendChild(e);
	printf("---- Original\n%s----\n\n", elemToString(order).data());

	Cipher::Key key1 = Cipher::generateKey(Cipher::AES_256);

	Cipher::Key key = Cipher::generateKey(Cipher::AES_256);
	printf("Key: { ");
	for(int n = 0; n < (int)key.data().size(); ++n)
		printf("%02x", (unsigned char)key.data()[n]);
	printf(" }\n\n");

	XmlEnc::Encrypted dat;
	dat.encryptKey(key, key1);
	QDomElement encKey = dat.toXml(&doc);

	printf("---- Encrypted Key\n%s----\n\n", elemToString(encKey).data());

	dat.encryptElement(e, key);
	QDomElement enc = dat.toXml(&doc);
	order.replaceChild(enc, e);
	printf("---- Encrypt\n%s----\n\n", elemToString(order).data());

	XmlEnc::Encrypted de;
	if(!de.fromXml(encKey))
		printf("Error: could not read encrypted xml!\n");
	Cipher::Key realKey = de.decryptKey(key1);

	if(!de.fromXml(enc))
		printf("Error: could not read encrypted xml!\n");

	QDomElement dec = de.decryptElement(&doc, realKey);
	order.replaceChild(dec, enc);
	printf("---- Decrypt\n%s----\n\n", elemToString(order).data());
}
