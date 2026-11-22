// SPDX-License-Identifier: MIT
import { test } from "node:test";
import assert from "node:assert/strict";
import { empty, encode, decode, newChannel } from "../static/js/codeplug/model.js";
import { setKind, validKeyHex, withoutSecrets } from "../static/js/codeplug/channelmodes.js";
import { DMR_PRIV, channelAddr } from "../static/js/codeplug/records.js";
import { channelForm } from "../static/js/editor/views.js";

test("channel AES picks matching IDs, including zero, and survives codeplug encoding", () => {
	const model = empty();
	const dmr = newChannel({ name: "DMR", mode: "dmr", rxHz: 433000000, txHz: 433000000 });
	const p25 = newChannel({ name: "P25", rxHz: 433000000, txHz: 433000000 });
	const nxdn = newChannel({ name: "NXDN", rxHz: 433000000, txHz: 433000000 });
	setKind(model, p25, 2);
	setKind(model, nxdn, 4);
	model.channels.push(dmr, p25, nxdn);
	const key = "AB".repeat(32);
	model.keyring = [{ alg: 0x25, id: 7, key }, { alg: 0x84, id: 65535, key }, { alg: 3, id: 0, key }];
	const fields = channelForm(model, (s) => s);
	fields.find((f) => f.key === "dmrPrivacy").set(dmr, DMR_PRIV.aes);
	for (const [channel, name, id] of [[dmr, "dmrPrivacyKey", 7], [p25, "k2.aes", 65535], [nxdn, "k4.aes", 0]]) {
		const f = fields.find((field) => field.key === name);
		assert.ok(f.show(channel));
		assert.deepEqual(f.options(channel).map(([keyId]) => keyId), [null, id]);
		f.set(channel, id);
		assert.equal(f.get(channel), id);
	}
	const restored = decode(encode(model));
	assert.equal(restored.channels[0].dmrPrivacy, DMR_PRIV.aes);
	assert.equal(restored.channels[0].dmrPrivacyKey, 7);
	assert.deepEqual(Array.from(encode(model).bytes.subarray(channelAddr(dmr.number) + 0x34, channelAddr(dmr.number) + 0x37)),
		[0xA5, 0x5A, 7]);
	assert.equal(restored.channels[1].params.keyId, 65535);
	assert.equal(restored.channels[2].params.aes, true);
	assert.equal(restored.channels[2].params.keyId, 0);

	model.keyring = [];
	const field = fields.find((f) => f.key === "k2.aes");
	assert.deepEqual(field.options(p25).map(([id]) => id), [null, 65535]);
});

test("DMR Basic, Enhanced and Off persist independently, and exports redact keys", () => {
	const m = empty();
	for (const name of ["Basic", "Enhanced", "Off"])
		m.channels.push(newChannel({ name, mode: "dmr", rxHz: 433000000, txHz: 433000000 }));
	const [basic, ep, off] = m.channels;
	m.keyring = [{ alg: 0x21, id: 15, key: "373374ABE8" }, { alg: 0x25, id: 9, key: "AB".repeat(32) }];
	const fields = channelForm(m, (s) => s);
	const kind = fields.find((f) => f.key === "dmrPrivacy");
	const key = fields.find((f) => f.key === "dmrPrivacyKey");
	kind.set(basic, DMR_PRIV.basic);
	assert.equal(key.options(basic).length, 256);
	key.set(basic, 179);
	kind.set(ep, DMR_PRIV.enhanced);
	assert.deepEqual(key.options(ep).map(([id]) => id), [null, 15]);
	key.set(ep, 15);
	kind.set(off, DMR_PRIV.off);
	assert.equal(key.show(off), false);
	assert.equal(validKeyHex(0x21, "373374ABE"), false);
	assert.equal(validKeyHex(0x21, "373374ABE8"), true);
	const image = encode(m);
	const before = decode(image);
	assert.deepEqual(before.channels.map((c) => [c.dmrPrivacy, c.dmrPrivacyKey]), [[2, 179], [3, 15], [4, 0]]);
	assert.deepEqual(before.keyring.map((k) => [k.alg, k.key.length]), [[0x21, 10], [0x25, 64]]);
	const exported = image.clone();
	withoutSecrets(exported.bytes);
	const after = decode(exported);
	assert.deepEqual(after.channels.map((c) => [c.dmrPrivacy, c.dmrPrivacyKey]), [[2, 0], [3, 15], [4, 0]]);
	assert.deepEqual(after.keyring, []);
	assert.equal(decode(image).channels[0].dmrPrivacyKey, 179);
});
