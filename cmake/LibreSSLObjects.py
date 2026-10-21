import re
import sys

def heredoc(script):
    with open(script, encoding="latin-1", newline="") as file:
        text = file.read()
    return re.search(r"print OUT <<'EOF';\n(.*?)^EOF\n", text, re.S | re.M).group(1)


def read_lines(path):
    with open(path, encoding="latin-1", newline="") as file:
        return [line[:-1] for line in file]


def process_oid(oid, objects, path, number):
    arcs = oid.split()
    prefix = ""
    separator = ""
    if not re.fullmatch(r"[0-9]+", arcs[0]):
        arcs[0] = arcs[0].replace("-", "_")
        if arcs[0] not in objects:
            sys.exit(f"{path}:{number}:Undefined identifier {arcs[0]}")
        prefix = "OBJ_" + arcs[0]
        separator = ","
        arcs = arcs[1:]
    joined = "L,".join(arcs) + "L"
    return prefix + separator + joined if joined != "L" else prefix


def objects_to_mac(directory, output_path):
    objects_path = f"{directory}/objects.txt"
    numbers_path = f"{directory}/obj_mac.num"
    nid = {}
    nid_names = {}
    max_nid = 0
    for number, line in enumerate(read_lines(numbers_path), 1):
        line = re.sub(r"#.*$", "", line)
        if re.fullmatch(r"\s*", line):
            continue
        fields = ("X" + line).split()
        name = fields[0][1:]
        value = int(fields[1])
        if value in nid_names:
            sys.exit(f"{numbers_path}:{number}:There's already an object with NID {value}")
        if name in nid:
            sys.exit(f"{numbers_path}:{number}:There's already an object with name {name}")
        nid[name] = value
        nid_names[value] = name
        max_nid = max(max_nid, value)

    order = {}
    short = {}
    long = {}
    objects = {}
    module = ""
    cname = ""
    for number, line in enumerate(read_lines(objects_path), 1):
        match = re.match(r"!module\s+(.*)$", line)
        if match:
            module = (match.group(1) + "-").replace(".", "_").replace("-", "_")
        if re.fullmatch(r"!global", line):
            module = ""
        match = re.match(r"!Cname\s+(.*)$", line)
        if match:
            cname = match.group(1)
        match = re.match(r"!Alias\s+(.+?)\s+(.*)$", line)
        if match:
            cname = (module + match.group(1)).replace("-", "_")
            order[number] = cname
            objects[cname] = process_oid(match.group(2), objects, objects_path, number)
            line = ""
            cname = ""
        line = re.sub(r"!.*$", "", line)
        line = re.sub(r"#.*$", "", line)
        if re.fullmatch(r"\s*", line):
            continue
        fields = (line.split(":", 3) + ["", "", ""])[:3]
        oid, sn, ln = (field.strip() for field in fields)
        if oid != "":
            oid = process_oid(oid, objects, objects_path, number)
        if cname == "" and " " not in ln:
            cname = ln.replace(".", "_").replace("-", "_")
            if cname != "" and module + cname in long:
                sys.exit(f"objects.txt:{number}:There's already an object with long name {long[module + cname]}")
        if cname == "":
            cname = sn.replace("-", "_")
            if cname != "" and module + cname in short:
                sys.exit(f"objects.txt:{number}:There's already an object with short name {short[module + cname]}")
        if cname == "":
            cname = ln.replace("-", "_").replace(".", "_").replace(" ", "_")
            if cname != "" and module + cname in long:
                sys.exit(f"objects.txt:{number}:There's already an object with long name {long[module + cname]}")
        cname = module + cname.replace(".", "_").replace("-", "_")
        order[number] = cname
        short[cname] = sn
        long[cname] = ln
        objects[cname] = oid
        if cname not in nid:
            max_nid += 1
            nid[cname] = max_nid
            nid_names[max_nid] = cname
            print(f"Added OID {cname}", file=sys.stderr)
        cname = ""

    out = [heredoc(f"{directory}/objects.pl")]
    for number in sorted(order):
        cname = order[number]
        if short.get(cname, "") != "":
            out.append(f'#define SN_{cname}\t\t"{short[cname]}"\n')
        if long.get(cname, "") != "":
            out.append(f'#define LN_{cname}\t\t"{long[cname]}"\n')
        if cname in nid:
            out.append(f"#define NID_{cname}\t\t{nid[cname]}\n")
        if objects.get(cname, "") != "":
            out.append(f"#define OBJ_{cname}\t\t{objects[cname]}\n")
        out.append("\n")
    with open(output_path, "w", encoding="latin-1", newline="") as file:
        file.write("".join(out))


def der(arcs):
    values = [int(arc) for arc in arcs.split()]
    encoded = bytearray([(values[0] * 40 + values[1]) & 0xFF])
    for value in values[2:]:
        chunk = [value & 0x7F]
        value >>= 7
        while value:
            chunk.append(0x80 | (value & 0x7F))
            value >>= 7
        encoded.extend(reversed(chunk))
    return bytes(encoded)


def mac_to_dat(directory, mac_path, output_path):
    short = {}
    long = {}
    nid = {}
    objects = {}
    definitions = {}
    for line in read_lines(mac_path):
        match = re.match(r"#define\s+(\S+)\s+(.*)$", line)
        if not match:
            continue
        name, value = match.group(1), match.group(2)
        value = re.sub(r'^"', "", value)
        value = re.sub(r'"$', "", value)
        if name.startswith("SN_"):
            short[name[3:]] = value
        elif name.startswith("LN_"):
            long[name[3:]] = value
        elif name.startswith("NID_"):
            nid[int(value)] = name[4:]
        elif name.startswith("OBJ_"):
            objects[name[4:]] = name
            definitions[name] = value

    changed = True
    while changed:
        changed = False
        for key in definitions:
            match = re.search(r"(OBJ_[^,]+),", definitions[key])
            if match:
                definitions[key] = definitions[key][:match.start()] + definitions.get(match.group(1), "") + "," + definitions[key][match.end():]
                changed = True

    count = max(nid) + 1
    entries = []
    lvalues = []
    offset = 0
    der_text = {}
    der_length = {}
    for index in range(count):
        if index not in nid:
            entries.append("{NULL,NULL,NID_undef,0,NULL,0},\n")
            continue
        name = nid[index]
        sn = short.get(name, "NULL")
        ln = long.get(name, "NULL")
        if sn == "NULL":
            sn = ln
            short[name] = ln
        if ln == "NULL":
            ln = sn
            long[name] = sn
        entry = f'{{"{sn}","{ln}",NID_{name},'
        object_name = objects.get(name)
        if object_name is not None and "," in definitions[object_name]:
            encoded = der(definitions[object_name].replace("L", "").replace(",", " "))
            text = "".join(f"0x{byte:02X}," for byte in encoded)
            der_text[object_name] = text
            der_length[object_name] = len(encoded)
            lvalues.append(f"{text:<45}/* [{offset:3d}] {object_name} */\n")
            entry += f"{len(encoded)},&(lvalues[{offset}]),0"
            offset += len(encoded)
        else:
            entry += "0,NULL,0"
        entry += "},\n"
        entries.append(entry)

    indices = [index for index in range(count + 1) if index in nid]
    sn_lines = [f'{index:2d},\t/* "{short[nid[index]]}" */\n' for index in sorted((i for i in indices if nid[i] in short), key=lambda i: short[nid[i]].encode("latin-1"))]
    ln_lines = [f'{index:2d},\t/* "{long[nid[index]]}" */\n' for index in sorted((i for i in indices if nid[i] in long), key=lambda i: long[nid[i]].encode("latin-1"))]
    object_indices = [i for i in indices if nid[i] in objects]
    object_indices.sort(key=lambda i: (der_length.get(objects[nid[i]], 0), der_text.get(objects[nid[i]], "").encode("latin-1")))
    ob_lines = []
    for index in object_indices:
        object_name = objects[nid[index]]
        arcs = definitions[object_name].replace("L", "").replace(",", " ")
        ob_lines.append(f"{index:2d},\t/* {object_name:<32} {arcs} */\n")

    out = [heredoc(f"{directory}/obj_dat.pl")]
    out.append(f"#define NUM_NID {count}\n")
    out.append(f"#define NUM_SN {len(sn_lines)}\n")
    out.append(f"#define NUM_LN {len(ln_lines)}\n")
    out.append(f"#define NUM_OBJ {len(ob_lines)}\n\n")
    out.append(f"static const unsigned char lvalues[{offset + 1}]={{\n")
    out.extend(lvalues)
    out.append("};\n\n")
    out.append("static const ASN1_OBJECT nid_objs[NUM_NID]={\n")
    for entry in entries:
        if len(entry) > 75:
            fields = entry.split(",")
            while fields and fields[-1] == "":
                fields.pop()
            line = ""
            for field in fields:
                candidate = line + field + ","
                if len(candidate) > 70:
                    out.append(line + "\n")
                    candidate = "\t" + field + ","
                line = candidate
            out.append(line[:-1])
        else:
            out.append(entry)
    out.append("};\n\n")
    out.append("static const unsigned int sn_objs[NUM_SN]={\n")
    out.extend(sn_lines)
    out.append("};\n\n")
    out.append("static const unsigned int ln_objs[NUM_LN]={\n")
    out.extend(ln_lines)
    out.append("};\n\n")
    out.append("static const unsigned int obj_objs[NUM_OBJ]={\n")
    out.extend(ob_lines)
    out.append("};\n\n")
    with open(output_path, "w", encoding="latin-1", newline="") as file:
        file.write("".join(out))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit("usage: LibreSSLObjects.py objects-directory obj_mac.h obj_dat.h")
    objects_to_mac(sys.argv[1], sys.argv[2])
    mac_to_dat(sys.argv[1], sys.argv[2], sys.argv[3])
