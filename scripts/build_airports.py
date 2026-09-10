"""Build the offline search index from public-domain OurAirports data."""
import csv, gzip, io, json, pathlib, urllib.request
# Use the original OurAirports distribution and retain its source fields.
URL = 'https://davidmegginson.github.io/ourairports-data/airports.csv'
with urllib.request.urlopen(URL, timeout=60) as response:
    rows = csv.DictReader(io.StringIO(response.read().decode('utf-8-sig')))
    airports = []
    for r in rows:
        if r['type'] == 'closed' or not (r['iata_code'] or r['iso_country'] == 'BR'):
            continue
        try:
            lat, lon = float(r['latitude_deg']), float(r['longitude_deg'])
        except ValueError:
            continue
        airports.append([r['name'], r['municipality'], r['iata_code'], r.get('icao_code') or r['ident'],
                         r['iso_country'], round(lat, 6), round(lon, 6)])
root = pathlib.Path(__file__).resolve().parents[1]
raw = json.dumps(airports, ensure_ascii=False, separators=(',', ':')).encode()
(root/'data/airports.json.gz').write_bytes(gzip.compress(raw, compresslevel=9, mtime=0))
print(f'{len(airports)} airports, {len(raw)} bytes JSON, {(root/"data/airports.json.gz").stat().st_size} bytes gzip')
