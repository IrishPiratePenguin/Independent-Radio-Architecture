// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Copyright (c) 2026 lyra XE1POC (changes)
// YSF in the codeplug (core/ysf/ysf_codeplug.c, docs/spec/ysf-codeplug.md) on a simulated NOR flash: the shared
// vectors (tests/vectors/channel-modes.txt, also read by the Codeplug Editor's tests), defaults, zeroed and short
// blocks, a channel's set-up from its tag, the radio-wide record and its record, and a channel written from the radio
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codeplug/codeplug.h"
#include "codeplug/codeplug_kv.h"
#include "codeplug/codeplug_tag.h"
#include "codeplug/codeplug_records.h"
#include "ysf/ysf_codeplug.h"
#include "storage/storage.h"
#include "sim/sim_flash.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

void storage_lock(void) {}
void storage_unlock(void) {}

// The board's air interface (core/radio/radio_ysf.c in the firmware): whether the radio runs kind 6 here
static bool fits;
bool radio_ysf_fits(void) { return fits; }

#define FLASH_SIZE	0x200000u

static const hal_flash_map_t map = {
	.settings_addr = 0x11000u, .settings_size = 0x8000u, .journal_addr = 0x19000u,
	.openfw_eeprom = 0, .openfw_flash = 0x20000u, .openfw_end = FLASH_SIZE,
	.calibration_addr = 0x10000u, .calibration_size = 0x200u,
};

static band_t bands[] = { { 136000000, 174000000, true }, { 400000000, 900000000, true } };

const board_t board = { .name = "test", .bands = bands, .band_count = 2, .power_count = 9, .flash = &sim_flash };

static void fresh(void)
{
	sim_flash_create(FLASH_SIZE, &map);
	CHECK(storage_init());
	codeplug_init();
}

static int unhex(const char *s, uint8_t *out, int max)
{
	int n = 0;

	if (strcmp(s, "-") == 0)
		return 0;
	while (s[0] != '\0' && s[1] != '\0' && n < max) {
		char b[3] = { s[0], s[1], 0 };
		out[n++] = (uint8_t)strtoul(b, NULL, 16);
		s += 2;
	}
	return n;
}

static uint32_t h(const char *s) { return (uint32_t)strtoul(s, NULL, 16); }

static const char *call(const char *s) { return strcmp(s, "-") == 0 ? "" : s; }

// The shared vectors of kind 6 against the codec
static void test_vectors(void)
{
	FILE *f = fopen("../vectors/channel-modes.txt", "r");
	char line[256];
	int counted = 0;

	CHECK(f != NULL);
	if (f == NULL)
		return;
	while (fgets(line, sizeof line, f) != NULL) {
		char *w[8] = { 0 };
		char *rest = NULL;
		uint8_t b[64];
		int n = 0;

		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '#' || line[0] == '\0')
			continue;
		for (char *p = strtok_r(line, " ", &rest); p != NULL && n < 8; p = strtok_r(NULL, " ", &rest))
			w[n++] = p;
		if (strcmp(w[0], "ysfconv") == 0) {
			ysf_conv_t c;
			ysf_conv_decode(b, unhex(w[1], b, sizeof b), &c);
			CHECK(c.dt == h(w[2]) && c.narrow == (h(w[3]) != 0) && c.rx_dgid == h(w[4]) && c.tx_dgid == h(w[5]) &&
			      strcmp(c.dest, w[6]) == 0);
		} else if (strcmp(w[0], "ysfenc") == 0) {
			ysf_conv_t c = { (uint8_t)h(w[1]), h(w[2]) != 0, (uint8_t)h(w[3]), (uint8_t)h(w[4]), "" };
			uint8_t e[YSF_CP_CONV_LEN];
			strcpy(c.dest, w[5]);
			ysf_conv_encode(&c, e);
			CHECK(unhex(w[6], b, sizeof b) == YSF_CP_CONV_LEN && memcmp(e, b, YSF_CP_CONV_LEN) == 0);
		} else if (strcmp(w[0], "ysfident") == 0) {
			ysf_identity_t id;
			ysf_identity_decode(b, unhex(w[1], b, sizeof b), &id);
			CHECK(strcmp(id.src, call(w[2])) == 0 && strcmp(id.down, call(w[3])) == 0 &&
			      strcmp(id.up, call(w[4])) == 0);
		} else {
			continue;
		}
		counted++;
	}
	fclose(f);
	CHECK(counted >= 20);
}

static channel_t make(const char *name, uint32_t rx, uint32_t tx)
{
	channel_t c;

	memset(&c, 0, sizeof c);
	strncpy(c.name, name, CODEPLUG_NAME_LEN);
	c.rx_hz = rx;
	c.tx_hz = tx;
	c.mode = RADIO_MODE_DMR;
	c.rx_only = true;
	c.power = CODEPLUG_POWER_DEFAULT;
	c.color_code = 1;
	c.timeslot = 1;
	c.system = CODEPLUG_SYSTEM_NONE;
	return c;
}

static void test_conv_load(void)
{
	ysf_conv_t yc = { .dt = YSF_DT_VD_MODE2, .narrow = true, .rx_dgid = 127, .tx_dgid = 5, .dest = "XE1ARS" };
	uint8_t blk[YSF_CP_CONV_LEN];
	codeplug_tag_t t = { .kind = CODEPLUG_KIND_YSF, .system = CODEPLUG_SYSTEM_NONE, .flags = CODEPLUG_TAG_TX };
	ysf_conv_cfg_t cfg;
	channel_t c;

	fresh();
	c = make("YSF Ops", 439250000, 434250000);
	CHECK(codeplug_set_channel(10, &c));
	ysf_conv_encode(&yc, blk);
	CHECK(codeplug_tag_write(10, "YSF Ops", &t, blk, sizeof blk));
	CHECK(codeplug_channel(10, &c) && c.kind == CODEPLUG_KIND_YSF && !c.rx_only);
	// Run where the board's air interface carries YSF, parked elsewhere, whatever the codec says
	fits = false;
	CHECK(!codeplug_kind_built(CODEPLUG_KIND_YSF) && strcmp(codeplug_kind_name(c.kind), "YSF") == 0);
	fits = true;
	CHECK(codeplug_kind_built(CODEPLUG_KIND_YSF));

	CHECK(ysf_conv_load(10, &c, &cfg));
	CHECK(cfg.conv.dt == YSF_DT_VD_MODE2 && cfg.conv.narrow && cfg.conv.rx_dgid == 127 && cfg.conv.tx_dgid == 5 &&
	      strcmp(cfg.conv.dest, "XE1ARS") == 0);
	// No radio-wide record yet: the call signs are empty
	CHECK(cfg.id.src[0] == '\0' && cfg.id.down[0] == '\0' && cfg.id.up[0] == '\0');
	// The radio-wide record: the source, downlink and uplink call signs
	{
		uint8_t rec[YSF_CP_IDENTITY_LEN];

		memset(rec, ' ', sizeof rec);
		memcpy(&rec[0], "XE1ARS", 6);
		memcpy(&rec[10], "XE1ARS", 6);
		memcpy(&rec[20], "XE1RPT", 6);
		CHECK(codeplug_kv_set((uint16_t)(CODEPLUG_KEY_MODE_SET + CODEPLUG_KIND_YSF), rec, sizeof rec));
		CHECK(ysf_conv_load(10, &c, &cfg) && strcmp(cfg.id.src, "XE1ARS") == 0 &&
		      strcmp(cfg.id.down, "XE1ARS") == 0 && strcmp(cfg.id.up, "XE1RPT") == 0);
		// A short record leaves the rest empty
		CHECK(codeplug_kv_set((uint16_t)(CODEPLUG_KEY_MODE_SET + CODEPLUG_KIND_YSF), rec, 10));
		CHECK(ysf_conv_load(10, &c, &cfg) && strcmp(cfg.id.src, "XE1ARS") == 0 && cfg.id.down[0] == '\0' &&
		      cfg.id.up[0] == '\0');
		// A record of all spaces reads empty
		memset(rec, ' ', sizeof rec);
		CHECK(codeplug_kv_set((uint16_t)(CODEPLUG_KEY_MODE_SET + CODEPLUG_KIND_YSF), rec, sizeof rec));
		CHECK(ysf_conv_load(10, &c, &cfg) && cfg.id.src[0] == '\0' && cfg.id.down[0] == '\0' &&
		      cfg.id.up[0] == '\0');
	}

	// A tag without a block: the defaults, receive only
	c = make("Zeroed", 439300000, 439300000);
	CHECK(codeplug_set_channel(11, &c));
	t.flags = 0;
	CHECK(codeplug_tag_write(11, "Zeroed", &t, NULL, 0));
	CHECK(codeplug_channel(11, &c) && c.kind == CODEPLUG_KIND_YSF && c.rx_only);
	CHECK(ysf_conv_load(11, &c, &cfg) && cfg.conv.dt == YSF_DT_VD_MODE1 && !cfg.conv.narrow &&
	      cfg.conv.rx_dgid == 0 && cfg.conv.tx_dgid == 0 && strcmp(cfg.conv.dest, YSF_CP_DEST_ALL) == 0);
	// A longer block from a newer writer: the fields known read as usual
	uint8_t longer[16] = { 0x06, 0x7F, 0x85, 'X', 'E', '1', 'A', 'R', 'S', ' ', ' ', ' ', ' ', 1, 2, 3 };
	t.flags = CODEPLUG_TAG_TX;
	CHECK(codeplug_tag_write(11, "Zeroed", &t, longer, sizeof longer));
	CHECK(codeplug_channel(11, &c) && ysf_conv_load(11, &c, &cfg) && cfg.conv.dt == YSF_DT_VD_MODE2 &&
	      cfg.conv.narrow && cfg.conv.rx_dgid == 127 && cfg.conv.tx_dgid == 5 &&
	      strcmp(cfg.conv.dest, "XE1ARS") == 0);
	// Another kind's channel, or a plain one: no set-up
	channel_t plain = make("Plain", 439400000, 439400000);
	CHECK(codeplug_set_channel(12, &plain) && codeplug_channel(12, &plain) && !ysf_conv_load(12, &plain, &cfg));
	t.kind = CODEPLUG_KIND_P25;
	plain = make("P25", 439500000, 439500000);
	CHECK(codeplug_set_channel(13, &plain) && codeplug_tag_write(13, "P25", &t, NULL, 0));
	CHECK(codeplug_channel(13, &plain) && plain.kind == CODEPLUG_KIND_P25 && !ysf_conv_load(13, &plain, &cfg));
}

// A YSF channel written from the radio (2.4): the safe stand-in first, then a new tag; an edit after lets it follow
static void test_conv_save(void)
{
	ysf_conv_t yc = { .dt = YSF_DT_VD_MODE1, .rx_dgid = 12, .tx_dgid = 13, .dest = "CQ" };
	codeplug_tag_t t = { .kind = 0x77, .system = CODEPLUG_SYSTEM_NONE, .flags = CODEPLUG_TAG_TX };
	ysf_conv_cfg_t back;
	codeplug_tag_t rt;
	channel_t c, r, old;

	fresh();
	c = make("Save", 439250000, 434250000);
	c.mode = RADIO_MODE_YSF;	// as the radio core runs it
	c.rx_only = false;
	// Over a plain channel with a stale tag of another kind at the number: the tag is replaced
	old = make("Old", 439250000, 439250000);
	CHECK(codeplug_set_channel(30, &old));
	CHECK(codeplug_tag_write(30, "Old", &t, NULL, 0));
	CHECK(ysf_conv_save(30, &c, &yc));
	CHECK(codeplug_channel(30, &r) && r.kind == CODEPLUG_KIND_YSF && !r.rx_only && r.mode == RADIO_MODE_DMR);
	CHECK(codeplug_tag_read(30, &rt) && rt.kind == CODEPLUG_KIND_YSF && rt.system == CODEPLUG_SYSTEM_NONE &&
	      (rt.flags & CODEPLUG_TAG_TX) != 0 && (rt.flags & CODEPLUG_TAG_BASE_OWN) == 0);
	CHECK(ysf_conv_load(30, &r, &back) && back.conv.dt == YSF_DT_VD_MODE1 && back.conv.rx_dgid == 12 &&
	      back.conv.tx_dgid == 13 && strcmp(back.conv.dest, "CQ") == 0);
	// The record underneath is the stand-in: a reader that knows no tags sees DMR, receive only
	codeplug_tag_remove(30);
	CHECK(codeplug_channel(30, &r) && r.kind == CODEPLUG_KIND_NONE && r.mode == RADIO_MODE_DMR && r.rx_only);
	CHECK(ysf_conv_save(30, &c, &yc) && codeplug_channel(30, &r) && r.kind == CODEPLUG_KIND_YSF);
	// Renamed from the radio: the tag follows, its block kept
	strcpy(r.name, "Saved");
	r.rx_only = true;
	CHECK(codeplug_set_channel(30, &r) && codeplug_channel(30, &r) && r.kind == CODEPLUG_KIND_YSF && r.rx_only);
	CHECK(ysf_conv_load(30, &r, &back) && back.conv.rx_dgid == 12);
	// A copy at another number takes the tag
	CHECK(codeplug_set_channel(31, &r) && codeplug_tag_copy(30, 31, &r));
	CHECK(codeplug_channel(31, &r) && r.kind == CODEPLUG_KIND_YSF && ysf_conv_load(31, &r, &back) &&
	      back.conv.tx_dgid == 13);
	// Receive only from the radio: the tag says so
	c.rx_only = true;
	CHECK(ysf_conv_save(32, &c, &yc) && codeplug_channel(32, &r) && r.kind == CODEPLUG_KIND_YSF && r.rx_only);
	// The block edited on a channel that is YSF already (the channel editor's DG-ID, data type and destination):
	// the tag keeps its system, its fallback, its other flags and what its block holds after this firmware's
	{
		uint8_t longer[YSF_CP_CONV_LEN + 3];
		uint8_t raw[sizeof longer + CODEPLUG_TAG_HEAD_MAX];
		codeplug_tag_t kt = { .kind = CODEPLUG_KIND_YSF, .system = 2, .fb_chan = 30, .fb_check = 0x1234,
				      .flags = CODEPLUG_TAG_FALLBACK | 0x40u };
		ysf_conv_t ed = { .dt = YSF_DT_VD_MODE2, .narrow = true, .rx_dgid = 21, .tx_dgid = 22, .dest = "XE1ARS" };
		channel_t base = make("Keep", 439250000, 434250000);

		ysf_conv_encode(&yc, longer);
		memcpy(&longer[YSF_CP_CONV_LEN], "\xA1\xA2\xA3", 3);
		CHECK(codeplug_set_channel(33, &base) && codeplug_tag_write(33, "Keep", &kt, longer, sizeof longer));
		CHECK(codeplug_channel(33, &r) && r.kind == CODEPLUG_KIND_YSF && r.system == 2 && r.rx_only);
		strcpy(r.name, "Kept");
		r.rx_only = false;
		r.mode = RADIO_MODE_YSF;
		CHECK(ysf_conv_save(33, &r, &ed));
		CHECK(codeplug_channel(33, &r) && r.kind == CODEPLUG_KIND_YSF && r.system == 2 && !r.rx_only &&
		      r.mode == RADIO_MODE_DMR && strcmp(r.name, "Kept") == 0);
		CHECK(codeplug_tag_read(33, &rt) && rt.flags == (CODEPLUG_TAG_FALLBACK | CODEPLUG_TAG_TX | 0x40u) &&
		      rt.fb_chan == 30 && rt.fb_check == 0x1234 && rt.len == CODEPLUG_TAG_HEAD_MAX + sizeof longer);
		CHECK(ysf_conv_load(33, &r, &back) && back.conv.dt == YSF_DT_VD_MODE2 && back.conv.narrow &&
		      back.conv.rx_dgid == 21 && back.conv.tx_dgid == 22 && strcmp(back.conv.dest, "XE1ARS") == 0);
		CHECK(codeplug_tag_params(33, &r, raw, sizeof longer) == (int)sizeof longer &&
		      memcmp(&raw[YSF_CP_CONV_LEN], "\xA1\xA2\xA3", 3) == 0);
	}
	// A mixed channel: its record is an FM channel of its own (wide, transmitting), its tag the YSF side (receive
	// only). The DG-ID edited on the radio, which holds the channel as it runs (YSF, narrow, the tag's receive only):
	// the FM side stays as stored, and so does the tag's flag that says the record is its own
	{
		uint8_t blk[YSF_CP_CONV_LEN];
		codeplug_tag_t mt = { .kind = CODEPLUG_KIND_YSF, .flags = CODEPLUG_TAG_BASE_OWN };
		ysf_conv_t ed = { .dt = YSF_DT_VD_MODE2, .narrow = true, .rx_dgid = 21, .tx_dgid = 22, .dest = "XE1ARS" };
		channel_t fm = make("Mixed", 439300000, 434300000);

		fm.mode = RADIO_MODE_FM;
		fm.bw = RADIO_BW_25K;
		fm.rx_only = false;
		ysf_conv_encode(&yc, blk);
		CHECK(codeplug_set_channel(34, &fm) && codeplug_tag_write(34, "Mixed", &mt, blk, sizeof blk));
		CHECK(codeplug_channel(34, &r) && r.kind == CODEPLUG_KIND_YSF && r.rx_only && r.mode == RADIO_MODE_FM &&
		      r.bw == RADIO_BW_25K);
		r.mode = RADIO_MODE_YSF;
		r.bw = RADIO_BW_12K5;
		CHECK(ysf_conv_save(34, &r, &ed));
		CHECK(codeplug_channel(34, &r) && r.kind == CODEPLUG_KIND_YSF && r.rx_only && r.mode == RADIO_MODE_FM &&
		      r.bw == RADIO_BW_25K && r.tx_hz == 434300000);
		CHECK(codeplug_tag_read(34, &rt) && rt.flags == CODEPLUG_TAG_BASE_OWN);
		CHECK(ysf_conv_load(34, &r, &back) && back.conv.rx_dgid == 21 && back.conv.tx_dgid == 22);
		CHECK(codeplug_tag_remove(34) && codeplug_channel(34, &r) && r.kind == CODEPLUG_KIND_NONE && !r.rx_only &&
		      r.mode == RADIO_MODE_FM && r.bw == RADIO_BW_25K);
	}
}

int main(void)
{
	test_vectors();
	test_conv_load();
	test_conv_save();
	sim_flash_free();
	if (failures) {
		printf("ysfCodeplugTest: %d failure(s)\n", failures);
		return 1;
	}
	printf("ysfCodeplugTest: all passed\n");
	return 0;
}
