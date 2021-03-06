
/*
 *  <SimpleSecureChat Client/Server - E2E encrypted messaging application written in C>
 *  Copyright (C) 2017-2018 The SimpleSecureChat Authors. <kping0> 
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "sscasymmetric.h"

typedef unsigned char byte;

/*
 * This Function point is taken from the OpenSSL wiki -> the LICENSE for the following function is 
 * https://www.openssl.org/source/license.html
 */
int envelope_open(EVP_PKEY *priv_key, unsigned char *ciphertext, int ciphertext_len,
	unsigned char *encrypted_key, int encrypted_key_len, unsigned char *iv,
	unsigned char *plaintext){
	debugprint();
	EVP_CIPHER_CTX *ctx;
	int len;
	int plaintext_len;
	if(!(ctx = EVP_CIPHER_CTX_new())) return 0;
	if(1 != EVP_OpenInit(ctx, EVP_aes_256_cbc(), encrypted_key,
		encrypted_key_len, iv, priv_key))
		return 0;
	if(1 != EVP_OpenUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
		return 0;
	plaintext_len = len;
	if(1 != EVP_OpenFinal(ctx, plaintext + len, &len)) return 0;
	plaintext_len += len;
	EVP_CIPHER_CTX_free(ctx);
	return plaintext_len;
}


/*
 * This Function point is taken from the OpenSSL wiki -> the LICENSE for the following function is 
 * https://www.openssl.org/source/license.html
 */
int envelope_seal(EVP_PKEY **pub_key, unsigned char *plaintext, int plaintext_len,
	unsigned char **encrypted_key, int *encrypted_key_len, unsigned char *iv,
	unsigned char *ciphertext){
	debugprint();
	EVP_CIPHER_CTX *ctx;
	int ciphertext_len;
	int len;
	if(!(ctx = EVP_CIPHER_CTX_new())) return 0;
	if(1 != EVP_SealInit(ctx, EVP_aes_256_cbc(), encrypted_key,
		encrypted_key_len, iv, pub_key, 1))
		return 0;
	if(1 != EVP_SealUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
		return 0;
	ciphertext_len = len;
	if(1 != EVP_SealFinal(ctx, ciphertext + len, &len)) return 0;
	ciphertext_len += len;
	EVP_CIPHER_CTX_free(ctx);
	return ciphertext_len;
}
/*
 * End of functions taken from the OpenSSL wiki, GPL3 Applies for the rest of the functions
 */

int load_keypair(EVP_PKEY* pubKey, EVP_PKEY* privKey,unsigned char* path4pubkey,unsigned char* path4privkey){
	debugprint();
	/*
	* This Function reads the Public&Private key from files into (initialized)EVP_PKEY objects...
	*/
	BIO* rsa_pub_bio = BIO_new_file(path4pubkey,"r");
	if(rsa_pub_bio == NULL){
		cerror("could not load public key!");
		return 0;	
	}
	RSA* rsa_pub = RSA_new();
	PEM_read_bio_RSAPublicKey(rsa_pub_bio,&rsa_pub,NULL,NULL);
	BIO_free(rsa_pub_bio);	
	RSA_blinding_on(rsa_pub,NULL);
	EVP_PKEY_assign_RSA(pubKey,rsa_pub);
	
	BIO* rsa_priv_bio = BIO_new_file(path4privkey,"r");
	if(rsa_priv_bio == NULL){
		cerror("could not load private key!");
		return 0;	
	}
	RSA* rsa_priv = RSA_new();
	PEM_read_bio_RSAPrivateKey(rsa_priv_bio, &rsa_priv,NULL,NULL);
	BIO_free(rsa_priv_bio);
	RSA_blinding_on(rsa_priv,NULL);
	if(RSA_check_key(rsa_priv) <= 0){
		cerror("Invalid Private Key");
		return 0;	
	}
	EVP_PKEY_assign_RSA(privKey,rsa_priv); 
	return 1;

}

void create_keypair(unsigned char* path4pubkey,unsigned char* path4privkey,int keysize){
	debugprint();
    RSA* rsa = RSA_new();
    BIGNUM* prime = BN_new();
    BN_set_word(prime,RSA_F4);
    RSA_generate_key_ex(rsa,keysize,prime,NULL);
    int check_key = RSA_check_key(rsa);
    while (check_key <= 0) {
	cerror("failed to generate RSA key, regenerating...");
	RSA_generate_key_ex(rsa,8192,prime,NULL);
        check_key = RSA_check_key(rsa);
    }
    RSA_blinding_on(rsa, NULL);

    // write out pem-encoded public key ----
    BIO* rsaPublicBio = BIO_new_file(path4pubkey, "w");
    PEM_write_bio_RSAPublicKey(rsaPublicBio, rsa);

    // write out pem-encoded encrypted private key ----
    BIO* rsaPrivateBio = BIO_new_file(path4privkey, "w");
    PEM_write_bio_RSAPrivateKey(rsaPrivateBio, rsa, NULL, NULL, 0, NULL, NULL);

    BIO_free(rsaPublicBio);
    BIO_free(rsaPrivateBio);
    RSA_free(rsa);
    return;
}


int test_keypair(EVP_PKEY* pubk_evp,EVP_PKEY* priv_evp){ //Also an example of how messages could be encrypted
	debugprint();
	//encrypt test	
	unsigned char* msg = calloc(1,100);
	strncpy((char*)msg,"secret  test_message",100);

	unsigned char* ek = calloc(1,EVP_PKEY_size(pubk_evp));
	int ekl = EVP_PKEY_size(pubk_evp); 

	unsigned char* iv = calloc(1,EVP_MAX_IV_LENGTH);
	RAND_poll(); //Seed CGRNG 
	if(RAND_bytes(iv,EVP_MAX_IV_LENGTH) != 1){
		cerror("Error generating IV from CSPRNG");
		return 0;	
	}
	RAND_poll(); //Change Seed for CGRNG
	unsigned char* enc_buf = calloc(1,2000);
	int enc_len = envelope_seal(&pubk_evp,msg,strlen((const char*)msg),&ek,&ekl,iv,enc_buf); //encrypt
	if(enc_len <= 0){
		cerror("could not encrypt test message");
		return 0;	
	}
	unsigned char* dec_buf = calloc(1,2000);
	envelope_open(priv_evp,enc_buf,enc_len,ek,ekl,iv,dec_buf); //decrypt

	if(strncmp((const char*)msg,(const char*)dec_buf,strlen((const char*)msg)) == 0){
		cdebug("Keypair Test Success");
	}
	else{
		cerror("Keypair Test Failed!");
		return 0;
	}
	free(msg);
	free(enc_buf);
	free(dec_buf);
	free(iv);
	free(ek);
	return 1;
}
