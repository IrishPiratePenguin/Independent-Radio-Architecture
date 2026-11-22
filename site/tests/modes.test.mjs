// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Copyright (c) 2026 lyra XE1POC (changes)
// The channel-mode layer of the Codeplug Editor (docs/spec/channel-modes.md): static/js/codeplug/channelmodes.js, the
// registry (modes.js) and the DMR Tier III and P25 modules, against the shared vectors (tests/vectors/channel-modes.txt)
// the C host tests read too. Run with node --test site/tests/ (site/tests/run.sh).

import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";

import { CodeplugImage } from "../static/js/codeplug/image.js";
import { decodeArea, getValue, setValue, putArea, KV_ADDR, KV_SIZE } from "../static/js/codeplug/kv.js";
import { decode, encode, empty, newChannel, newContact, remove, validate, numberAll } from "../static/js/codeplug/model.js";
import { MODE_KEY, decodeTag, encodeTag, newSystem, setKind, syncModes, decodeSystemRecord, identityKey, countSecrets } from "../static/js/codeplug/channelmodes.js";
import { KINDS, SYSTEM_TYPES, offeredKinds, offeredTypes, kindName } from "../static/js/codeplug/modes.js";
import { nameCheck } from "../static/js/codeplug/modebytes.js";
import { decodeConv, encodeConv, decodeUnit, decodeTsys, p25System, NAC_ANY } from "../static/js/codeplug/mode_p25.js";

const VECTORS = new URL("../../tests/vectors/channel-modes.txt", import.meta.url);
const bytes = (s) => (s === "-" ? null : Uint8Array.from(s.match(/../g) ?? [], (x) => parseInt(x, 16)));
const hexOf = (b) => [...b].map((x) => x.toString(16).padStart(2, "0")).join("").toUpperCase();
const vectors = (kind) => readFileSync(VECTORS, "utf8").split("\n").filter((l) => l.startsWith(kind + " ")).map((l) => l.slice(kind.length + 1));
const area = (img) => decodeArea(img.bytes).entries;
const round = (m) => decode(encode(m));

test("vectors: name check", () => {
	const v = vectors("check");
	assert.ok(v.length >= 4);
	for (const l of v) {
		const [chk, ...name] = l.split(" ");
		assert.equal(nameCheck(name.join(" ")), parseInt(chk, 16), JSON.stringify(name.join(" ")));
	}
});

test("vectors: P25 conventional block, unit ID record", () => {
	for (const l of vectors("p25conv")) {
		const [b, rx, tx, sq] = l.split(" ");
		const { squelch, rxNac, txNac } = decodeConv(bytes(b));
		assert.deepEqual({ squelch, rxNac, txNac }, { squelch: parseInt(sq, 16), rxNac: parseInt(rx, 16), txNac: parseInt(tx, 16) }, l);
	}
	for (const l of vectors("p25enc")) {
		const [rx, tx, sq, b] = l.split(" ");
		assert.equal(hexOf(encodeConv({ squelch: parseInt(sq, 16), rxNac: parseInt(rx, 16), txNac: parseInt(tx, 16) })), b, l);
	}
	for (const l of vectors("p25aes")) {
		const [b, aes, kid] = l.split(" ");
		const d = decodeConv(bytes(b));
		assert.deepEqual({ aes: d.aes, keyId: d.keyId }, { aes: aes === "1", keyId: parseInt(kid, 16) }, l);
	}
	for (const l of vectors("p25aesenc")) {
		const [rx, tx, sq, aes, kid, b] = l.split(" ");
		assert.equal(hexOf(encodeConv({ squelch: parseInt(sq, 16), rxNac: parseInt(rx, 16), txNac: parseInt(tx, 16), aes: aes === "1", keyId: parseInt(kid, 16) })), b, l);
	}
	for (const l of vectors("p25unit")) {
		const [b, dmr, unit] = l.split(" ");
		const u = decodeUnit(bytes(b)).unitId;
		assert.equal(u || parseInt(dmr, 16), parseInt(unit, 16), l);
	}
});

test("vectors: P25 trunked system block and identifier plan rows", () => {
	for (const l of vectors("p25tsys")) {
		const [b, wacn, sysid, nac, opts, hold, roam] = l.split(" ").map((x, i) => (i ? parseInt(x, 16) : x));
		const s = decodeTsys(bytes(b));
		assert.equal(s.wacn ?? 0xFFFFFF, wacn, l);
		assert.equal(s.sysid ?? 0xFFFF, sysid, l);
		assert.equal(s.ccNac ?? NAC_ANY, nac, l);
		assert.equal((s.skipEnc ? 1 : 0) | (s.planOnly ? 2 : 0) | (s.noReg ? 4 : 0), opts, l);
		assert.equal(s.holdS, hold, l);
		assert.equal(s.roamDbm & 0xFF, roam, l);
	}
	const plan = p25System.classes.find((c) => c.cls === 2);
	for (const l of vectors("p25iden")) {
		const [b, base, sp, off, bw, type, slots] = l.split(" ").map((x, i) => (i ? parseInt(x, 16) : x));
		const e = plan.decode(bytes(b));
		assert.deepEqual([e.baseHz, e.spacingHz, e.txOffsetHz >>> 0, e.bwHz, e.tdma ? 1 : 0, e.tdma ? e.slots : 1], [base, sp, off, bw, type, slots], l);
	}
});

test("registry: unique ids, names, a module each", () => {
	for (const list of [KINDS, SYSTEM_TYPES]) {
		assert.equal(new Set(list.map((k) => k.id)).size, list.length);
		for (const k of list) assert.ok(k.id >= 1 && k.id < 0xFF && k.module.id === k.id && k.short);
	}
	assert.equal(kindName(0x80), "?");
	for (const k of KINDS) if (k.module.systemType) assert.ok(SYSTEM_TYPES.some((s) => s.id === k.module.systemType));
});

// A codeplug with one channel of each kind, a Tier III and a P25 trunked system, a key and the unit ID
function sample() {
	const m = empty();
	const tg = newContact({ name: "TG 100", id: 100 });
	m.contacts.push(tg);
	const plain = newChannel({ name: "Plain FM" });
	const conv = newChannel({ name: "P25 Ops", rxHz: 453100000, txHz: 458100000, contact: tg });
	const t3 = newChannel({ name: "T3 Ops", contact: tg });
	const p25t = newChannel({ name: "P25 Trunk", contact: tg });
	m.channels.push(plain, conv, t3, p25t);
	const s1 = newSystem(m, 1, "Site net");
	s1.fields.net = 5;
	s1.needsKey = true;
	s1.secret = { key: "00112233445566778899AABBCCDDEEFF", psn: 0x123456 };
	s1.entries.push({ cls: 0, group: 0xFF, control: true, flags: 0, chan: 12, rxHz: 451000000, txHz: 456000000, cc: null });
	const s3 = newSystem(m, 3, "County");
	s3.fields.wacn = 0xBEE00;
	s3.fields.sysid = 0x1A2;
	s3.entries.push({ cls: 1, group: 0, control: false, flags: 0, rfss: 1, site: 7 });
	s3.entries.push({ cls: 0, group: 0, control: true, flags: 0, iden: 1, chanNo: 0x123, chan: 0, rxHz: 851012500, txHz: 0 });
	s3.entries.push({ cls: 2, group: 1, control: false, flags: 0, baseHz: 851006250, spacingHz: 6250, txOffsetHz: -45000000, bwHz: 12500, tdma: false, slots: 2 });
	m.systems.push(s1, s3);
	numberAll(m);
	setKind(m, conv, 2);
	conv.params = { ...conv.params, rxNac: 0x34, squelch: 1 };
	setKind(m, t3, 1);
	t3.system = s1;
	t3.fallback = plain;
	setKind(m, p25t, 3);
	p25t.system = s3;
	p25t.params = { followAll: true };
	m.modeSettings[2] = { unitId: 123456 };
	return { m, plain, conv, t3, p25t, s1, s3 };
}

test("model: channels of every kind, systems, entries, the key and the unit ID round-trip", () => {
	const { m } = sample();
	const img = encode(m);
	const d = decode(img);
	const by = (n) => d.channels.find((c) => c.name === n);
	assert.equal(by("Plain FM").kind, 0);
	const conv = by("P25 Ops"), t3 = by("T3 Ops"), p25t = by("P25 Trunk");
	assert.equal(conv.kind, 2);
	assert.deepEqual(conv.params, { squelch: 1, rxNac: 0x34, txNac: 0x293, aes: false, keyId: 0 });
	assert.equal(conv.tagTx, true);
	assert.equal(conv.mode, "dmr");
	assert.equal(conv.rxOnly, true);
	assert.equal(conv.rxHz, 453100000);
	assert.equal(t3.kind, 1);
	assert.equal(t3.system.name, "Site net");
	assert.equal(t3.fallback, by("Plain FM"));
	assert.equal(t3.tagTx, true);
	assert.equal(t3.rxHz, 451000000, "the record holds the control channel");
	assert.equal(p25t.kind, 3);
	assert.equal(p25t.tagTx, false, "P25 trunked: receive only by default");
	assert.deepEqual(p25t.params, { followAll: true });
	assert.equal(p25t.rxHz, 851012500);
	assert.equal(p25t.txHz, 851012500);
	const s1 = d.systems.find((s) => s.type === 1), s3 = d.systems.find((s) => s.type === 3);
	assert.equal(s1.fields.net, 5);
	assert.equal(s1.tx, true);
	assert.equal(s1.needsKey, true);
	assert.deepEqual(s1.secret, { key: "00112233445566778899AABBCCDDEEFF", psn: 0x123456 });
	assert.equal(s3.tx, false, "P25 trunked: listen only by default");
	assert.equal(s3.fields.wacn, 0xBEE00);
	assert.deepEqual(s3.entries.map((e) => e.cls), [1, 0, 2]);
	assert.equal(s3.entries[1].iden, 1);
	assert.equal(s3.entries[1].chanNo, 0x123);
	assert.equal(s3.entries[2].txOffsetHz, -45000000);
	assert.deepEqual(d.modeSettings[2], { unitId: 123456 });
	// unedited: byte for byte
	assert.deepEqual(encode(d).bytes, img.bytes);
	// the stored bytes: the system header of 7.1, the key, the tag
	const e = area(img);
	const sys = decodeSystemRecord(getValue(e, MODE_KEY.system + s1.number - 1));
	assert.equal(sys.type, 1);
	assert.equal(sys.idLen, 4);
	assert.equal(sys.flags, 3);
	assert.equal(getValue(e, MODE_KEY.secret + s1.number - 1).length, 19);
	const tag = decodeTag(getValue(e, MODE_KEY.tag + t3.number - 1));
	assert.deepEqual([tag.kind, tag.system, tag.check, tag.flags & 0x0F, tag.fbChan], [1, s1.number - 1, nameCheck("T3 Ops"), 0x05, by("Plain FM").number]);
	assert.equal(hexOf(getValue(e, MODE_KEY.setting + 2)), "40E20100");
});

test("tags: follow a rename, go when the channel is made plain or deleted, a stale one is kept", () => {
	const { m } = sample();
	const d = round(m);
	const conv = d.channels.find((c) => c.name === "P25 Ops"), n = conv.number;
	conv.name = "P25 Two";
	let e = area(encode(d));
	assert.equal(decodeTag(getValue(e, MODE_KEY.tag + n - 1)).check, nameCheck("P25 Two"));
	// a stale tag on a plain channel (a check of neither name) stays
	const d2 = round(m);
	const plain = d2.channels.find((c) => c.name === "Plain FM");
	const stale = encodeTag({ kind: 2, system: 0xFF, check: 0x1234, flags: 1, params: new Uint8Array(0) });
	putArea(d2.base.bytes, setValue(area(d2.base), MODE_KEY.tag + plain.number - 1, stale));
	const d3 = decode(d2.base.clone());
	assert.equal(d3.channels.find((c) => c.name === "Plain FM").kind, 0);
	d3.channels.find((c) => c.name === "Plain FM").name = "Plain 2";
	assert.deepEqual(getValue(area(encode(d3)), MODE_KEY.tag + plain.number - 1), stale);
	// made plain: the tag goes
	const d4 = round(m);
	const c4 = d4.channels.find((c) => c.name === "P25 Ops");
	setKind(d4, c4, 0, "fm");
	e = area(encode(d4));
	assert.equal(getValue(e, MODE_KEY.tag + c4.number - 1), null);
	assert.equal(c4.rxOnly, false, "transmit allowed comes back as the record's");
	// a kind whose record holds its system's frequencies: made plain, receive only
	const t3p = d4.channels.find((c) => c.name === "T3 Ops");
	t3p.tagTx = true;
	setKind(d4, t3p, 0, "dmr");
	assert.equal(t3p.rxOnly, true, "never transmit on a system's control channel");
	// deleted: the tag goes, and a fallback naming it is none
	const d5 = round(m);
	remove(d5, "channels", [d5.channels.find((c) => c.name === "Plain FM")]);
	const t3 = d5.channels.find((c) => c.name === "T3 Ops");
	assert.equal(t3.fallback, null);
	e = area(encode(d5));
	assert.equal(decodeTag(getValue(e, MODE_KEY.tag + t3.number - 1)).flags & 0x04, 0, "no fallback of its own");
	const d6 = round(m);
	const c6 = d6.channels.find((c) => c.name === "P25 Ops");
	remove(d6, "channels", [c6]);
	assert.equal(getValue(area(encode(d6)), MODE_KEY.tag + c6.number - 1), null);
});

test("a tag, a system and an entry of ids this editor doesn't know are kept byte for byte", () => {
	const img = CodeplugImage.blank();
	const m = decode(img);
	const c = newChannel({ name: "Future", mode: "dmr", rxOnly: true });
	const other = newChannel({ name: "Other" });
	m.channels.push(c, other);
	numberAll(m);
	const base = encode(m);
	const tag = encodeTag({ kind: 0x80, system: 0, check: nameCheck("Future"), flags: 0x31, params: Uint8Array.of(9, 8, 7) });
	const sys = new Uint8Array(28 + 3);
	sys.set([0x80, 0x06, 1, 2]);
	sys.set([1, 2, 3], 28);
	const ent = Uint8Array.of(0, 0x81, 0xFF, 0, 5, 5);
	let e = setValue(area(base), MODE_KEY.tag, tag);
	e = setValue(e, MODE_KEY.system, sys);
	e = setValue(e, MODE_KEY.entry, ent);
	e = setValue(e, MODE_KEY.setting + 0x80, Uint8Array.of(1));
	putArea(base.bytes, e);
	const d = decode(base);
	const f = d.channels.find((x) => x.name === "Future");
	assert.equal(f.kind, 0x80);
	assert.equal(f.params, null);
	assert.equal(d.systems[0].type, 0x80);
	assert.ok(validate(d).some((p) => p.code === "mode_kind_unknown"));
	assert.ok(validate(d).some((p) => p.code === "mode_type_unknown"));
	d.channels.find((x) => x.name === "Other").name = "Other 2";
	f.name = "Future 2";
	const out = area(encode(d));
	assert.deepEqual(getValue(out, MODE_KEY.system), sys);
	assert.deepEqual(getValue(out, MODE_KEY.entry), ent);
	assert.deepEqual(getValue(out, MODE_KEY.setting + 0x80), Uint8Array.of(1));
	const t = getValue(out, MODE_KEY.tag);
	assert.deepEqual([...t.subarray(4)], [0x31, 9, 8, 7], "flags and parameters kept");
	assert.equal(decodeTag(t).check, nameCheck("Future 2"), "the check follows the rename");
});

test("bytes a module doesn't cover are kept: a longer parameter block, a longer system block", () => {
	const { m } = sample();
	const img = encode(m);
	const d = decode(img);
	const conv = d.channels.find((c) => c.name === "P25 Ops");
	const key = MODE_KEY.tag + conv.number - 1;
	const long = Uint8Array.from([...getValue(area(img), key), 0xAB, 0xCD]);
	putArea(img.bytes, setValue(area(img), key, long));
	const d2 = decode(img);
	d2.channels.find((c) => c.name === "P25 Ops").params.txNac = 0x55;
	const t = getValue(area(encode(d2)), key);
	assert.deepEqual([...t.subarray(-2)], [0xAB, 0xCD]);
	assert.equal(decodeConv(decodeTag(t).params).txNac, 0x55);
});

test("deleting a system drops its key, its entries, and turns its channels plain and receive only", () => {
	const { m } = sample();
	const d = round(m);
	const s1 = d.systems.find((s) => s.type === 1);
	remove(d, "systems", [s1]);
	const t3 = d.channels.find((c) => c.name === "T3 Ops");
	assert.equal(t3.kind, 0);
	assert.equal(t3.rxOnly, true);
	const e = area(encode(d));
	assert.equal(getValue(e, MODE_KEY.system + s1.number - 1), null);
	assert.equal(getValue(e, MODE_KEY.secret + s1.number - 1), null);
	const d2 = decode(encode(d));
	assert.equal(d2.systems.length, 1);
	assert.equal(d2.systems[0].entries.length, 3);
});

test("offered kinds: the radio's features and what the codeplug holds", () => {
	const m = empty();
	assert.deepEqual(offeredKinds(m, "FM,DMR").map((k) => k.id), []);
	assert.deepEqual(offeredKinds(m, "FM,DMR,P25").map((k) => k.id), [2]);
	const { m: s } = sample();
	assert.deepEqual(offeredKinds(s, "FM,DMR").map((k) => k.id), [1, 2, 3]);
	assert.deepEqual(offeredKinds(m, null).map((k) => k.id), KINDS.map((k) => k.id), "no radio: every kind");
	assert.deepEqual(offeredTypes(m, null).map((k) => k.id), SYSTEM_TYPES.map((k) => k.id), "no radio: every type");
});

test("validation: mode not in the radio, no system, unit ID, a P25 talk group", () => {
	const { m, t3, conv } = sample();
	t3.system = null;
	m.modeSettings[2] = { unitId: 10000000 };
	conv.contact.id = 70000;
	const p = validate(m, { features: "FM,DMR,P25" });
	const has = (code, c) => p.some((x) => x.code === code && (!c || x.item === c));
	assert.ok(has("mode_not_in_radio", t3));
	assert.ok(!has("mode_not_in_radio", conv));
	assert.ok(has("mode_no_system", t3));
	assert.ok(has("p25_unit_long"));
	assert.ok(has("p25_tg_range", conv));
	assert.ok(!validate(m).some((x) => x.code === "mode_not_in_radio"), "no radio: nothing to check against");
});

test("identity: two systems of the same type and identity are the same system", () => {
	const m = empty();
	const a = newSystem(m, 3, "A"), b = newSystem(m, 3, "B");
	a.fields.wacn = b.fields.wacn = 0x12345;
	a.fields.sysid = b.fields.sysid = 0x10;
	b.fields.holdS = 9;
	assert.equal(identityKey(a), identityKey(b));
	m.systems.push(a, b);
	assert.ok(validate(m).some((x) => x.code === "mode_same_identity"));
});

test("a system of either type with no control channel is flagged", () => {
	const m = empty();
	for (const type of [1, 3]) m.systems.push(newSystem(m, type, `S${type}`));
	const flagged = validate(m).filter((x) => x.code === "mode_no_control").map((x) => x.item.type);
	assert.deepEqual(flagged.sort(), [1, 3]);
});

test("an area of a newer version is never written", () => {
	const { m } = sample();
	const img = encode(m);
	img.bytes[KV_ADDR + 4] = 9;
	const d = decode(img);
	assert.equal(d.systems.length, 0);
	const out = encode(d);
	assert.deepEqual(out.bytes.subarray(KV_ADDR, KV_ADDR + KV_SIZE), img.bytes.subarray(KV_ADDR, KV_ADDR + KV_SIZE));
	syncModes(d);
});

// --- The editor's forms (views.js, modefields.js), without a page ---

import { channelForm, columns } from "../static/js/editor/views.js";
import { parseSpec, formatSpec, specField } from "../static/js/editor/modefields.js";

const t = (k, p) => (p ? `${k} ${JSON.stringify(p)}` : k);

test("channel form: the mode list offers the radio's kinds; picking one makes the channel that kind", () => {
	const m = empty();
	const c = newChannel({ name: "Ch", rxOnly: false });
	m.channels.push(c);
	numberAll(m);
	const mode = (f) => f.find((x) => x.key === "mode");
	assert.deepEqual(mode(channelForm(m, t, { features: "FM,DMR" })).options(c).map(([v]) => v), ["fm", "dmr"]);
	const form = channelForm(m, t, { features: "FM,DMR,P25" });
	const f = mode(form);
	assert.deepEqual(f.options(c).map(([v]) => v), ["fm", "dmr", "k2"]);
	const work = { ...c };
	f.set(work, "k2");
	assert.equal(work.kind, 2);
	assert.equal(work.mode, "dmr");
	assert.equal(work.rxOnly, true, "the record is the stand-in");
	assert.equal(work.tagTx, true, "transmit allowed carries over");
	assert.equal(f.get(work), "k2");
	// the RX-only box shows the tag's transmit allowed
	const rxOnly = form.find((x) => x.key === "rxOnly");
	assert.equal(rxOnly.get(work), false);
	rxOnly.set(work, true);
	assert.equal(work.tagTx, false);
	// the P25 fields: shown, parsed into the parameters; DMR's own fields hidden, the talk target kept
	const nac = form.find((x) => x.key === "k2.rxNac");
	assert.ok(nac.show(work));
	assert.equal(nac.parse(work, nac.key, "f7e"), undefined);
	assert.equal(work.params.rxNac, 0xF7E);
	assert.ok(nac.parse(work, nac.key, "1000"));
	const tx = form.find((x) => x.key === "k2.txNac");
	tx.parse(work, tx.key, "");
	assert.equal(work.params.txNac, 0x293, "blank TX NAC with RX any: the default");
	assert.equal(form.find((x) => x.key === "timeslot").show(work), false);
	assert.equal(form.find((x) => x.key === "contact").show(work), true);
	assert.equal(form.find((x) => x.key === "dmrId").show(work), true);
	assert.equal(form.find((x) => x.key === "squelch").show(work), false, "FM's fields hidden");
	// back to FM: plain, TX as it was in the tag
	f.set(work, "fm");
	assert.equal(work.kind, 0);
	assert.equal(work.rxOnly, true);
});

test("channel table: a channel of a kind shows its mode, offers every mode, and locks the fields of other modes", () => {
	const { m, conv, p25t } = sample();
	const cols = columns("channels", m, t);
	const col = (k) => cols.find((c) => c.key === k);
	assert.equal(col("mode").format(conv.mode, conv), "P25");
	assert.equal(col("mode").readOnly, false, "the kind can be changed in the table too");
	const offered = col("mode").options(conv).map(([v]) => v);
	assert.ok(["fm", "dmr", "k2", "k3"].every((v) => offered.includes(v)), "FM, DMR and the kinds, with no radio connected");
	assert.equal(col("mode").get(conv), "k2");
	assert.equal(col("timeslot").readOnly(conv), true);
	assert.equal(col("contact").readOnly(conv), false);
	assert.equal(col("rxHz").readOnly(conv), false);
	assert.equal(col("rxHz").readOnly(p25t), true, "a trunked channel's frequencies are its system's");
	assert.equal(col("rxOnly").get(p25t), true);
});

test("declared fields: hex with any, lists, keys, frequencies", () => {
	const hexAny = { type: "hex", min: 0, max: 0xFFFFF, any: true };
	assert.deepEqual(parseSpec(hexAny, "", t), { value: null });
	assert.deepEqual(parseSpec(hexAny, "bee00", t), { value: 0xBEE00 });
	assert.ok(parseSpec(hexAny, "100000", t).error);
	assert.equal(formatSpec({ ...hexAny, format: (v) => v.toString(16) }, null, t), "");
	const list = { type: "list", min: 0, max: 7, count: 3 };
	assert.deepEqual(parseSpec(list, "1, 2 7", t), { value: [1, 2, 7] });
	assert.ok(parseSpec(list, "1 2 3 4", t).error);
	assert.ok(parseSpec(list, "8", t).error);
	const key = { type: "hexbytes", bytes: 16 };
	assert.deepEqual(parseSpec(key, "00112233-44556677 8899aabb:ccddeeff", t), { value: "00112233445566778899AABBCCDDEEFF" });
	assert.ok(parseSpec(key, "0011", t).error);
	assert.deepEqual(parseSpec({ type: "freq", zero: true }, "", t), { value: 0 });
	assert.deepEqual(parseSpec({ type: "freq" }, "851.0125", t), { value: 851012500 });
	// a field of another mode neither sets nor complains
	const f = specField({ key: "x", label: "editor.x", type: "int", min: 0, max: 1 }, t, { on: () => false, get: (r) => r.p, put: () => assert.fail() }, "k");
	assert.equal(f.parse({}, f.key, "9"), undefined);
});

// --- NXDN conventional (docs/spec/nxdn-codeplug.md) ---

import * as nxdn from "../static/js/codeplug/mode_nxdn.js";

test("vectors: NXDN conventional block, unit ID record", () => {
	const conv = (rate, rx, tx, sq) => ({ rate9600: rate === "1", rxRan: parseInt(rx, 16), txRan: parseInt(tx, 16), squelch: parseInt(sq, 16) });
	assert.ok(vectors("nxdnconv").length >= 10);
	for (const l of vectors("nxdnconv")) {
		const [b, ...p] = l.split(" ");
		const { aes, keyId, ...rest } = nxdn.decodeConv(bytes(b) ?? new Uint8Array(0));
		assert.deepEqual(rest, conv(...p), l);
	}
	for (const l of vectors("nxdnaes")) {
		const [b, aes, kid] = l.split(" ");
		const d = nxdn.decodeConv(bytes(b));
		assert.deepEqual({ aes: d.aes, keyId: d.keyId }, { aes: aes === "1", keyId: parseInt(kid, 16) }, l);
	}
	for (const l of vectors("nxdnaesenc")) {
		const [rate, rx, tx, sq, aes, kid, b] = l.split(" ");
		assert.equal(hexOf(nxdn.encodeConv({ ...conv(rate, rx, tx, sq), aes: aes === "1", keyId: parseInt(kid, 16) })), b, l);
	}
	for (const l of vectors("nxdnenc")) {
		const [rate, rx, tx, sq, b] = l.split(" ");
		assert.equal(hexOf(nxdn.encodeConv(conv(rate, rx, tx, sq))), b, l);
	}
	for (const l of vectors("nxdnunit")) {
		const [b, dmr, unit] = l.split(" ");
		const u = nxdn.decodeUnit(bytes(b)).unitId;
		assert.equal(u || nxdn.unitFallback(parseInt(dmr, 16)), parseInt(unit, 16), l);
	}
});

// --- YSF conventional (docs/spec/ysf-codeplug.md) ---

import * as ysf from "../static/js/codeplug/mode_ysf.js";

test("vectors: YSF conventional block, identity record", () => {
	const conv = (dt, narrow, rx, tx, dest) => ({ dt: parseInt(dt, 16), narrow: narrow === "1", rxDgid: parseInt(rx, 16), txDgid: parseInt(tx, 16), dest });
	assert.ok(vectors("ysfconv").length >= 10);
	for (const l of vectors("ysfconv")) {
		const [b, ...p] = l.split(" ");
		assert.deepEqual(ysf.decodeConv(bytes(b) ?? new Uint8Array(0)), conv(...p), l);
	}
	for (const l of vectors("ysfenc")) {
		const [dt, narrow, rx, tx, dest, b] = l.split(" ");
		assert.equal(hexOf(ysf.encodeConv(conv(dt, narrow, rx, tx, dest))), b, l);
	}
	for (const l of vectors("ysfident")) {
		const [b, src, down, up] = l.split(" ");
		const call = (s) => (s === "-" ? "" : s);
		assert.deepEqual(ysf.decodeIdentity(bytes(b) ?? new Uint8Array(0)), { src: call(src), down: call(down), up: call(up) }, l);
	}
});

// A codeplug with a YSF channel and the radio-wide call signs
function ysfSample() {
	const m = empty();
	const a = newChannel({ name: "YSF Ops", rxHz: 439250000, txHz: 434250000 });
	m.channels.push(a);
	numberAll(m);
	setKind(m, a, 6);
	a.params = { ...a.params, dt: 2, narrow: true, rxDgid: 127, txDgid: 5, dest: "XE1ARS" };
	m.modeSettings[6] = { src: "XE1ARS", down: "", up: "" };
	return { m, a };
}

test("YSF: channel and the call signs round-trip; unknown bytes are kept", () => {
	const { m } = ysfSample();
	const img = encode(m);
	const d = decode(img);
	const a = d.channels.find((c) => c.name === "YSF Ops");
	assert.equal(a.kind, 6);
	assert.deepEqual(a.params, { dt: 2, narrow: true, rxDgid: 127, txDgid: 5, dest: "XE1ARS" });
	assert.equal(a.tagTx, true);
	assert.equal(a.mode, "dmr");
	assert.equal(a.rxOnly, true, "the record is the safe stand-in");
	assert.deepEqual(d.modeSettings[6], { src: "XE1ARS", down: "", up: "" });
	assert.deepEqual(encode(d).bytes, img.bytes);
	const e = area(img);
	assert.equal(hexOf(decodeTag(getValue(e, MODE_KEY.tag + a.number - 1)).params), "067F8558453141525320202020");
	assert.equal(hexOf(getValue(e, MODE_KEY.setting + 6)), "584531415253202020202020202020202020202020202020202020202020");
	// a longer block keeps its tail through an edit
	const key = MODE_KEY.tag + a.number - 1;
	putArea(img.bytes, setValue(area(img), key, Uint8Array.from([...getValue(area(img), key), 0xAB])));
	const d2 = decode(img);
	d2.channels.find((c) => c.name === "YSF Ops").params.txDgid = 9;
	const t = decodeTag(getValue(area(encode(d2)), key)).params;
	assert.equal(hexOf(t), "067F8958453141525320202020AB");
	// blank call signs write no record
	d2.modeSettings[6] = { src: "", down: "", up: "" };
	assert.equal(getValue(area(encode(d2)), MODE_KEY.setting + 6), null);
});

test("YSF: offered with the radio's feature", () => {
	assert.deepEqual(offeredKinds(empty(), "FM,DMR,YSF").map((k) => k.id), [6]);
	assert.equal(kindName(6), "YSF");
	const { m, a } = ysfSample();
	assert.deepEqual(offeredKinds(m, "FM,DMR").map((k) => k.id), [6], "the codeplug holds it");
	const has = (p, code, c) => p.some((x) => x.code === code && (!c || x.item === c));
	let p = validate(m, { features: "FM,DMR,YSF" });
	assert.ok(!has(p, "mode_not_in_radio", a));
	assert.ok(has(validate(m, { features: "FM,DMR,P25" }), "mode_not_in_radio", a));
});

// A codeplug with an NXDN channel on a group, one on a unit, and the radio-wide unit ID
function nxdnSample() {
	const m = empty();
	const grp = newContact({ name: "Group 7", id: 7 });
	const unit = newContact({ name: "Unit 42", id: 42, type: "private" });
	m.contacts.push(grp, unit);
	const a = newChannel({ name: "NXDN Ops", rxHz: 451250000, txHz: 456250000, contact: grp });
	const b = newChannel({ name: "NXDN Unit", rxHz: 451250000, txHz: 451250000, contact: unit });
	m.channels.push(a, b);
	numberAll(m);
	setKind(m, a, 4);
	a.params = { ...a.params, rxRan: 9, txRan: 9, squelch: 1 };
	setKind(m, b, 4);
	b.params = { ...b.params, rate9600: false, rxRan: 0, txRan: 2 };
	m.modeSettings[4] = { unitId: 1234 };
	return { m, a, b };
}

test("NXDN: channels and the unit ID round-trip; unknown bytes are kept", () => {
	const { m } = nxdnSample();
	const img = encode(m);
	const d = decode(img);
	const a = d.channels.find((c) => c.name === "NXDN Ops"), b = d.channels.find((c) => c.name === "NXDN Unit");
	assert.equal(a.kind, 4);
	assert.deepEqual(a.params, { rate9600: true, squelch: 1, rxRan: 9, txRan: 9, aes: false, keyId: 0 });
	assert.equal(a.tagTx, true);
	assert.equal(a.mode, "dmr");
	assert.equal(a.rxOnly, true, "the record is the safe stand-in");
	assert.deepEqual(b.params, { rate9600: false, squelch: 0, rxRan: 0, txRan: 2, aes: false, keyId: 0 });
	assert.equal(b.contact.type, "private");
	assert.deepEqual(d.modeSettings[4], { unitId: 1234 });
	assert.deepEqual(encode(d).bytes, img.bytes);
	const e = area(img);
	assert.equal(hexOf(decodeTag(getValue(e, MODE_KEY.tag + a.number - 1)).params), "0109000000");
	assert.equal(hexOf(decodeTag(getValue(e, MODE_KEY.tag + b.number - 1)).params), "0400820000");
	assert.equal(hexOf(getValue(e, MODE_KEY.setting + 4)), "D204");
	// a longer block keeps its tail through an edit
	const key = MODE_KEY.tag + a.number - 1;
	putArea(img.bytes, setValue(area(img), key, Uint8Array.from([...getValue(area(img), key), 0xAB])));
	const d2 = decode(img);
	d2.channels.find((c) => c.name === "NXDN Ops").params.txRan = 5;
	const t = decodeTag(getValue(area(encode(d2)), key)).params;
	assert.equal(hexOf(t), "0109850000AB");
	// a blank unit ID writes no record
	d2.modeSettings[4] = { unitId: 0 };
	assert.equal(getValue(area(encode(d2)), MODE_KEY.setting + 4), null);
});

test("NXDN: offered with the radio's feature; group, unit and channel ID ranges", () => {
	assert.deepEqual(offeredKinds(empty(), "FM,DMR,NXDN").map((k) => k.id), [4]);
	assert.equal(kindName(4), "NXDN");
	const { m, a, b } = nxdnSample();
	assert.deepEqual(offeredKinds(m, "FM,DMR").map((k) => k.id), [4], "the codeplug holds it");
	const has = (p, code, c) => p.some((x) => x.code === code && (!c || x.item === c));
	let p = validate(m, { features: "FM,DMR,NXDN" });
	assert.ok(!has(p, "mode_not_in_radio", a));
	assert.ok(!has(p, "nxdn_group_range") && !has(p, "nxdn_unit_range"));
	assert.ok(has(validate(m, { features: "FM,DMR,P25" }), "mode_not_in_radio", a));
	a.contact.id = 70000;
	b.contact.id = 65520;
	p = validate(m, { features: "FM,DMR,NXDN" });
	assert.ok(has(p, "nxdn_group_range", a));
	assert.ok(has(p, "nxdn_unit_range", b));
	b.contact.id = 42;
	a.contact.id = 7;
	a.dmrId = 3101276;
	assert.ok(has(validate(m, { features: "FM,DMR,NXDN" }), "nxdn_unit_range", a));
});

// --- AM, receive only (docs/spec/channel-modes.md 4, airband-and-broadcast.md 3.1) ---

test("AM: a channel of the kind has a header-only tag, receive only, and round-trips", () => {
	const m = empty();
	const a = newChannel({ name: "Mil Air", rxHz: 250000000, txHz: 250000000 });
	m.channels.push(a);
	numberAll(m);
	setKind(m, a, 5);
	const img = encode(m);
	const d = decode(img);
	const c = d.channels.find((x) => x.name === "Mil Air");
	assert.equal(c.kind, 5);
	assert.equal(kindName(5), "AM");
	assert.equal(c.mode, "dmr", "the record is the safe stand-in");
	assert.equal(c.rxOnly, true);
	assert.equal(c.tagTx, false);
	assert.deepEqual(c.params, {});
	assert.equal(decodeTag(getValue(area(img), MODE_KEY.tag + c.number - 1)).params.length, 0);
	assert.deepEqual(encode(d).bytes, img.bytes);
	assert.deepEqual(offeredKinds(empty(), "FM,DMR,AM_RX").map((k) => k.id), [5]);
	assert.deepEqual(offeredKinds(empty(), "FM,DMR"), []);
});

// --- The keyring (docs/spec/air-encryption.md 4) ---

import { newKey, KEY_BASE } from "../static/js/codeplug/channelmodes.js";

test("keyring: AES-256 keys of the air modes round-trip, other traffic keys are kept, errors are flagged", () => {
	const m = empty();
	const p = newKey(0x84), n = newKey(3);
	p.id = 0x1234;
	p.key = "00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF";
	n.id = 7;
	n.key = "A".repeat(64);
	m.keyring = [p, n];
	const img = encode(m);
	const d = decode(img);
	assert.equal(d.keyring.length, 2);
	assert.deepEqual(d.keyring.map((k) => [k.alg, k.id, k.key]), [[0x84, 0x1234, p.key], [3, 7, n.key]]);
	const e = area(img);
	const rec = getValue(e, KEY_BASE + d.keyring[0].slot);
	assert.equal(hexOf(rec.subarray(0, 5)), "FF84341200", "system 0xFF, algorithm, key id LE, flags");
	assert.equal(rec.length, 37);
	assert.deepEqual(encode(d).bytes, img.bytes, "unchanged: the same bytes");
	// a traffic key of a system, or of an algorithm the editor does not know, is left as found
	const foreign = Uint8Array.from([2, 0x84, 1, 0, 0, 1, 2, 3, 4]);
	putArea(img.bytes, setValue(area(img), KEY_BASE + 40, foreign));
	const d2 = decode(img);
	assert.equal(d2.keyring.length, 2, "not in the keyring");
	d2.keyring = d2.keyring.filter((k) => k.alg !== 3);
	const e2 = area(encode(d2));
	assert.equal(hexOf(getValue(e2, KEY_BASE + 40)), hexOf(foreign));
	assert.equal(getValue(e2, KEY_BASE + d.keyring[1].slot), null, "a key the model dropped is gone");
	// a new key takes the first free record
	d2.keyring.push({ ...newKey(0x84), id: 9, key: "B".repeat(64) });
	const d3 = decode(encode(d2));
	assert.deepEqual(d3.keyring.map((k) => [k.alg, k.id]), [[0x84, 0x1234], [0x84, 9]]);
	// validation: 64 hex digits, an id in range, no two the same
	const bad = empty();
	bad.keyring = [{ ...newKey(0x84), key: "xyz" }, { ...newKey(3), id: 64, key: "C".repeat(64) }, { ...newKey(0x84), id: 1, key: "D".repeat(64) }, { ...newKey(0x84), id: 1, key: "E".repeat(64) }];
	const codes = validate(bad, { features: "FM,DMR,P25,NXDN" }).map((x) => x.code);
	assert.ok(codes.includes("key_bad") && codes.includes("key_id_range") && codes.includes("key_duplicate"), codes.join());
	// the keys are secrets: counted, and taken out of a file saved without keys
	assert.equal(countSecrets(img.bytes), 3);
});
