#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
font_source=${1:-".pio/libdeps/radar/lvgl/scripts/built_in_font/Montserrat-Medium.ttf"}

if [ ! -f "$font_source" ]; then
  echo "Fonte Montserrat do LVGL não encontrada. Execute pio run -e radar primeiro." >&2
  exit 1
fi

for size in 16 18 24 28 42; do
  npx --yes lv_font_conv \
    --font "$font_source" \
    --symbols "áàâãäéèêëíìîïóòôõöúùûüçÁÀÂÃÄÉÈÊËÍÌÎÏÓÒÔÕÖÚÙÛÜÇº°" \
    --size "$size" --bpp 4 --no-compress --no-kerning --format lvgl --lv-include lvgl.h \
    --lv-fallback "lv_font_montserrat_$size" \
    --lv-font-name "font_pt_$size" \
    --output "src/ui/font_pt_$size.c"
done
