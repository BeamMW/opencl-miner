#include "blake2b.h"

void blake2bInstance::init(uint8_t hash_len, uint32_t n, uint32_t k, std::string personal) {
	h[0] = blake2b_iv[0] ^ (0x01010000 | hash_len);

	for (uint32_t i = 1; i <= 5; i++) {
		h[i] = blake2b_iv[i];
	}

	//char personalization[8];
	//strcpy(personalization, personal.c_str()); 

	h[6] = blake2b_iv[6] ^ *(uint64_t *) personal.c_str();
   	h[7] = blake2b_iv[7] ^ (((uint64_t)k << 32) | n);
	
	bytes = 0;
}

static uint64_t rotr64(uint64_t a, uint8_t bits) {
    return (a >> bits) | (a << (64 - bits));
}

static void mix(uint64_t *va, uint64_t *vb, uint64_t *vc, uint64_t *vd,
        uint64_t x, uint64_t y) {
    *va = (*va + *vb + x);
    *vd = rotr64(*vd ^ *va, 32);
    *vc = (*vc + *vd);
    *vb = rotr64(*vb ^ *vc, 24);
    *va = (*va + *vb + y);
    *vd = rotr64(*vd ^ *va, 16);
    *vc = (*vc + *vd);
    *vb = rotr64(*vb ^ *vc, 63);
}

void blake2bInstance::update(const uint8_t *_msg, uint32_t msg_len, uint32_t is_final) {
	const uint64_t *m = (const uint64_t *)_msg;
	uint64_t v[16];

	if (msg_len > 128) return;

	memcpy(v + 0, h, 8 * sizeof (*v));
	memcpy(v + 8, blake2b_iv, 8 * sizeof (*v));
	bytes += msg_len;
    	v[12] ^= bytes;
    	v[14] ^= is_final ? -1 : 0;

	for (uint32_t round = 0; round < blake2b_rounds; round++) {
        	const uint8_t   *s = blake2b_sigma[round];
        	mix(v + 0, v + 4, v + 8,  v + 12, m[s[0]],  m[s[1]]);
        	mix(v + 1, v + 5, v + 9,  v + 13, m[s[2]],  m[s[3]]);
        	mix(v + 2, v + 6, v + 10, v + 14, m[s[4]],  m[s[5]]);
        	mix(v + 3, v + 7, v + 11, v + 15, m[s[6]],  m[s[7]]);
        	mix(v + 0, v + 5, v + 10, v + 15, m[s[8]],  m[s[9]]);
        	mix(v + 1, v + 6, v + 11, v + 12, m[s[10]], m[s[11]]);
        	mix(v + 2, v + 7, v + 8,  v + 13, m[s[12]], m[s[13]]);
        	mix(v + 3, v + 4, v + 9,  v + 14, m[s[14]], m[s[15]]);
	}

	for (uint32_t i = 0; i < 8; i++) {
		h[i] ^= v[i] ^ v[i + 8];
	}
}


void blake2bInstance::ret_final(uint8_t *out, uint32_t outlen){
	memcpy(out, h, outlen);
}

void blake2bInstance::ret_state(uint64_t *out){
	for (uint32_t i = 0; i < 8; i++) {
		out[i] = h[i];
	}
}

blake2bInstance::blake2bInstance(const blake2bInstance &inp) {
	bytes = inp.bytes;
	for (int i=0; i<8; i++) h[i] = inp.h[i];
}

blake2bInstance::blake2bInstance() {
	bytes = 0;
	for (int i=0; i<8; i++) h[i] = 0;
}
