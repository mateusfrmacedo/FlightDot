"""Generate the on-device major-airport landmark index.

Airport data comes from OurAirports (public domain). City searching remains
available through the web UI, but surrounding cities are intentionally absent
from the radar to keep wide-range rendering responsive.
"""
import csv
import io
import pathlib
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]


def text(url):
    with urllib.request.urlopen(url, timeout=90) as response:
        return response.read()


def quoted(value, limit):
    value = value.strip().replace("\\", " ").replace('"', "'")[:limit]
    return '"' + value + '"'


places = []

rows = csv.DictReader(io.StringIO(text(
    "https://davidmegginson.github.io/ourairports-data/airports.csv").decode("utf-8-sig")))
for row in rows:
    # The round display only needs airports that serve regional/commercial
    # traffic. Small strips, heliports and private fields remain searchable in
    # the web index but do not clutter the radar itself.
    if row["type"] not in {"medium_airport", "large_airport"}:
        continue
    code = row["icao_code"] or row["ident"]
    # All Brazilian airports plus international airports that have an IATA code.
    if row["iso_country"] != "BR" and not row["iata_code"]:
        continue
    municipality = row["municipality"] or row["name"]
    if municipality == "São José do Rio Preto":
        municipality = "Rio Preto"
    places.append((round(float(row["latitude_deg"]) * 100000),
                   round(float(row["longitude_deg"]) * 100000),
                   municipality, code, "A"))

places = sorted(set(places), key=lambda x: (x[4], x[2].casefold(), x[3]))
out = ROOT / "src/core/places_generated.h"
with out.open("w", encoding="utf-8") as f:
    f.write("#pragma once\n#include <stdint.h>\n")
    f.write("struct PlaceRecord { int32_t latE5, lonE5; const char *name; const char *code; char kind; };\n")
    f.write("static const PlaceRecord PLACE_RECORDS[] = {\n")
    for lat, lon, name, code, kind in places:
        f.write(f"  {{{lat}, {lon}, {quoted(name, 34)}, {quoted(code, 8)}, '{kind}'}},\n")
    f.write("};\nstatic constexpr unsigned PLACE_RECORD_COUNT = sizeof(PLACE_RECORDS) / sizeof(PLACE_RECORDS[0]);\n")
print(f"{len(places)} places -> {out}")
