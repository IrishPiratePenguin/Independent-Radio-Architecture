// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 lyra XE1POC
// DMR AES-256 voice (core/dmr/dmrAes.c) against a real recording: the archive.org item dmr-encrypted-samples_202511
// (Public Domain Mark 1.0), AES-256.wav, whose key the item publishes, as dmrgo's tests carry its frames (v2/privacy
// testdata/aes256.txt, MIT). Kept here: its PI header and, per frame, only what the cipher XORed onto the 49 voice
// bits (the frame as received XOR as decrypted with the key): the keystream, not the voice.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dmr/dmrAes.h"
#include "dmr/ambe/ambeFec.h"

static void unhex(const char *s, uint8_t *out, unsigned n)
{
	for (unsigned i = 0; i < n; i++) {
		unsigned x;
		assert(sscanf(s + 2 * i, "%2x", &x) == 1);
		out[i] = (uint8_t)x;
	}
}

// Three superframes of the capture, 18 frames each (bursts A-F, three frames a burst)
static const char *const masks[54] = {
	"BC0985CCA8FA80", "8FC2EA1D167D80", "FE9BD71FFF2480", "E2C4DDE336FD00", "D2399152105380", "3176CE8D355880",
	"2E05DEB27AC280", "553BD72EBEA600", "92165F922E9800", "249D55C523E880", "DF22AED5246880", "6F379514519000",
	"BD51AC7B925C80", "DBE0BCBC7F4200", "3079D55181E100", "2FCAC374AEC200", "6993E6262EF500", "629051C1DAB880",
	"A795CF7C842180", "E5EA5B873B0000", "BE9C4A8E1F5A00", "E6582C2EAF4500", "B552921CE2C100", "1608AF925F3100",
	"840165CFA5AE80", "A67CD9C06B3080", "BD9013EFC6C780", "90E323D7FA0680", "8BB6B8DFB95D80", "C4852B6067A080",
	"B69FC45AE7BC00", "11FB0F0880A680", "A2AD1519305D80", "3060E97AB55B80", "52E6DA8EC3F880", "CB62B3D0BCF380",
	"04D09258543C80", "8B536804E27300", "78599360A04600", "0EDEF02227D280", "F25E245F61E480", "9E8508A99B9300",
	"B54F76CAB80100", "DC7FD1321B0580", "B16EB914776880", "89FDD18CDFE880", "49BF4B648DCF00", "3CF329C8B3B980",
	"172CD579B45280", "BB0E10D71C7680", "0DD68D12129580", "A238BB0A0A9080", "A63C0190225600", "F9970EC8B04D00",
};

// From the public RC4-40.wav sample, decoded with DSD-FME (dmrgo v2/privacy/testdata/rc4.txt).
static const char *const arc4_masks[54] = {
	"5372CDF4493B00", "2A0723F7F65100", "397D9C66D9A680", "4E86B08BBAA000", "773193A131EA80", "4D1659190FAC80",
	"1D9F68F2E2AD80", "59F0AB529F1800", "3D861CD358F700", "8DC6C193A13000", "8BFD50FBFD1400", "F4DD5FA1564500",
	"95CADECF1F1700", "74D7E3B9271180", "B7262149D3BC80", "7E7FB8717C8780", "11E2A173E7F800", "505B50274CFB80",
	"FD0DE196292400", "7D684641EEF400", "A179A700A16800", "A3549C5EBFF000", "BB28126FBC2400", "8F17A94389DC00",
	"C9C61EB9607880", "C0990707717500", "959CAEC89AA700", "4E4FBCFFB4DF80", "37B646E0763080", "439CC7767AB380",
	"DE11FE25593200", "03FA2AE8876E00", "79D6658E062B00", "23B0CB4AF1B800", "2577D18F090880", "F98779BE982380",
	"A2843841B65B00", "A97AA35EC11200", "1FB98827191600", "3463B3F6C70600", "EFC63F54527880", "06B51A7BCA2400",
	"D56170E9BAC600", "E12E9AF0A8CB00", "85C74C554FAE80", "1A4A172C88FB80", "7871CB3B55FE80", "8C9433848AE100",
	"A72AF39CAE0080", "397356F5528B00", "6463589CE15B80", "3BEB2C847B1200", "E8DE82DBB3A000", "40E68B9D862900",
};

// A burst whose frames carry the mask of frames n..n+2 as their voice bits: decrypting it must give all-zero bits
static void masked_burst(const char *const *frame_masks, unsigned n, uint8_t v[27])
{
	for (unsigned f = 0; f < 3; f++) {
		uint8_t m[7], bits[AMBE_PARAM_BITS];

		unhex(frame_masks[n + f], m, 7);
		for (unsigned b = 0; b < AMBE_PARAM_BITS; b++)
			bits[b] = (uint8_t)((m[b / 8] >> (7 - b % 8)) & 1u);
		ambeFecEncode(bits, &v[9 * f]);
	}
}

static unsigned zero_frames(const uint8_t v[27])
{
	unsigned z = 0;

	for (unsigned f = 0; f < 3; f++) {
		uint8_t bits[AMBE_PARAM_BITS], zero[AMBE_PARAM_BITS] = { 0 };

		ambeFecDecode(&v[9 * f], bits, NULL);
		z += memcmp(bits, zero, sizeof bits) == 0;
	}
	return z;
}

static void test_arc4_capture(void)
{
	uint8_t key[DMR_ARC4_KEY_BYTES], pi[12], built[DMR_AES_PI_BYTES], v[27], kid;
	uint32_t mi;
	dmr_aes_t a;

	unhex("373374ABE8", key, sizeof key);
	unhex("21100F531699C40000010810", pi, sizeof pi);
	assert(dmrVoiceCipherPiParse(pi, DMR_ARC4_ALG, &kid, &mi) && kid == 15 && mi == 0x531699C4u);
	dmrVoiceCipherPiBuild(DMR_ARC4_ALG, kid, mi, 1, built);
	assert(!memcmp(pi, built, sizeof built));
	assert(!dmrAesPiParse(pi, &kid, &mi));
	assert(dmrVoiceCipherNextMi(DMR_ARC4_ALG, mi) == 0x178D0B9Bu);
	assert(dmrVoiceCipherNextMi(DMR_ARC4_ALG, 0x178D0B9Bu) == 0x7F900958u);
	built[0] = 1;
	assert(dmrVoiceCipherPiParse(built, DMR_ARC4_ALG, &kid, &mi));
	built[1] = 0x68;
	assert(!dmrVoiceCipherPiParse(built, DMR_ARC4_ALG, &kid, &mi));

	for (unsigned skip = 0; skip < 2; skip++) {
		dmrVoiceCipherStart(&a, DMR_ARC4_ALG, key, mi);
		for (unsigned n = 0; n < 54; n += 3) {
			if (skip && n >= 9 && n < 39)
				continue;	// a whole superframe and surrounding bursts missing
			masked_burst(arc4_masks, n, v);
			assert(dmrAesBurst(&a, v, (uint8_t)(n / 3 % 6), 100 + 2 * n / 3, NULL));
			assert(zero_frames(v) == 3);
		}
	}
	puts("dmrAesTest: three DSD-FME ARC4-40 superframes, matching PI layout, and loss recovery passed");
}

int main(void)
{
	test_arc4_capture();
	static const uint8_t silence[9] = { 0xB9, 0xE8, 0x81, 0x52, 0x61, 0x73, 0x00, 0x2A, 0x6B };
	uint8_t key[32], pi[12], v[27], kid;
	uint32_t mi;
	dmr_aes_t a;

	unhex("3AF8C96E21BEFDE927EE1D03DF53CFA701516B0642AC7B38643549E2BE79840E", key, 32);
	unhex("25100E05D30BF20000013D0C", pi, 12);	// the capture's PI header (CRC included)
	assert(dmrAesPiParse(pi, &kid, &mi) && kid == 14 && mi == 0x05D30BF2u);
	{
		uint8_t again[DMR_AES_PI_BYTES];

		dmrAesPiBuild(kid, mi, 1, again);
		assert(memcmp(again, pi, DMR_AES_PI_BYTES) == 0);
		again[0] = 0x05;	// AES-256 as some makers number it
		assert(dmrAesPiParse(again, &kid, &mi));
		again[0] = 0x24;	// AES-128: not ours
		assert(!dmrAesPiParse(again, &kid, &mi));
		again[0] = 0x25;
		again[1] = 0x68;	// another maker's layout
		assert(!dmrAesPiParse(again, &kid, &mi));
	}

	// Three superframes in order, without a slot counter: the index wrap steps the MI
	dmrAesStart(&a, key, mi);
	for (unsigned n = 0; n < 54; n += 3) {
		masked_burst(masks, n, v);
		assert(dmrAesBurst(&a, v, (uint8_t)(n / 3 % 6), 0, silence));
		assert(zero_frames(v) == 3);
	}

	// The second superframe lost whole: the slot counter (a burst every other tick) tells, the third still decrypts
	dmrAesStart(&a, key, mi);
	for (unsigned n = 0; n < 18; n += 3) {
		masked_burst(masks, n, v);
		assert(dmrAesBurst(&a, v, (uint8_t)(n / 3), 100 + 2 * n / 3, silence));
	}
	for (unsigned n = 36; n < 54; n += 3) {
		masked_burst(masks, n, v);
		assert(dmrAesBurst(&a, v, (uint8_t)(n / 3 % 6), 100 + 2 * n / 3, silence));
		assert(zero_frames(v) == 3);
	}

	// A frame that is silence on air stays silence; a channel error in C0 is corrected before decrypting
	dmrAesStart(&a, key, mi);
	masked_burst(masks, 0, v);
	memcpy(&v[9], silence, 9);
	v[0] ^= 0x80;
	assert(dmrAesBurst(&a, v, 0, 0, silence));
	assert(memcmp(&v[9], silence, 9) == 0 && zero_frames(v) == 2);

	// TX then RX with the same key and MI: the frames come back
	{
		dmr_aes_t tx, rx;
		uint8_t plain[27], air[27];

		for (unsigned i = 0; i < 27; i++)
			plain[i] = (uint8_t)(i * 37 + 5);
		for (unsigned f = 0; f < 3; f++) {	// whole FEC words, as the encoder makes them
			uint8_t bits[AMBE_PARAM_BITS];

			ambeFecDecode(&plain[9 * f], bits, NULL);
			ambeFecEncode(bits, &plain[9 * f]);
		}
		dmrAesStart(&tx, key, 0x12345678u);
		dmrAesStart(&rx, key, 0x12345678u);
		for (unsigned b = 0; b < 20; b++) {
			memcpy(air, plain, 27);
			assert(dmrAesBurst(&tx, air, (uint8_t)(b % 6), 0, NULL));
			assert(memcmp(air, plain, 27) != 0);
			assert(dmrAesBurst(&rx, air, (uint8_t)(b % 6), 0, silence));
			assert(memcmp(air, plain, 27) == 0);
		}
	}

	// Late entry: a superframe's fragments carry the NEXT superframe's MI in its AMBE bits, and decode to it
	{
		uint8_t frag[6][3], got[6][3];
		uint32_t mi2;

		dmrAesLeBuild(dmrAesMiNext(mi), frag);
		assert(dmrAesLeDecode(frag, &mi2) && mi2 == dmrAesMiNext(mi));
		int bad = 0;

		for (uint32_t t = 0; t < 512; t++) {
			uint32_t m = 0x9E3779B9u * (t + 1) ^ 0x12345678u;

			dmrAesLeBuild(m, frag);
			if (!dmrAesLeDecode(frag, &mi2) || mi2 != m)
				bad++;
			// One, two and three bit errors in any one Golay word still decode; four never give a wrong MI
			for (int w = 0; w < 3; w++) {
				for (int nerr = 1; nerr <= 3; nerr++) {
					memcpy(got, frag, sizeof got);
					for (int e = 0; e < nerr; e++)
						got[e][w] ^= (uint8_t)(1u << e);
					if (!dmrAesLeDecode(got, &mi2) || mi2 != m)
						bad++;
				}
				memcpy(got, frag, sizeof got);
				for (int e = 0; e < 4; e++)
					got[e][w] ^= (uint8_t)(1u << e);
				if (dmrAesLeDecode(got, &mi2) && mi2 != m)
					bad++;
			}
		}
		assert(bad == 0);

		// Stuffing and collecting are inverses, and only touch the four late-entry bits of each frame
		uint8_t burst[27], plain[27];

		for (int i = 0; i < 27; i++)
			plain[i] = (uint8_t)(i * 37 + 5);
		memcpy(burst, plain, 27);
		dmrAesLeStuff(burst, 3, frag);
		for (int f = 0; f < 3; f++) {
			for (int i = 0; i < 7; i++)
				assert(burst[9 * f + i] == plain[9 * f + i]);
			assert((burst[9 * f + 7] & ~0x11u) == (plain[9 * f + 7] & ~0x11u));
			assert((burst[9 * f + 8] & ~0x11u) == (plain[9 * f + 8] & ~0x11u));
		}
		dmrAesLeCollect(burst, 3, got);
		assert(memcmp(got[3], frag[3], sizeof frag[3]) == 0);
	}

	// No key or MI: no stream; a gap too long stops it
	dmrAesStart(&a, key, 0);
	assert(!dmrAesBurst(&a, v, 0, 0, silence));
	dmrAesStart(&a, key, mi);
	assert(dmrAesBurst(&a, v, 0, 10, silence));
	assert(!dmrAesBurst(&a, v, 0, 10 + 12 * (DMR_AES_MAX_GAP + 1), silence) && !a.ready);
	puts("dmrAesTest: PI header, 3 superframes of a real AES-256 recording, a lost superframe, TX/RX, late entry passed");
	return 0;
}
