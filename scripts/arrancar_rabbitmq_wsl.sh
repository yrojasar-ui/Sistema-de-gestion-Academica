#!/usr/bin/env bash
# Arranca un nodo RabbitMQ real en loopback (127.0.0.1) para desarrollo/demo nativa en WSL
# conservando los datos en un directorio persistente (por defecto data/rabbitmq-wsl).
set -euo pipefail
cd "$(dirname "$0")/.."

BASE_DIR="${1:-$PWD/data/rabbitmq-wsl}"
mkdir -p "$BASE_DIR"
BASE_DIR="$(cd "$BASE_DIR" && pwd)"
AMQP_PORT="${AMQP_PORT:-5672}"
EPMD_PORT="${EPMD_PORT:-24369}"
DIST_PORT="${DIST_PORT:-35672}"

mkdir -p "$BASE_DIR"/{home,mnesia,log,etc,plugins,schema,generated}
chmod 700 "$BASE_DIR/home"

RT="$PWD/build/deps/rabbitmq-runtime"
if [[ -d "$RT/usr/lib/rabbitmq/lib" ]]; then
  SBIN=$(echo "$RT"/usr/lib/rabbitmq/lib/rabbitmq_server-*/sbin)
  export PATH="$RT/usr/lib/erlang/bin:$SBIN:$RT/usr/bin:$PATH"
  RMQ_BIN="$SBIN/rabbitmq-server"
elif [[ -x /usr/lib/rabbitmq/bin/rabbitmq-server ]]; then
  RMQ_BIN="/usr/lib/rabbitmq/bin/rabbitmq-server"
elif command -v rabbitmq-server >/dev/null 2>&1; then
  RMQ_BIN="$(command -v rabbitmq-server)"
elif [[ -x /usr/sbin/rabbitmq-server ]]; then
  RMQ_BIN="/usr/sbin/rabbitmq-server"
else
  echo "Error: instale rabbitmq-server o prepare build/deps/rabbitmq-runtime." >&2
  exit 1
fi

export HOME="$BASE_DIR/home"
export ERL_EPMD_ADDRESS="127.0.0.1"
export ERL_EPMD_PORT="$EPMD_PORT"
export RABBITMQ_NODENAME="biblioteca_wsl@localhost"
export RABBITMQ_NODE_IP_ADDRESS="127.0.0.1"
export RABBITMQ_NODE_PORT="$AMQP_PORT"
export RABBITMQ_DIST_PORT="$DIST_PORT"
export RABBITMQ_MNESIA_BASE="$BASE_DIR/mnesia"
export RABBITMQ_LOG_BASE="$BASE_DIR/log"
export RABBITMQ_CONFIG_FILE="$BASE_DIR/etc/rabbitmq.conf"
export RABBITMQ_CONF_ENV_FILE="$BASE_DIR/etc/rabbitmq-env.conf"
export RABBITMQ_ENABLED_PLUGINS_FILE="$BASE_DIR/etc/enabled_plugins"
export RABBITMQ_PLUGINS_EXPAND_DIR="$BASE_DIR/plugins"
export RABBITMQ_PID_FILE="$BASE_DIR/rabbitmq.pid"
export RABBITMQ_SCHEMA_DIR="$BASE_DIR/schema"
export RABBITMQ_GENERATED_CONFIG_DIR="$BASE_DIR/generated"
export RABBITMQ_FEATURE_FLAGS_FILE="$BASE_DIR/feature_flags"

: > "$RABBITMQ_CONF_ENV_FILE"
cat > "$RABBITMQ_CONFIG_FILE" <<EOF
listeners.tcp.1 = 127.0.0.1:${AMQP_PORT}
loopback_users = none
EOF
echo "[]." > "$RABBITMQ_ENABLED_PLUGINS_FILE"

echo "Iniciando RabbitMQ en 127.0.0.1:${AMQP_PORT} (datos persistentes en ${BASE_DIR})"
exec "$RMQ_BIN"
