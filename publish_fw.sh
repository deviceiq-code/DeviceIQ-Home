#!/usr/bin/env bash
set -euo pipefail

### ========= CONFIG =========
PROJECT_NAME="deviceiq-home"                           # prefixo do bin publicado
VERSION_H="./include/Version.h"                        # caminho para Version.h
FIRMWARE_SRC="./.pio/build/${PROJECT_NAME}/firmware.bin"

WEB_ROOT="/var/www/html"                               # onde fica o update.json servido (ex.: 8081)
BIN_DIR="/var/www/html/bin"                            # subpasta onde os .bin serão servidos
BASE_URL="http://server.dts-network.com:8082"          # URL pública (host:porta)
BIN_URL_PREFIX="${BASE_URL}/bin"                       # prefixo público da subpasta /bin
UPDATE_JSON_LOCAL="./update.json"                      # update.json na pasta atual (modelo/alvo)
PERM="0644"                                            # -rw-r--r--
### ===========================

# dependências
for dep in jq curl sha256sum; do
  command -v "$dep" >/dev/null 2>&1 || { echo "ERRO: '$dep' não encontrado."; exit 1; }
done
INSTALL_BIN="$(command -v install || true)"

SUDO=""
if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then SUDO="sudo"; fi

# garante que a subpasta bin exista no servidor
$SUDO mkdir -p "$BIN_DIR"

# --- ler versão do Version.h ---
[[ -f "$VERSION_H" ]] || { echo "ERRO: $VERSION_H não encontrado."; exit 1; }

get_num() {  # Major/Minor/Revision
  grep -E "^\s*const\s+uint8_t\s+$1\s*=" "$VERSION_H" \
   | head -n1 | sed -E 's/.*=\s*([0-9]+)\s*;.*/\1/'
}
get_str() {  # ProductName
  grep -E "^\s*const\s+String\s+$1\s*=\s*\"" "$VERSION_H" \
   | head -n1 | sed -E 's/.*"\s*([^"]+)\s*".*/\1/'
}

MAJOR="$(get_num Major)"; MINOR="$(get_num Minor)"; REVISION="$(get_num Revision)"
[[ -n "$MAJOR" && -n "$MINOR" && -n "$REVISION" ]] || { echo "ERRO: não consegui ler Major/Minor/Revision de $VERSION_H"; exit 1; }
NEW_VER="${MAJOR}.${MINOR}.${REVISION}"

PRODUCT_NAME="$(get_str ProductName || true)"
PRODUCT_NAME="${PRODUCT_NAME:-ESP32-Pro}"

echo "[INFO] Versão nova: $NEW_VER  |  ProductName: $PRODUCT_NAME"

# --- validar firmware origem (LOCAL) ---
[[ -f "$FIRMWARE_SRC" ]] || { echo "ERRO: firmware não encontrado em $FIRMWARE_SRC"; exit 1; }

# --- publicar BIN (somente no SERVIDOR) ---
ARTIFACT="${PROJECT_NAME}-${NEW_VER}.bin"
DEST_BIN="${BIN_DIR}/${ARTIFACT}"
echo "[INFO] Publicando BIN: $FIRMWARE_SRC -> $DEST_BIN"

# copia e define permissão SOMENTE no destino (servidor)
if [[ -n "$INSTALL_BIN" ]]; then
  $SUDO "$INSTALL_BIN" -m "$PERM" "$FIRMWARE_SRC" "$DEST_BIN"
else
  $SUDO cp "$FIRMWARE_SRC" "$DEST_BIN"
  $SUDO chmod "$PERM" "$DEST_BIN"
fi

# --- SHA256 do arquivo no servidor (filesystem) ---
FILE_SHA="$($SUDO sha256sum "$DEST_BIN" | awk '{print $1}')"
echo "[INFO] SHA256 (arquivo): $FILE_SHA"

# --- SHA256 do conteúdo SERVIDO via HTTP (garante ausência de gzip/proxy) ---
ARTIFACT_URL="${BIN_URL_PREFIX}/${ARTIFACT}"
SERVED_SHA="$(curl -sS -H 'Accept-Encoding: identity' "$ARTIFACT_URL" | sha256sum | awk '{print $1}')"
if [[ -z "${SERVED_SHA:-}" ]]; then
  echo "ERRO: não consegui obter o binário servido de $ARTIFACT_URL"
  exit 1
fi
echo "[INFO] SHA256 (servido): $SERVED_SHA"

# --- comparar hashes ---
if [[ "$FILE_SHA" != "$SERVED_SHA" ]]; then
  echo "ERRO: SHA do arquivo ($FILE_SHA) difere do SHA servido ($SERVED_SHA)."
  echo "      Verifique compressão (gzip) no Apache e o docroot/BASE_URL."
  exit 1
fi
NEW_SHA="$SERVED_SHA"

# --- versionar update.json atual do SERVIDOR (se existir) ---
OLD_JSON="$WEB_ROOT/update.json"
if $SUDO test -f "$OLD_JSON"; then
  set +e
  OLD_VER="$($SUDO jq -r '."Version" // empty' "$OLD_JSON" 2>/dev/null)"
  set -e
  if [[ -n "${OLD_VER:-}" ]]; then
    echo "[INFO] Versionando update.json atual -> $WEB_ROOT/update.json.${OLD_VER}"
    $SUDO mv "$OLD_JSON" "$WEB_ROOT/update.json.${OLD_VER}"
  else
    TS="$(date +%Y%m%d-%H%M%S)"
    echo "[WARN] Não consegui ler 'Version' do update.json atual; salvando como backup com timestamp."
    $SUDO mv "$OLD_JSON" "$WEB_ROOT/update.json.bak-${TS}"
  fi
else
  echo "[INFO] Nenhum update.json atual em $WEB_ROOT (primeira publicação?)"
fi

# --- atualizar/criar update.json LOCAL (NÃO altera permissão local) ---
NEW_URL="$ARTIFACT_URL"
if [[ -f "$UPDATE_JSON_LOCAL" ]]; then
  echo "[INFO] Atualizando $UPDATE_JSON_LOCAL (local)"
  tmp="$(mktemp)"
  jq --arg model "$PRODUCT_NAME" \
     --arg ver   "$NEW_VER" \
     --arg min   "$NEW_VER" \
     --arg url   "$NEW_URL" \
     --arg sha   "$NEW_SHA" \
     '.["Model"]       = $model
    | .["Version"]     = $ver
    | .["Min Version"] = $min
    | .["URL"]         = $url
    | .["SHA256"]      = $sha' \
     "$UPDATE_JSON_LOCAL" > "$tmp"
  mv "$tmp" "$UPDATE_JSON_LOCAL"   # mantemos as permissões locais como estão
else
  echo "[INFO] Criando $UPDATE_JSON_LOCAL (local)"
  cat > "$UPDATE_JSON_LOCAL" <<EOF
{
  "Model": "${PRODUCT_NAME}",
  "Version": "${NEW_VER}",
  "Min Version": "${NEW_VER}",
  "URL": "${NEW_URL}",
  "SHA256": "${NEW_SHA}"
}
EOF
fi

# --- publicar update.json no SERVIDOR com permissão 0644 ---
DEST_JSON="$WEB_ROOT/update.json"
echo "[INFO] Publicando $UPDATE_JSON_LOCAL -> $DEST_JSON (servidor)"
if [[ -n "$INSTALL_BIN" ]]; then
  $SUDO "$INSTALL_BIN" -m "$PERM" "$UPDATE_JSON_LOCAL" "$DEST_JSON"
else
  $SUDO cp "$UPDATE_JSON_LOCAL" "$DEST_JSON"
  $SUDO chmod "$PERM" "$DEST_JSON"
fi

chmod 777 update.json

# --- relatório final (somente arquivos do servidor) ---
echo "============================================================"
echo "[OK] Publicação concluída"
echo "  Versão:        $NEW_VER"
echo "  BIN:           $DEST_BIN"
echo "  URL:           $NEW_URL"
echo "  SHA256:        $NEW_SHA"
echo "  update.json -> $DEST_JSON"
echo "  Permissões (servidor):"
$SUDO stat -c '%a %n' "$DEST_BIN" "$DEST_JSON" 2>/dev/null || true
echo "============================================================"
echo "[DICA] Desative gzip para .bin/.json no Apache para manter SHA consistente:"
echo "<IfModule mod_deflate.c>"
echo "  SetEnvIfNoCase Request_URI '\\.(json|bin)\$' no-gzip=1"
echo "</IfModule>"
