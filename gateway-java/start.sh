#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
dependency="$root/tmp/java-libs/gson-2.14.0.jar"
mkdir -p "$root/tmp/java-libs" "$root/gateway-java/target/classes"
if [[ ! -f "$dependency" ]]; then
    curl -fL 'https://repo.maven.apache.org/maven2/com/google/code/gson/gson/2.14.0/gson-2.14.0.jar' -o "$dependency"
fi
echo "2cbd119bf1961c28788310963dc80ba65f58cdeec1dd139c8bdb1240faa2c36f  $dependency" | sha256sum -c -
javac --release 17 --add-modules jdk.httpserver -encoding UTF-8 -cp "$dependency" \
    -d "$root/gateway-java/target/classes" "$root"/gateway-java/src/main/java/factory/*.java
exec java --add-modules jdk.httpserver "-Dfactory.frontend=$root/frontend" \
    "-Dfactory.runtime.mode=${RUNTIME_MODE:-cpp}" "-Dfactory.runtime.url=${RUNTIME_URL:-http://127.0.0.1:8082}" \
    "-Dfactory.runtime.timeoutMs=${RUNTIME_TIMEOUT_MS:-2000}" \
    -cp "$root/gateway-java/target/classes:$dependency" factory.Gateway "${1:-8081}"
