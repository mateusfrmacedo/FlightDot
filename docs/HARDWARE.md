# Hardware verificado — Waveshare 1.43

Fontes consultadas antes de escrever o driver (09/09/2026):

- [Wiki oficial](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43)
- [Demo V3 oficial](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43/ESP32-S3-AMOLED-1.43-Demo-V3.zip), `Arduino/examples/07_LVGL_Test/{lcd_config.h,lcd_bsp.c,FT3168.cpp,read_lcd_id_bsp.c}`.
- [Esquema elétrico oficial](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43/ESP32-S3-Touch-AMOLED-1.43.pdf)

O exemplo V3 suporta **dois painéis na 1.43**: SH8601 (`0xDA → 0x86`) e CO5300 (ramo alternativo, `0xFF`). CO5300 exige **offset X +6**. Não se reutilizou o driver/pinagem de 1.75. A identificação `0xFF` é o fallback usado pelo fabricante, não uma leitura inequívoca de silício; painel desconectado também pode retornar FF. O teste visual no dispositivo confirmou a inicialização, geometria e RGB.

Touch: **FT3168 é o controlador documentado e o nome do driver oficial V3**, I²C `0x38`, contador de toques em `0x02`, coordenadas de 12 bits em `0x03..0x06`. Há listagens oficiais recentes de variantes FT6146; os registradores observados abaixo não são usados para afirmar uma identificação de silício sem tabela oficial. O protocolo foi confirmado no barramento; teste de gestos é necessário no exemplar real.

| Sinal | GPIO / endereço |
|---|---|
| QSPI CS | 9 |
| QSPI clock | 10 |
| QSPI D0 / D1 / D2 / D3 | 11 / 12 / 13 / 14 |
| AMOLED reset | 21 |
| OLED enable | 42 |
| I²C SDA / SCL (touch, RTC, IMU) | 47 / 48 |
| Touch FT3168 (protocolo) | 0x38, polling, sem IRQ/reset separado no exemplo |
| RTC PCF85063 | 0x51, IRQ GPIO15 (não usado) |
| IMU QMI8658 | IRQ GPIO8 (não usado) |
| ADC VIN | GPIO4, divisor 200k/100k, fator 3 |
| USB D− / D+ | 19 / 20 |

O carregador no esquema é **ETA6098**, não AXP2101. O ADC mede a tensão do sistema/VIN (incluindo USB). A leitura continua disponível internamente, mas foi retirada do HUD por solicitação do usuário. O brilho é fixo e o auto-dim está desativado; orientação pela IMU não foi habilitada.

## Exemplar conectado

`esptool flash-id`: ESP32-S3 revisão 0.2, PSRAM embutida 8 MB, flash QSPI 16 MB (ID fabricante 20 / dispositivo 4018). USB JTAG/Serial, `/dev/cu.usbmodem1101` no Mac deste teste.

Serial do firmware de teste:

```
[boot] flash=16777216 psram=8388608
[display] ID=0xFF CO5300 (official ID fallback), 466x466
[touch] FT3168 protocol @0x38: ACK
[touch] reg 0xa3 = 0x64
[touch] reg 0xa8 = 0x11
```

O usuário confirmou texto e blocos RGB corretos no teste mínimo. O backup integral anterior à gravação está em `backups/original-16MB.bin` (16.777.216 bytes; ignorado no Git pois pode conter credenciais).
