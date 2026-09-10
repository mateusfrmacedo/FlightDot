# Referências e licenças

## Capsule Radar

[socquique/capsule-radar](https://github.com/socquique/capsule-radar), MIT, copyright 2026 Quique Tortosa. Referência de arquitetura (worker de rede, radar LVGL, simulador SDL), funcionalidades e build PlatformIO Arduino 3.x. Implementação do aplicativo neste projeto foi escrita separadamente; não se copiou seu driver da 1.75.

## Driver oficial Waveshare / Espressif

`lib/WavesharePanel/src/esp_lcd_sh8601.c` e `.h`: Demo V3 Waveshare 1.43; cabeçalhos SPDX originais preservados (Espressif, Apache-2.0). `read_lcd_id_bsp.c/.h` são do mesmo exemplo oficial; não trazem licença explícita nos arquivos. Mantidos como código de suporte do fabricante, com proveniência documentada. Verifique as condições de redistribuição da Waveshare antes de distribuir um produto comercial.

`scripts/gen_crt_bundle.py`: Espressif ESP-IDF v5.3.2, Apache-2.0, cabeçalho original preservado.

## Dependências

LVGL 8.4.0 (MIT), ArduinoJson 6.21.5 (MIT), Arduino-ESP32 3.1.3 (LGPL e componentes sob outras licenças), ESP-IDF 5.3 (Apache-2.0), PlatformIO / pioarduino, SDL2 (zlib), Unity (MIT). As fontes bitmap `font_pt_*` foram geradas da Montserrat Medium distribuída pelo LVGL, licenciada sob SIL Open Font License 1.1. Consulte as licenças distribuídas com cada biblioteca.

## Certificados

`certs/x509_crt_bundle` gerado em 09/09/2026 a partir do [CA bundle Mozilla distribuído pelo curl](https://curl.se/docs/caextract.html), 121 certificados. O formato é o bundle ESP-IDF 5.3. Não há chaves privadas. Ver `certs/README.md` para renovação.

## Dados externos

airplanes.live / ADSB.lol / [adsb.fi](https://adsb.fi/): dados ADS-B com cobertura variável. Uso pessoal/educacional e moderação de consultas. adsbdb: enriquecimento opcional de companhia, tipo e rota. A busca de cidades usa a API de geocodificação do Open-Meteo somente quando o usuário envia uma pesquisa no navegador.

`src/core/places_generated.h`: 4.580 aeroportos gerados por `scripts/build_places.py`. Fonte: [OurAirports](https://ourairports.com/data/), domínio público; somente tipos `medium_airport` e `large_airport`. Cidades vizinhas não são incorporadas ao mapa. O clima atual do destino usa [Open-Meteo](https://open-meteo.com/en/docs), CC BY 4.0.


`data/airports.json.gz`: 16.426 registros derivados de [OurAirports](https://ourairports.com/data/), baixados em 09/09/2026. Dados em domínio público; filtro: aeroportos não fechados com código IATA ou no Brasil. Campos: nome, município, IATA, ICAO/identificador, país e coordenadas. Gerador em `scripts/build_airports.py`. Busca de cidades via [Open-Meteo](https://open-meteo.com/en/docs/geocoding-api), com dados [GeoNames](https://www.geonames.org/) e atribuição na página.
