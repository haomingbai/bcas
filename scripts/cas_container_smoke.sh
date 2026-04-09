#!/usr/bin/env bash
set -euo pipefail

IMAGE="${BCAS_CAS_IMAGE:-docker.io/apereo/cas:7.3.6}"
REQUESTED_PORT="${BCAS_CAS_HOST_PORT:-}"
CONTAINER_NAME="bcas-cas-smoke-$$"
TMPDIR="$(mktemp -d)"

cleanup() {
podman rm -f "${CONTAINER_NAME}" >/dev/null 2>&1 || true
  rm -rf "${TMPDIR}"
}
trap cleanup EXIT

command -v podman >/dev/null
command -v curl >/dev/null
command -v keytool >/dev/null

mkdir -p "${TMPDIR}/config"

keytool -genkeypair \
  -alias cas \
  -keyalg RSA \
  -storetype PKCS12 \
  -validity 3650 \
  -keystore "${TMPDIR}/thekeystore" \
  -storepass changeit \
  -keypass changeit \
  -dname "CN=127.0.0.1, OU=Test, O=bcas, L=CI, ST=CI, C=US" \
  -ext SAN=IP:127.0.0.1 >/dev/null 2>&1

cat > "${TMPDIR}/config/cas.properties" <<'EOF'
cas.authn.accept.users=casuser::Mellon
server.ssl.key-store=file:/etc/cas/thekeystore
server.ssl.key-store-password=changeit
server.ssl.key-password=changeit
server.ssl.key-store-type=PKCS12
EOF

start_container() {
  local host_port="$1"
  podman rm -f "${CONTAINER_NAME}" >/dev/null 2>&1 || true
  podman run -d \
    --name "${CONTAINER_NAME}" \
    -p "${host_port}:8443" \
    -v "${TMPDIR}/thekeystore:/etc/cas/thekeystore:Z" \
    -v "${TMPDIR}/config:/etc/cas/config:Z" \
    "${IMAGE}" >/dev/null
}

HOST_PORT=""
if [[ -n "${REQUESTED_PORT}" ]]; then
  start_container "${REQUESTED_PORT}"
  HOST_PORT="${REQUESTED_PORT}"
else
  for candidate_port in 18443 19443 20443 21443 22443; do
    if start_container "${candidate_port}" >/dev/null 2>&1; then
      HOST_PORT="${candidate_port}"
      break
    fi
  done
fi

if [[ -z "${HOST_PORT}" ]]; then
  echo "Unable to allocate a host port for the CAS smoke container" >&2
  exit 1
fi

for _ in $(seq 1 90); do
  code="$(curl -k -s -o /dev/null -w '%{http_code}' \
    "https://127.0.0.1:${HOST_PORT}/cas/login" || true)"
  if [[ "${code}" == "200" || "${code}" == "302" ]]; then
    echo "CAS smoke check succeeded with HTTP ${code}"
    exit 0
  fi
  sleep 2
done

echo "CAS smoke check failed; recent logs:" >&2
podman logs "${CONTAINER_NAME}" --tail 120 >&2 || true
exit 1
