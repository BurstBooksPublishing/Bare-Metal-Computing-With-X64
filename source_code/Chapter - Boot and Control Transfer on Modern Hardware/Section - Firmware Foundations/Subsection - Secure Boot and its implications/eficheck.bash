#!/usr/bin/env bash
set -euo pipefail
# GUID used by UEFI variables (UEFI spec).
GUID="8be4df61-93ca-11d2-aa0d-00e098032b8c"
efivar_dir="/sys/firmware/efi/efivars"

# read one-byte data after 4-byte attributes
read_var() {
  local name="$1"
  local file="${efivar_dir}/${name}-${GUID}"
  if [[ ! -r "$file" ]]; then
    printf '%s: not present\n' "$name"
    return 1
  fi
  # Skip attributes (4 bytes) and read first data byte.
  dd if="$file" bs=1 skip=4 count=1 status=none | od -An -t u1 | tr -d '[:space:]'
}

for v in "SecureBoot" "SetupMode" "PK" "KEK" "db" "dbx"; do
  val=$(read_var "$v" 2>/dev/null) || val="N/A"
  printf '%-10s: %s\n' "$v" "$val"
done
# Exit 0 if SecureBoot enabled (value 1), non-zero otherwise.
sb=$(read_var "SecureBoot" 2>/dev/null || echo 0)
if [[ "$sb" -eq 1 ]]; then
  echo "Secure Boot: ENABLED"
else
  echo "Secure Boot: DISABLED or unavailable"
  exit 2
fi