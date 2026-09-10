# Bundle TLS

O firmware valida os certificados HTTPS e aguarda RTC válido ou NTP. Não utiliza `setInsecure()`.

Atualize periodicamente (na raiz do projeto):

```sh
curl --fail --location https://curl.se/ca/cacert.pem -o /tmp/plano-cacert.pem
.venv/bin/pip install cryptography
(cd certs && ../.venv/bin/python ../scripts/gen_crt_bundle.py --input /tmp/plano-cacert.pem)
.venv/bin/pio run -e radar
```

Gerador fixado em [ESP-IDF v5.3.2](https://github.com/espressif/esp-idf/blob/v5.3.2/components/mbedtls/esp_crt_bundle/gen_crt_bundle.py).

`gts-root-r4.pem` é a raiz GTS Root R4 extraída do mesmo bundle Mozilla. Para os hosts Cloudflare (airplanes.live e adsb.fi), o firmware usa essa raiz PEM diretamente, evitando a falha observada no callback de bundle do core Arduino 3.1.3. A cadeia, o hostname e a validade continuam sendo verificados. Atualize também esse PEM quando a cadeia dos provedores mudar.
