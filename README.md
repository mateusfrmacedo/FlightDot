# FlightDot — radar ADS-B para ESP32-S3 AMOLED 1.43"

**The sky in your pocket.**

Radar ADS-B pessoal com Arduino, PlatformIO e LVGL, para **Waveshare ESP32-S3-Touch-AMOLED-1.43, 466×466, flash 16 MB, PSRAM OPI 8 MB**. Centro inicial: **Orindiúva (-20.183608, -49.354661)**; alcance **150 km**, incluindo São José do Rio Preto, a aproximadamente 70 km. Não é um receptor de rádio ADS-B: precisa de Wi-Fi e consulta serviços de dados pela internet.

Repositório oficial: [github.com/mateusfrmacedo/FlightDot](https://github.com/mateusfrmacedo/FlightDot).

## Usar a placa

1. Ligue por USB. Uma animação mostra **FlightDot** e um avião atravessando o centro antes de abrir o radar. No primeiro uso, conecte o celular à rede **FlightDot-Setup**.
2. O portal deve abrir automaticamente. Se não abrir, acesse **http://192.168.4.1/**. Informe o nome e a senha da sua rede **2,4 GHz**.
3. Volte à rede de casa e abra **http://flightdot.local/**. Se o roteador não oferecer mDNS, use o IP informado pelo monitor serial.
4. A página permite pesquisar cidade/aeroporto, seguir um voo e mudar alcance (10–250 km), tema, brilho, fuso e limite de atraso. O minimapa e os campos manuais de latitude/longitude foram retirados.

Os voos só aparecem após conexão, sincronização do relógio e uma resposta válida da API. **Zero aeronaves é um resultado válido**; a cobertura depende dos receptores da região. Falhas preservam a última posição e mostram `DADOS ATRASADOS` depois do limite configurado (padrão: 20 s). O ambiente `mock` é explicitamente marcado como demonstração e nunca é ativado automaticamente em caso de erro da API.

## Configurações disponíveis

| Opção | Padrão | Intervalo / efeito |
|---|---:|---|
| Centro | Orindiúva | Definido pela pesquisa de cidade ou aeroporto; nome e coordenadas ficam em NVS |
| Alcance | 150 km | 10–250 km pela página; gestos alternam 50/100/150/250 km |
| Tema | Fósforo verde | Fósforo, âmbar, vermelho, azul ou verde neon |
| Brilho | 150 | 10–255, fixo até nova alteração |
| Dados atrasados | 20 s | 10–300 s |
| Varredura | Ligada | Habilita o feixe e seu rastro em degradê |
| Fuso | `<-03>3` | Regra POSIX; a página pode usar o fuso informado pelo navegador |

As configurações e a senha OTA ficam em NVS. As credenciais Wi-Fi são armazenadas pela pilha Wi-Fi do ESP32 e não aparecem no código-fonte. O firmware usa `FlightDot-Setup`, `flightdot.local`, servidor HTTP na porta 80 e consultas HTTPS externas.

## Pesquisar um local ou seguir um voo

Abra a seção **Pesquisar** na página local:

- **Cidade ou aeroporto**: digite o nome da cidade, nome do aeroporto, IATA (ex.: `SJP`) ou ICAO (`SBSR`). Clique em **Centralizar** no resultado; as coordenadas são salvas e um acompanhamento ativo é encerrado. A busca de cidades depende da internet do navegador e usa Open-Meteo/GeoNames, somente por envio do formulário, com cache de 24 h. A base local de aeroportos inclui os registros abertos do Brasil e aeroportos internacionais com código IATA.
- **Seguir avião**: informe companhia e número (`LA3819`, `G31600`) ou callsign ICAO (`TAM3819`, `AZU4321`). O worker resolve o número comercial via adsbdb quando necessário. Para LA, consulta TAM e LAN juntos e só escolhe quando há uma única aeronave com posição recente. Mais de um resultado pede callsign exato; nenhuma posição disponível mantém o aviso de busca/perda de sinal.
- Durante o acompanhamento, a tela mostra a aeronave selecionada centralizada e as referências geográficas se movem com ela. Depois da primeira identificação, as consultas usam o hexadecimal ICAO da mesma aeronave, inclusive fora do alcance da cidade salva. O centro original fica preservado em NVS. **Parar de seguir** retorna a ele.
- Acompanhamento continua com o navegador fechado, mas termina ao reiniciar a placa. A posição recebida não é uma garantia de rota comercial: números compartilhados e diferenças de callsign podem exigir o identificador ICAO. Posições sem recepção recente não são inventadas.

A base `data/airports.json.gz` é enviada comprimida diretamente da flash e pesquisada no navegador, sem carregar todos os aeroportos na RAM do ESP32. Para renovar a base: `python scripts/build_airports.py`, depois compile e grave novamente. Fontes: [OurAirports](https://ourairports.com/data/), [Open-Meteo](https://open-meteo.com/en/docs/geocoding-api), [adsbdb](https://github.com/mrjackwills/adsbdb) e [API adsb.fi](https://github.com/adsbfi/opendata).

## Gestos

- Toque em avião (ou linha na Lista): detalhes e consulta opcional de rota/tipo.
- Toque no cartão: voltar.
- Duplo toque no fundo: alcance 50 → 100 → 150 → 250 km.
- Toque longo: alterna entre os cinco temas.
- Swipe horizontal: Radar / Lista / Estatísticas.
- Toque no rodapé da Lista: próxima página de aeronaves.

O radar tem norte para cima e indicadores N/S/L/O nas bordas, quatro anéis com traços de 5–6 px, raio visual de 228 px e varredura contínua em degradê. Os números de distância, o alcance e a quantidade de voos foram retirados da tela principal; alcance e total recebido aparecem em **Estatísticas**. Uma rotação direta no buffer DMA deixa a USB-C na parte de baixo sem a espera síncrona da rotação genérica do LVGL. Aviões, helicópteros (A7), balões/dirigíveis (B2) e drones (B6) têm desenhos distintos. Os cinco temas são fósforo verde, âmbar, vermelho, azul e verde neon.

A flash contém 4.580 aeroportos médios e grandes gerados do OurAirports. O mapa não desenha cidades vizinhas nem aeroportos pequenos: mostra apenas o nome do centro pesquisado e até oito aeroportos relevantes, em formato `Rio Preto  SBSR`. Os aeroportos aparecem como texto, sem bolinhas ou símbolos. Até 56 candidatos próximos são avaliados para evitar colisão entre os rótulos. Atualize o índice com `python scripts/build_places.py`.

O cartão centralizado da aeronave usa texto ampliado e mostra matrícula, tipo, altitude em metros e pés, velocidade em km/h e nós (`kt`), distância em quilômetros e milhas náuticas, rumo, squawk, destino e cidade de destino. Ele não classifica o voo como particular, serviço, linha aérea ou militar. A temperatura atual do destino vem do Open-Meteo. ETA e tempo de voo são estimados pela distância e velocidade atuais; as APIs públicas usadas não fornecem horário operacional oficial. Logotipos gráficos oficiais não estão embutidos porque exigem um catálogo de imagens e licenças de marca.

As fontes rápidas originais do LVGL recebem pequenos fallbacks `font_pt_16/18/24/28` com os glifos acentuados portugueses. Para regenerá-los depois de instalar as dependências do PlatformIO, execute `scripts/build_fonts.sh`; o gerador usa a Montserrat Medium distribuída com o LVGL.

O firmware não desenha trajetórias percorridas. Entre respostas da API, a posição de cada aeronave é interpolada suavemente sem deixar linhas no mapa.

## Instalar ferramentas e compilar

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install platformio
pio run
pio run -t upload
pio device monitor -b 115200
```

No Mac usado para desenvolvimento, a porta foi `/dev/cu.usbmodem1101`; se necessário:

```sh
pio device list
pio run -e radar -t upload --upload-port /dev/cu.usbmodem1101
```

Ambiente padrão `radar`. A definição genérica `esp32-s3-devkitc-1` recebe overrides explícitos para flash 16 MB, PSRAM OPI, Arduino `qio_opi` e duas partições OTA de 6 MB. O nome informativo da placa no PlatformIO pode continuar mencionando N8; os overrides e os tamanhos impressos no boot são a configuração efetiva. Versões fixadas: pioarduino 53.03.13 / Arduino 3.1.3 / LVGL 8.4.0 / ArduinoJson 6.21.5.

### Pinagem da Waveshare 1.43

| Função | GPIO / endereço |
|---|---|
| AMOLED QSPI CS/CLK | 9 / 10 |
| AMOLED QSPI D0/D1/D2/D3 | 11 / 12 / 13 / 14 |
| AMOLED reset/enable | 21 / 42 |
| I²C SDA/SCL | 47 / 48 |
| Touch FT3168 | I²C `0x38` |
| RTC PCF85063 | I²C `0x51`, IRQ 15 não usado |
| IMU QMI8658 | IRQ 8 não usado |
| Tensão VSYS | ADC GPIO4, divisor 200k/100k |

O exemplar conectado identificou o painel pelo ramo CO5300 do Demo V3 oficial, com offset X de 6 pixels. O mesmo driver também contém o caminho SH8601 quando o ID `0x86` é detectado. Detalhes e fontes oficiais estão em [docs/HARDWARE.md](docs/HARDWARE.md).

## Etapas de validação

```sh
# Teste mínimo: identificação do painel, RGB e coordenadas de toque na serial
pio run -e display-test -t upload

# Radar com 8 aviões simulados, sem consultar APIs
pio run -e mock -t upload

# Firmware normal, sem voos inventados
pio run -e radar -t upload

# Parser, limites e geografia no computador
pio test -e test-native
```

## Simulador LVGL + SDL2

Usa a **mesma UI C++** do firmware, com mouse simulando touch e dados fictícios.

```sh
brew install sdl2
pio run -e native -t exec
```

Captura sem janela, útil para revisar a interface:

```sh
pio run -e native
mkdir -p artifacts
SDL_VIDEODRIVER=dummy .pio/build/native/program artifacts/radar.bmp
```

## APIs e memória

- Consulta HTTPS de raio com `ceil(km / 1.852)` milhas náuticas. Uma requisição ADS-B por ciclo, **5 s depois do término da anterior**.
- Prioridade inicial: `api.airplanes.live`; fallbacks: `api.adsb.lol` e `opendata.adsb.fi` (endpoint `/api/v3/lat/.../lon/.../dist/...`), mantendo o serviço que estiver funcionando.
- No teste de 09/09/2026, airplanes.live retornou pedido de contato/liberação; adsb.lol retornou aeronaves na região. O firmware não depende de uma liberação do primeiro para continuar.
- Falhas têm backoff progressivo de 5–60 s. HTTP 403/429 pausa o provedor por 10 minutos. Sem consultas paralelas ou retries em rajada.
- Parser incremental: no máximo 2 MiB recebidos, 8 KiB por registro, documento filtrado de 4 KiB; mantém as **80 aeronaves mais próximas**. Descarta posições ausentes, inválidas, fora do alcance ou com `seen_pos > 60 s`. Não acumula o JSON completo na RAM. Respostas chunked inesperadas são rejeitadas (o cliente pede HTTP/1.0).
- Para manter a animação fluida, a tela desenha até 80 aeronaves em alcances menores, 60 em 150–199 km e as 40 mais próximas em 200–250 km. Em 250 km aparecem no máximo 16 etiquetas de voo; Estatísticas continua mostrando o total coletado. A associação com a posição anterior ocorre uma vez por atualização, evitando buscas repetidas a cada quadro.
- O alocador mbedTLS é configurado antes de iniciar o Wi-Fi para usar PSRAM, com fallback para RAM interna; os buffers DMA do AMOLED permanecem internos. Isso evita falhas de handshake por falta de blocos contíguos na RAM interna.
- HTTPS valida um bundle de CAs; NTP/RTC válido é necessário. Veja [certs/README.md](certs/README.md).
- Rede roda em worker FreeRTOS, separado do loop de LVGL. Troca snapshots por filas. Respostas de uma configuração de localização anterior são descartadas por geração.
- adsbdb resolve números comerciais uma vez por busca e enriquece detalhes ao selecionar aeronave, no mesmo worker, com intervalo mínimo de 5 s entre consultas. Cache NVS de 24 slots, chave ICAO+callsign, TTL positivo 6 h e negativo 15 min; TTL requer relógio válido. Rota pode não estar disponível.
- Objetos LVGL ficam na PSRAM; buffers DMA da tela ficam na RAM interna. Um mapa geométrico de cerca de 434 KB na PSRAM e tabelas de cor/opacidade permitem desenhar grade e varredura juntas diretamente no buffer LVGL, sem calcular trigonometria por pixel em cada quadro. A rotação de 270° é transposta uma vez para um buffer DMA fixo. A cada quadro, apenas as áreas da varredura e dos aviões em movimento são invalidadas. A lista de aeroportos só é recalculada quando o centro anda pelo menos 2 km ou o alcance muda. O HUD é transparente, sem caixas pretas atrás dos textos.

## Configuração persistente, RTC e energia

Configurações e cache usam namespaces separados em NVS. Credenciais Wi-Fi são mantidas pela pilha Wi-Fi do ESP32. O RTC PCF85063 guarda UTC e é atualizado após NTP. Fuso padrão POSIX `<-03>3` (São Paulo); o navegador sugere regras para fusos conhecidos ou offset fixo para outros. Para regiões com horário de verão, confira a regra POSIX.

O brilho permanece fixo no valor configurado; não há escurecimento por inatividade. O campo legado `autoDim` é forçado a falso ao carregar configurações, preservando o restante da estrutura NVS. A tela principal mostra hora, data e somente avisos necessários; contador, alcance, `Wi-Fi`, `Vsys` e endereço local foram retirados do HUD. O estado da rede e as contagens permanecem disponíveis na página ou em Estatísticas. Sleep por orientação/IMU não foi implementado (opcional). Não há áudio nesta implementação.

## OTA pela página local

Compile `pio run -e radar` e selecione `.pio/build/radar/firmware.bin` em **Atualizar firmware**. O usuário é `admin`; a senha individual persistente aparece na serial no boot. Não use `bootloader.bin`, `partitions.bin` nem imagem de outra placa.

A página exige token de sessão nas alterações e senha para OTA. Firmware é gravado na partição inativa; falha de upload não seleciona a imagem incompleta. Não há rollback automático por falha lógica após boot nem assinatura de firmware; OTA deve ser usado na rede local confiável. Uma atualização interrompida não substitui a partição em execução.

## Arquivos

| Caminho | Responsabilidade |
|---|---|
| `src/hardware/` | AMOLED, touch, RTC e tensão |
| `lib/WavesharePanel/` | Transporte e identificação oficiais Waveshare |
| `src/core/` | Modelos, geografia, parser, mock e storage NVS |
| `src/net/` | Wi-Fi/captive portal, ADS-B/rotas, servidor de configuração/OTA |
| `src/ui/` | Radar, lista, detalhes, estatísticas e gestos |
| `src/sim_main.cpp` | Simulador desktop SDL2 |
| `test/test_core/` | Testes do parser, geografia e validação |
| `docs/HARDWARE.md` | Fontes, pinagem e leituras do exemplar testado |

## Recuperação

Se a porta desaparecer, segure BOOT, pressione/release RESET e solte BOOT; volte a executar upload. O backup completo anterior à primeira gravação está em `backups/original-16MB.bin` no workspace local. Para restaurar **inclusive NVS original**:

```sh
pip install esptool
esptool --chip esp32s3 --port /dev/cu.usbmodem1101 write-flash 0 backups/original-16MB.bin
```

Isso substitui toda a flash e perde as configurações do FlightDot. SHA256 do backup: `ab03ee55fc258eea1bfb20bf6ea62f0ecdb77bd47b76a5099227df709e02d803`.

Dados adicionais: [adsb.fi](https://adsb.fi/), uso pessoal e não comercial, conforme [termos da API](https://github.com/adsbfi/opendata).

## Referências

[Capsule Radar](https://github.com/socquique/capsule-radar) foi referência arquitetural e funcional. [Waveshare 1.43](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43) e seu Demo V3 foram a fonte dos drivers e da pinagem. Veja [THIRD_PARTY.md](THIRD_PARTY.md). As instruções adicionais do documento anexado foram tratadas como referência: não foram incorporados requisitos extras como quatro temas ou áudio que não constavam da solicitação principal.

Coordenadas aproximadas dos centros urbanos: [Prefeitura de Orindiúva](https://orindiuva.sp.gov.br/wp-content/uploads/2025/04/LAUDO-VTN-ORINDIUVA.pdf) e [IBGE Rio Preto](https://geoftp.ibge.gov.br/cartas_e_mapas/mapas_municipais/colecao_de_mapas_municipais/2020/SP/sao_jose_do_rio_preto/3549805_MM.pdf). Ajuste no mapa para representar sua localização exata.
