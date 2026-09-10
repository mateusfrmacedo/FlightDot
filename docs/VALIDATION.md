# Validação no Mac e na placa — 10/09/2026

- Hardware: flash 16 MB e PSRAM 8 MB lidos por esptool e pelo firmware.
- Driver: inicialização CO5300 pelo ramo `ID=0xFF` do exemplo oficial; offset +6; texto e blocos RGB confirmados visualmente pelo usuário.
- Touch: ACK I²C 0x38, registradores A3=64 e A8=11. Protocolo FT3168 do exemplo oficial. Não se inferiu chip exato a partir desses dois registradores sem tabela oficial.
- Build: ambientes `radar`, `display-test`, `mock` e `native` compilados durante o desenvolvimento.
- Testes nativos: 9 casos aprovados (geografia, lista vazia, JSON inválido/truncado, campos opcionais/ground/emergência/categoria, 1.200 aeronaves com limite nearest-80, validação de configuração, identificadores, busca global e referências geográficas).
- Desktop: mesma UI compilada com SDL2, renderização inspecionada em 466×466. Capturas em `artifacts/` (geradas localmente).
- Rede real: ESP32 reconectou ao Wi-Fi salvo, obteve `192.168.3.39`, respondeu HTTP e recebeu aeronaves por `api.adsb.lol` com erro vazio e atualizações sucessivas. Quantidade varia com o tráfego; foram observadas 1 e 5 aeronaves.
- O provedor airplanes.live retornou restrição HTTP403 no Mac e falha de validação da cadeia TLS no ESP32. A verificação TLS foi mantida; foi adicionado o uso direto da raiz confiável GTS Root R4 para os dois hosts Cloudflare. Após essa correção, o ESP32 validou a cadeia do airplanes.live e recebeu o mesmo HTTP403 com pedido de contato; o fallback prosseguiu normalmente. Fallbacks são ADSB.lol e adsb.fi; o segundo foi validado por consulta HTTP no Mac, sem forçar uma indisponibilidade do primeiro na placa.
- Configuração: mapa e valores inspecionados no navegador; POST sem token →403, latitude `nan` →400, alcance 251 →400, gravação válida →200.
- Persistência: valores comparados antes e depois de reboot OTA, iguais.
- OTA real: `firmware.bin` enviado ao servidor da própria placa com autenticação, retorno HTTP200, reboot e retomada de conexão/dados. Backup original mantido.
- Binário principal aproximadamente 1,67 MB; ocupação estática da RAM interna aproximadamente 32%. LVGL usa PSRAM; após consulta HTTPS foram observados cerca de 85 KB de heap interno livre.

As verificações acima não substituem um ensaio prolongado de autonomia, perda de energia durante OTA ou calibração mecânica do touch. RTC usa protocolo oficial; carga da bateria não é estimada. Sleep pela IMU permanece opcional não implementado. Enriquecimento/cache de rotas está implementado, mas nem toda aeronave/callsign tem rota disponível no adsbdb.

## Ajustes visuais solicitados

Raio 228 px, margem aproximada de 5 px, anéis/grade 5 px e contorno externo 6 px. Fontes de HUD/etiquetas 16 px, corpo 18 px e títulos 24–28 px. Aviões ampliados em 50% para aproximadamente 30 px, traço 3 px e seleção por toque com raio de 30 px.

A referência foi extraída do vídeo fornecido pelo usuário: frente verde luminosa e estreita, com rastro contínuo em degradê de aproximadamente 55 graus. A varredura completa uma volta em 6 segundos. O HUD é transparente, sem retângulos pretos. Um mapa geométrico em PSRAM e tabelas de cor permitem desenhar grade e efeito juntos no buffer LVGL. Invalidação parcial limita cada atualização ao rastro, aviões em movimento e áreas de texto; mudanças de dados ou de tela invalidam a tela inteira.

Builds `radar` e `native` aprovados após os ajustes. Prévia com os ícones e textos ampliados inspecionada em `artifacts/radar-larger.png`. A atualização alvo permanece 30 Hz; o campo `fps` em `/api/status` mede a taxa efetiva da placa.

A versão anterior com imagens rotacionadas atingia cerca de 10–13 FPS. Com desenho direto e invalidação parcial foram medidos 20–25 FPS na versão final, com uma aeronave e consultas HTTPS ativas. A taxa depende da quantidade de aeronaves e área redesenhada; o alvo do timer não garante 30 FPS.

Uma transferência OTA desta revisão foi interrompida; a atualização foi concluída por USB, com verificação de escrita pelo esptool. O Wi-Fi salvo foi recuperado e o fallback `opendata.adsb.fi` entregou dados válidos com erro vazio. O HTTP403 do airplanes.live e HTTP429 do ADSB.lol recebem cooldown de 10 minutos; o segundo fallback foi confirmado no hardware. Na verificação final após ampliar fontes e ícones, seis amostras registraram Wi-Fi conectado, uma aeronave via ADSB.lol, erro vazio e atualizações sucessivas. Configurações preservadas: centro Orindiúva, alcance 100 km, tema verde e varredura ativa.

## Busca, acompanhamento e brilho fixo

- Brilho automático retirado do loop; o campo legado NVS é carregado como falso. Hora/data mantidas, textos Wi-Fi/Vsys/radar.local removidos do HUD. Indicadores N/S/L/O conferidos em `artifacts/radar-cardinals.png`.
- Base OurAirports: 16.426 registros, 471.964 bytes gzip na flash. Pesquisa `SJP` no navegador retornou Prof. Eribelto Manoel Reino, SJP/SBSR, São José do Rio Preto. Cidades são pesquisadas por Open-Meteo/GeoNames e os resultados usam texto DOM seguro.
- Oito testes nativos aprovados, incluindo normalização de números/callsigns, rejeição de identificadores inválidos e consulta global que mantém a rejeição de posições antigas. Builds radar/native aprovados e JavaScript passou na verificação de sintaxe.
- Teste de LA3819 expôs resolução LAN3819 no adsbdb enquanto a aeronave ativa transmitia TAM3819. A busca LA passou a consultar ambos e rejeitar seleção ambígua; após encontrar um único avião, o acompanhamento usa seu hexadecimal ICAO. Endpoints com múltiplos callsigns foram verificados em ADSB.lol e adsb.fi.
- Um teste em hardware identificou `SSL - Memory allocation failed`. O SDK usa mbedTLS com alocação interna por padrão; os callbacks oficiais `mbedtls_platform_set_calloc_free` foram configurados antes do Wi-Fi para usar PSRAM, preservando a validação TLS e a RAM interna dos buffers DMA.

- Após a correção de memória, teste real na placa: busca `LA3819` encontrou `TAM3819`, hex `e49764`, via `opendata.adsb.fi`; coordenadas avançaram de -21.544098/-48.961370 para -21.529861/-48.959023, erro vazio, cerca de 25–27 FPS e 71–74 KB de heap interno livre. ADSB.lol retornou 429 durante o teste e o fallback assumiu. O navegador exibiu “Seguindo LA3819 · TAM3819”.
- “Parar de seguir” confirmado na interface; consultas de raio retomadas e centro Orindiúva preservado. GET de configuração confirmou brilho 150 e autoDim=false. Alterações de alcance feitas durante o uso foram mantidas. POST de acompanhamento sem token retornou 403; número sem companhia e entrada com caminho retornaram 400.
- Binário final cerca de 2,19 MB (35% da partição OTA de 6 MB), RAM estática 115 KB (35%). Atualização USB concluída com hash verificado.

## Cidades, aeroportos, detalhes e temas

- A tela usa a rotação LVGL `270°` adotada no exemplo oficial, com touch associado ao display rotacionado, para posicionar a USB-C na parte inferior.
- Índice reduzido para 4.580 aeroportos médios e grandes; cidades vizinhas foram removidas integralmente do mapa. A seleção mantém até 56 aeroportos próximos e mostra até 12 nomes/códigos sem colisão por quadro. `Rio Preto SBSR` está coberto pelo teste geográfico.
- Os temas fósforo, âmbar, vermelho, azul e verde neon foram compilados e inspecionados no simulador. O antigo tema de grade pontilhada foi removido e a configuração NVS é migrada automaticamente.
- O cartão selecionado foi inspecionado em `artifacts/flightdot-details.png`: conteúdo centralizado, texto ampliado, selo maior, cidade de destino, ETA/tempo de voo estimados e temperatura atual do destino. O HUD não é desenhado sobre o cartão.
- A tela principal foi inspecionada sem trajetórias, pontos de aeroporto, números de distância, contador ou alcance. A interpolação usa posições preparadas uma vez por atualização e limita a quantidade desenhada nos maiores alcances.
- Categorias ADS-B A7, B2 e B6 são desenhadas como helicóptero, balão/dirigível e drone. Companhia/categoria recebe selo textual; imagens de marcas não fazem parte do binário.
- Firmware final desta correção: 2.501.024 bytes (39,8% da partição OTA de 6 MB), RAM estática 121.984 bytes (37,2%). A rotação genérica síncrona foi substituída por transposição direta em buffer DMA, e a atualização da lista de locais passou a ocorrer somente quando o centro se desloca 2 km ou o alcance muda. Gravação USB concluída com hash verificado em `/dev/cu.usbmodem1101`.
- Após a gravação, a placa reconectou e recebeu 2–3 aeronaves por ADSB.lol. Um HTTP 429 acionou o fallback, que respondeu por adsb.fi; nenhum voo simulado foi usado.
- Após a correção da rotação, a placa permaneceu estável durante a observação serial, com aproximadamente 17–20 atualizações efetivas por segundo, cerca de 51 KB de heap interno livre e consultas ADS-B ativas. O recálculo dos locais deixou de ocorrer a cada resposta da API, eliminando essa pausa periódica.
- A revisão sem cidades e com fallback de glifos portugueses foi gravada por USB. Foram observadas 15–27 atualizações efetivas por segundo com 1–2 aeronaves e consultas HTTPS ativas; os 9 testes nativos passaram. O firmware ocupa 2.381.848 bytes (37,9% da partição OTA) e 122.024 bytes de RAM estática.
