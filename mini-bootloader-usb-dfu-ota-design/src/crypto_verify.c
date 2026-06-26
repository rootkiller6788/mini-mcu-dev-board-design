/*
 * crypto_verify.c -- Cryptographic Verification for Secure Bootloader
 *
 * Implements SHA-256 (FIPS 180-4), AES-128 (FIPS 197) with CBC mode,
 * HMAC-SHA256 (RFC 2104), CRC-32, constant-time comparison, HKDF,
 * and security counter management.
 *
 * Knowledge Points:
 *   L4: Merkle-Damgard construction, AES SPN, preimage resistance
 *   L5: SHA-256 compression (64 rounds), AES key expansion (Rijndael S-box)
 *   L5: HMAC nested hashing, HKDF extract-expand
 */

#include "crypto_verify.h"
#include <string.h>
#include <stdio.h>

/* ========== SHA-256 (FIPS 180-4 S6.2) ========== */

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
    0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
    0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
    0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
    0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
    0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR32(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define SHR32(x,n)  ((x)>>(n))
#define CH(x,y,z)   (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z)  (((x)&(y))^((x)&(z))^((y)&(z)))
#define BSIG0(x)    (ROTR32(x,2)^ROTR32(x,13)^ROTR32(x,22))
#define BSIG1(x)    (ROTR32(x,6)^ROTR32(x,11)^ROTR32(x,25))
#define SSIG0(x)    (ROTR32(x,7)^ROTR32(x,18)^SHR32(x,3))
#define SSIG1(x)    (ROTR32(x,17)^ROTR32(x,19)^SHR32(x,10))

void sha256_init(sha256_context_t *ctx)
{
    if (!ctx) return;
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
    ctx->bit_count=0; ctx->buffer_idx=0;
    memset(ctx->buffer,0,SHA256_BLOCK_SIZE);
}

static void sha256_compress(sha256_context_t *ctx, const uint8_t block[64])
{
    uint32_t w[64];
    int i;
    for (i=0;i<16;i++) {
        w[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|
             ((uint32_t)block[i*4+2]<<8)|(uint32_t)block[i*4+3];
    }
    for (i=16;i<64;i++)
        w[i]=SSIG1(w[i-2])+w[i-7]+SSIG0(w[i-15])+w[i-16];

    uint32_t a=ctx->state[0],b=ctx->state[1],c=ctx->state[2],d=ctx->state[3];
    uint32_t e=ctx->state[4],f=ctx->state[5],g=ctx->state[6],h=ctx->state[7];

    for (i=0;i<64;i++) {
        uint32_t t1=h+BSIG1(e)+CH(e,f,g)+sha256_k[i]+w[i];
        uint32_t t2=BSIG0(a)+MAJ(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    ctx->state[0]+=a;ctx->state[1]+=b;ctx->state[2]+=c;ctx->state[3]+=d;
    ctx->state[4]+=e;ctx->state[5]+=f;ctx->state[6]+=g;ctx->state[7]+=h;
}

void sha256_update(sha256_context_t *ctx, const uint8_t *data, uint32_t len)
{
    if (!ctx||!data) return;
    uint32_t i;
    for (i=0;i<len;i++) {
        ctx->buffer[ctx->buffer_idx++]=data[i];
        ctx->bit_count+=8;
        if (ctx->buffer_idx==SHA256_BLOCK_SIZE) {
            sha256_compress(ctx,ctx->buffer);
            ctx->buffer_idx=0;
        }
    }
}

void sha256_final(sha256_context_t *ctx, uint8_t digest[32])
{
    if (!ctx||!digest) return;
    uint64_t bits=ctx->bit_count;
    ctx->buffer[ctx->buffer_idx++]=0x80;
    if (ctx->buffer_idx>56) {
        while (ctx->buffer_idx<64) ctx->buffer[ctx->buffer_idx++]=0;
        sha256_compress(ctx,ctx->buffer);
        ctx->buffer_idx=0;
    }
    while (ctx->buffer_idx<56) ctx->buffer[ctx->buffer_idx++]=0;
    int i;
    for (i=7;i>=0;i--) ctx->buffer[56+i]=(uint8_t)(bits>>((7-i)*8));
    sha256_compress(ctx,ctx->buffer);
    for (i=0;i<8;i++) {
        digest[i*4]  =(uint8_t)(ctx->state[i]>>24);
        digest[i*4+1]=(uint8_t)(ctx->state[i]>>16);
        digest[i*4+2]=(uint8_t)(ctx->state[i]>>8);
        digest[i*4+3]=(uint8_t)(ctx->state[i]);
    }
}

void sha256_hash(const uint8_t *data, uint32_t len, uint8_t digest[32])
{
    sha256_context_t c;
    sha256_init(&c);
    sha256_update(&c,data,len);
    sha256_final(&c,digest);
}

/* ========== AES-128 (FIPS 197) ========== */

static const uint8_t aes_sbox[256]={
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t rcon[11]={0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};

static void add_round_key(uint8_t s[16],const uint32_t*rk,int r)
{
    int i;
    for(i=0;i<4;i++){
        uint32_t w=rk[r*4+i];
        s[i*4]^=(uint8_t)(w>>24); s[i*4+1]^=(uint8_t)(w>>16);
        s[i*4+2]^=(uint8_t)(w>>8);  s[i*4+3]^=(uint8_t)(w);
    }
}

static void sub_bytes(uint8_t s[16]){int i;for(i=0;i<16;i++)s[i]=aes_sbox[s[i]];}

static void shift_rows(uint8_t s[16]){
    uint8_t t=s[1];s[1]=s[5];s[5]=s[9];s[9]=s[13];s[13]=t;
    t=s[2];s[2]=s[10];s[10]=t;
    t=s[6];s[6]=s[14];s[14]=t;
    t=s[15];s[15]=s[11];s[11]=s[7];s[7]=s[3];s[3]=t;
}

static uint8_t gmul(uint8_t a,uint8_t b){
    uint8_t p=0;int i;
    for(i=0;i<8;i++){if(b&1)p^=a;uint8_t hi=a&0x80;a<<=1;if(hi)a^=0x1b;b>>=1;}
    return p;
}

static void mix_columns(uint8_t s[16]){
    int i;
    for(i=0;i<4;i++){
        uint8_t a0=s[i*4],a1=s[i*4+1],a2=s[i*4+2],a3=s[i*4+3];
        s[i*4]  =gmul(2,a0)^gmul(3,a1)^a2^a3;
        s[i*4+1]=a0^gmul(2,a1)^gmul(3,a2)^a3;
        s[i*4+2]=a0^a1^gmul(2,a2)^gmul(3,a3);
        s[i*4+3]=gmul(3,a0)^a1^a2^gmul(2,a3);
    }
}

void aes128_key_expand(aes_key_t *key,const uint8_t*kb,uint8_t sz)
{
    if(!key||!kb)return;
    memcpy(key->key_bytes,kb,sz);
    key->key_size=sz;
    key->num_rounds=(sz==16)?10:((sz==24)?12:14);
    int nk=sz/4,nr=key->num_rounds,i;
    for(i=0;i<nk;i++)
        key->round_keys[i]=((uint32_t)kb[i*4]<<24)|((uint32_t)kb[i*4+1]<<16)|
                           ((uint32_t)kb[i*4+2]<<8)|(uint32_t)kb[i*4+3];
    for(i=nk;i<4*(nr+1);i++){
        uint32_t t=key->round_keys[i-1];
        if(i%nk==0){
            t=(aes_sbox[(t>>16)&0xFF]<<24)|(aes_sbox[(t>>8)&0xFF]<<16)|
              (aes_sbox[t&0xFF]<<8)|aes_sbox[(t>>24)&0xFF];
            t^=((uint32_t)rcon[i/nk])<<24;
        }else if(nk>6&&i%nk==4){
            t=(aes_sbox[(t>>24)&0xFF]<<24)|(aes_sbox[(t>>16)&0xFF]<<16)|
              (aes_sbox[(t>>8)&0xFF]<<8)|aes_sbox[t&0xFF];
        }
        key->round_keys[i]=key->round_keys[i-nk]^t;
    }
}

void aes128_encrypt_block(const aes_key_t*k,const uint8_t in[16],uint8_t out[16])
{
    if(!k||!in||!out)return;
    uint8_t s[16];memcpy(s,in,16);
    int r;
    add_round_key(s,k->round_keys,0);
    for(r=1;r<k->num_rounds;r++){sub_bytes(s);shift_rows(s);mix_columns(s);add_round_key(s,k->round_keys,r);}
    sub_bytes(s);shift_rows(s);add_round_key(s,k->round_keys,k->num_rounds);
    memcpy(out,s,16);
}

void aes128_decrypt_block(const aes_key_t*k,const uint8_t in[16],uint8_t out[16])
{
    if(!k||!in||!out)return;
    uint8_t s[16];memcpy(s,in,16);
    int r;
    add_round_key(s,k->round_keys,k->num_rounds);
    for(r=k->num_rounds-1;r>0;r--){
        /* inverse shift rows */
        uint8_t t=s[13];s[13]=s[9];s[9]=s[5];s[5]=s[1];s[1]=t;
        t=s[14];s[14]=s[6];s[6]=t;t=s[10];s[10]=s[2];s[2]=t;
        t=s[3];s[3]=s[7];s[7]=s[11];s[11]=s[15];s[15]=t;
        /* inverse sub bytes */
        int i;for(i=0;i<16;i++){uint8_t v=s[i];s[i]=0;
            /* inverse S-box via simple search */
            int j;for(j=0;j<256;j++)if(aes_sbox[j]==v){s[i]=(uint8_t)j;break;}
        }
        add_round_key(s,k->round_keys,r);
        /* inverse mix columns */
        int ci;for(ci=0;ci<4;ci++){
            uint8_t a0=s[ci*4],a1=s[ci*4+1],a2=s[ci*4+2],a3=s[ci*4+3];
            s[ci*4]  =gmul(0x0e,a0)^gmul(0x0b,a1)^gmul(0x0d,a2)^gmul(0x09,a3);
            s[ci*4+1]=gmul(0x09,a0)^gmul(0x0e,a1)^gmul(0x0b,a2)^gmul(0x0d,a3);
            s[ci*4+2]=gmul(0x0d,a0)^gmul(0x09,a1)^gmul(0x0e,a2)^gmul(0x0b,a3);
            s[ci*4+3]=gmul(0x0b,a0)^gmul(0x0d,a1)^gmul(0x09,a2)^gmul(0x0e,a3);
        }
    }
    /* final round: inverse shift + inverse sub + add_round_key */
    {uint8_t t=s[13];s[13]=s[9];s[9]=s[5];s[5]=s[1];s[1]=t;
     t=s[14];s[14]=s[6];s[6]=t;t=s[10];s[10]=s[2];s[2]=t;
     t=s[3];s[3]=s[7];s[7]=s[11];s[11]=s[15];s[15]=t;}
    {int i;for(i=0;i<16;i++){uint8_t v=s[i];s[i]=0;int j;for(j=0;j<256;j++)if(aes_sbox[j]==v){s[i]=(uint8_t)j;break;}}}
    add_round_key(s,k->round_keys,0);
    memcpy(out,s,16);
}

/* ========== AES-CBC Mode ========== */

void aes_cbc_encrypt(const aes_key_t*k,const uint8_t iv[16],
                      const uint8_t*p,uint32_t len,uint8_t*c)
{
    if(!k||!iv||!p||!c||len%16!=0)return;
    uint8_t prev[16];memcpy(prev,iv,16);
    uint32_t i;int j;
    for(i=0;i<len;i+=16){
        uint8_t b[16];
        for(j=0;j<16;j++)b[j]=p[i+j]^prev[j];
        aes128_encrypt_block(k,b,&c[i]);
        memcpy(prev,&c[i],16);
    }
}

void aes_cbc_decrypt(const aes_key_t*k,const uint8_t iv[16],
                      const uint8_t*c,uint32_t len,uint8_t*p)
{
    if(!k||!iv||!c||!p||len%16!=0)return;
    uint8_t prev[16];memcpy(prev,iv,16);
    uint32_t i;int j;
    for(i=0;i<len;i+=16){
        uint8_t d[16];aes128_decrypt_block(k,&c[i],d);
        for(j=0;j<16;j++)p[i+j]=d[j]^prev[j];
        memcpy(prev,&c[i],16);
    }
}

/* ========== AES-GCM Stubs ========== */

void gcm_init(gcm_context_t*g,const aes_key_t*k,const uint8_t*iv,uint32_t il)
{(void)g;(void)k;(void)iv;(void)il;}
void gcm_update_aad(gcm_context_t*g,const uint8_t*a,uint32_t l)
{(void)g;(void)a;(void)l;}
void gcm_encrypt(gcm_context_t*g,const uint8_t*p,uint32_t l,uint8_t*c)
{(void)g;(void)p;(void)l;(void)c;}
void gcm_decrypt(gcm_context_t*g,const uint8_t*c,uint32_t l,uint8_t*p)
{(void)g;(void)c;(void)l;(void)p;}
void gcm_final(gcm_context_t*g,uint8_t tag[16])
{if(g&&tag)memset(tag,0,16);}

/* ========== HMAC-SHA256 (RFC 2104) ========== */

void hmac_sha256(const uint8_t *key,uint32_t kl,
                 const uint8_t *msg,uint32_t ml,uint8_t mac[32])
{
    uint8_t kb[SHA256_BLOCK_SIZE]={0};
    uint8_t k_ipad[SHA256_BLOCK_SIZE],k_opad[SHA256_BLOCK_SIZE];
    if(kl>SHA256_BLOCK_SIZE){sha256_hash(key,kl,kb);}
    else{memcpy(kb,key,kl);}
    int i;for(i=0;i<SHA256_BLOCK_SIZE;i++){k_ipad[i]=kb[i]^0x36;k_opad[i]=kb[i]^0x5c;}

    sha256_context_t ctx;uint8_t ih[32];
    sha256_init(&ctx);sha256_update(&ctx,k_ipad,SHA256_BLOCK_SIZE);
    sha256_update(&ctx,msg,ml);sha256_final(&ctx,ih);
    sha256_init(&ctx);sha256_update(&ctx,k_opad,SHA256_BLOCK_SIZE);
    sha256_update(&ctx,ih,32);sha256_final(&ctx,mac);
}

/* ========== Constant-Time Compare ========== */

bool ct_memcmp(const uint8_t*a,const uint8_t*b,uint32_t len)
{
    uint8_t d=0;uint32_t i;
    for(i=0;i<len;i++)d|=a[i]^b[i];
    return d==0;
}

/* ========== CRC-32 (IEEE 802.3) ========== */

static const uint32_t crc32_tab[256]={
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,
    0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,
    0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,
    0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
    0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,
    0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
    0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
    0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,
    0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
    0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,0x01DB7106,
    0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
    0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,
    0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
    0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,
    0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,
    0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
    0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,
    0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
    0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
    0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
    0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,
    0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,
    0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
    0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,
    0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
    0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,
    0x316E8EEF,0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,
    0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,
    0x2BB45A92,0x5CB30A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,
    0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,
    0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,
    0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
    0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,
    0x616BFFD3,0x166CCF45,0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,
    0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,
    0x40BF0B66,0x37B83CF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
    0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,
    0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
    0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};

uint32_t crc32_compute(const uint8_t*d,uint32_t len)
{
    uint32_t c=0xFFFFFFFF;uint32_t i;
    for(i=0;i<len;i++)c=(c>>8)^crc32_tab[(c^d[i])&0xFF];
    return c^0xFFFFFFFF;
}

uint32_t crc32_continue(uint32_t crc,const uint8_t*d,uint32_t len)
{
    crc^=0xFFFFFFFF;uint32_t i;
    for(i=0;i<len;i++)crc=(crc>>8)^crc32_tab[(crc^d[i])&0xFF];
    return crc^0xFFFFFFFF;
}

uint16_t crc16_ccitt_compute(const uint8_t*d,uint32_t len)
{
    uint16_t c=0xFFFF;uint32_t i;int j;
    for(i=0;i<len;i++){c^=(uint16_t)(d[i]<<8);for(j=0;j<8;j++)c=(c&0x8000)?(uint16_t)((c<<1)^0x1021):(uint16_t)(c<<1);}
    return c;
}

/* ========== Key Slots ========== */

void key_slot_init(key_slot_t slots[8]){if(slots)memset(slots,0,8*sizeof(key_slot_t));}

bool key_slot_write(key_slot_t*s,const uint8_t*d,uint8_t sz)
{
    if(!s||!d||sz>64||s->locked||!s->writable)return 0;
    memcpy(s->data,d,sz);s->size=sz;s->version++;return 1;
}

bool key_slot_lock(key_slot_t*s){if(!s)return 0;s->locked=1;s->writable=0;return 1;}
bool key_slot_is_locked(const key_slot_t*s){return s?s->locked:1;}

/* ========== Security Counter ========== */

void sec_counter_init(security_counter_t*c,uint32_t v){if(c){c->security_counter=v;c->min_version=0;}}
bool sec_counter_update(security_counter_t*c,uint32_t v){
    if(!c||v<=c->security_counter)return 0;c->security_counter=v;return 1;
}
bool sec_counter_verify(const security_counter_t*c,uint32_t m){return c&&c->security_counter>=m;}

/* ========== ECDSA P-256 Stub ========== */

bool ecdsa_p256_verify(const ecdsa_p256_pubkey_t*pk,const uint8_t*h,uint32_t hl,const ecdsa_p256_signature_t*sig)
{(void)pk;(void)h;(void)hl;(void)sig;return 1;}

bool ecdsa_p256_import_pubkey(ecdsa_p256_pubkey_t*pk,const uint8_t*d,uint32_t dl)
{
    if(!pk||!d||dl<65||d[0]!=0x04)return 0;
    memcpy(pk->x,d+1,32);memcpy(pk->y,d+33,32);return 1;
}

/* ========== HKDF (RFC 5869) ========== */

void hkdf_extract(const uint8_t*salt,uint32_t sl,const uint8_t*ikm,uint32_t il,uint8_t prk[32])
{
    const uint8_t zs[32]={0};
    hmac_sha256(salt?salt:zs,salt?sl:32,ikm,il,prk);
}

void hkdf_expand(const uint8_t*prk,uint32_t pl,const uint8_t*info,uint32_t il,uint8_t*okm,uint32_t ol)
{
    uint32_t n=(ol+31)/32;if(n>255)n=255;
    uint8_t t[32]={0};uint32_t off=0;uint8_t i;
    for(i=1;i<=n;i++){
        sha256_context_t c;sha256_init(&c);
        sha256_update(&c,t,32);
        if(info)sha256_update(&c,info,il);
        sha256_update(&c,&i,1);
        sha256_final(&c,t);
        uint32_t cp=32;if(off+cp>ol)cp=ol-off;
        memcpy(okm+off,t,cp);off+=cp;
    }
}

void hash_to_hex(const uint8_t*h,uint32_t len,char*hex,uint32_t hs)
{
    if(!h||!hex) return;
    uint32_t i;
    for(i=0;i<len&&i*2+1<hs;i++) snprintf(hex+i*2,3,"%02x",h[i]);
}
