// SPDX-License-Identifier: GPL-3.0-only
// The .fwsc package: its identity, the logical image the FM-1 reads during an update, and a damage check
// (header, file list, every file's CRC) so a broken download is caught before anything is written.
// After tools/fm1_install.py (Felucca, Leo Kuroshita).

const BLOCKS = 20, BLK = 0x30, KEEP = 0x2F;
const LOADER_MARK = "FELUCCA-LOADER-1";

// the package identity ("FM-1_926"): one marker byte after each of the first 20 blocks
export function productOf(raw) {
  if (raw.length < BLOCKS * BLK) throw new Error("not an FM-1 package (too short)");
  let s = "";
  for (let i = 0; i < BLOCKS; i++) {
    const m = raw[i * BLK + KEEP];
    if (m !== 0x7D) s += String.fromCharCode((m - i - 1) & 0xFF);
  }
  return s;
}

// the image the device addresses: the package without the 20 marker bytes
export function logicalImage(raw) {
  const out = new Uint8Array(raw.length - BLOCKS);
  for (let i = 0; i < BLOCKS; i++) out.set(raw.subarray(i * BLK, i * BLK + KEEP), i * KEEP);
  out.set(raw.subarray(BLOCKS * BLK), BLOCKS * KEEP);
  return out;
}

function crc16(d, crc = 0) {                     // CRC-16/XMODEM, as the package and the device use
  for (const b of d) {
    crc ^= b << 8;
    for (let k = 0; k < 8; k++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF;
  }
  return crc;
}

function dec(b) {                                // JieLi ENC stream cipher (its own inverse)
  const out = Uint8Array.from(b);
  let key = 0xFFFF;
  for (let i = 0; i < out.length; i++) {
    out[i] ^= key & 0xFF;
    key = ((key << 1) ^ (key & 0x8000 ? 0x1021 : 0)) & 0xFFFF;
  }
  return out;
}

const u16 = (b, o) => b[o] | (b[o + 1] << 8);
const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;

// what is wrong with the image, or null
export function packageDamage(img) {
  if (img.length < 0x40) return "too short";
  const hdr = dec(img.subarray(0, 0x40));
  if (crc16(hdr.subarray(2, 0x40)) !== u16(hdr, 0)) return "header CRC";
  const count = u16(hdr, 8);
  if (count < 1 || count > 8 || img.length < 0x40 + 0x50 * count) return "file list";
  const lst = img.subarray(0x40, 0x40 + 0x50 * count);
  if (crc16(lst) !== u16(hdr, 2)) return "file list CRC";
  for (let i = 0; i < count; i++) {
    const e = dec(lst.subarray(i * 0x50, (i + 1) * 0x50));
    const name = new TextDecoder("ascii").decode(e.subarray(0x40, 0x50)).replace(/\0.*$/s, "");
    const off = u32(e, 8), size = u32(e, 12);
    if (off + size > img.length) return `${name}: cut short`;
    if (crc16(img.subarray(off, off + size)) !== u16(e, 4)) return `${name}: CRC`;
  }
  return null;
}

function hasLoader(raw) {
  const m = new TextEncoder().encode(LOADER_MARK);
  outer: for (let i = 0; i + m.length <= raw.length; i++) {
    for (let j = 0; j < m.length; j++) if (raw[i + j] !== m[j]) continue outer;
    return true;
  }
  return false;
}

// raw .fwsc bytes -> { product, image }, or throws with a reason
export function readPackage(raw) {
  const product = productOf(raw);
  if (!/^[^_]+_\d+$/.test(product)) throw new Error(`not an FM-1 package (identity "${product}")`);
  if (!hasLoader(raw)) throw new Error("no Felucca update loader in this package");
  const image = logicalImage(raw);
  const bad = packageDamage(image);
  if (bad) throw new Error(`damaged package (${bad}): download it again`);
  return { product, image };
}
