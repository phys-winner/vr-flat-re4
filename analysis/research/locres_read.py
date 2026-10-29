"""Read the original UE4 optimized locres without changing any game resources."""
from pathlib import Path
import argparse
import hashlib
import json
import struct

MAGIC = bytes.fromhex("0e147475674a03fc4a15909dc3377f1b")


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def unpack(self, fmt):
        length = struct.calcsize(fmt)
        if self.pos + length > len(self.data):
            raise ValueError("locres field exceeds file")
        value = struct.unpack_from(fmt, self.data, self.pos)
        self.pos += length
        return value[0] if len(value) == 1 else value

    def string(self):
        count = self.unpack("<i")
        if abs(count) > 1_000_000:
            raise ValueError("invalid FString count")
        length = count if count >= 0 else -2 * count
        end = self.pos + length
        if end > len(self.data):
            raise ValueError("FString exceeds file")
        raw = self.data[self.pos:end]
        self.pos = end
        if not count:
            return ""
        if count > 0:
            if not raw.endswith(b"\0"):
                raise ValueError("missing FString terminator")
            return raw[:-1].decode("utf8")
        if not raw.endswith(b"\0\0"):
            raise ValueError("missing UTF16 FString terminator")
        return raw[:-2].decode("utf-16-le")

    def key(self):
        return {"hash": self.unpack("<I"), "text": self.string()}


def read_locres(data):
    if data[:16] != MAGIC:
        raise ValueError("unsupported locres magic")
    r = Reader(data)
    r.pos = 16
    version = r.unpack("<B")
    if version != 2:
        raise ValueError(f"unsupported locres version {version}")
    string_offset, entry_count, namespace_count = r.unpack("<Q"), r.unpack("<I"), r.unpack("<I")
    if not 0 <= namespace_count <= entry_count <= 1_000_000:
        raise ValueError("invalid namespace/entry count")
    entries = []
    for _ in range(namespace_count):
        namespace = r.key()
        count = r.unpack("<I")
        if count > entry_count - len(entries):
            raise ValueError("namespace key count exceeds total entries")
        for _ in range(count):
            key = r.key()
            entries.append({"namespace": namespace["text"], "namespace_hash": namespace["hash"],
                            "key": key["text"], "key_hash": key["hash"],
                            "source_hash": r.unpack("<I"), "string_index": r.unpack("<i")})
    if len(entries) != entry_count or r.pos != string_offset:
        raise ValueError("entry count or string-table offset mismatch")
    count = r.unpack("<I")
    if count > entry_count:
        raise ValueError("invalid localized-string count")
    strings = []
    for _ in range(count):
        strings.append({"text": r.string(), "references": r.unpack("<i")})
    if r.pos != len(data):
        raise ValueError("unparsed locres tail")
    references = [0] * count
    for entry in entries:
        index = entry["string_index"]
        if not 0 <= index < count:
            raise ValueError("invalid localized-string index")
        entry["text"] = strings[index]["text"]
        references[index] += 1
    if references != [s["references"] for s in strings]:
        raise ValueError("localized-string reference counts differ from namespace entries")
    return {"version": version, "namespace_count": namespace_count, "entry_count": entry_count,
            "localized_string_count": count, "sha256": hashlib.sha256(data).hexdigest(),
            "entries": entries}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = read_locres(args.input.read_bytes())
    if args.json:
        args.json.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf8")
    print(json.dumps({k: v for k, v in result.items() if k != "entries"}))
