#!/bin/sh
# Prova end-to-end del trasporto CAN su bus virtuale.
#
# Inietta con cansend i frame del nodo sensori su vcan0 e verifica che
# efis-can-monitor li legga e li interpreti. E' la prova che le registrazioni non
# possono dare: dimostra che il socket, il filtro e la conversione funzionano sul
# nucleo vero, non solo che il codice rispetta una specifica.
#
# Va eseguito su un nodo Linux con can-utils. Sul nodo black box vcan0 esiste
# gia'; altrove si crea con:
#
#   sudo modprobe vcan
#   sudo ip link add dev vcan0 type vcan
#   sudo ip link set up vcan0
#
# Uso: tools/can/bench_vcan.sh [interfaccia] [percorso di efis-can-monitor]

set -eu

interface="${1:-vcan0}"
monitor="${2:-build/claude-verify/efis-can-monitor}"

if ! command -v cansend >/dev/null 2>&1; then
    echo "cansend non trovato: servono i can-utils" >&2
    exit 2
fi

if ! ip link show "$interface" >/dev/null 2>&1; then
    echo "interfaccia $interface assente" >&2
    exit 2
fi

if [ ! -x "$monitor" ]; then
    echo "eseguibile non trovato: $monitor" >&2
    exit 2
fi

output=$(mktemp)
trap 'rm -f "$output"' EXIT

# L'osservatore parte prima, altrimenti i primi frame passerebbero sul bus
# mentre il socket non e' ancora collegato e la prova misurerebbe il caso
# sbagliato.
"$monitor" "$interface" 4 > "$output" 2>&1 &
monitor_pid=$!
sleep 1

# Assetto: beccheggio -1,32°, rollio 4,12°, prua 90,00°, sensori dichiarati
# buoni, calibrazione piena.
cansend "$interface" 100#7CFF9C01282307FF
# Dati aria: 87,4 kt, 3200 ft, 1013,2 hPa, 47,5 Pa di differenziale.
cansend "$interface" 101#6A03800C8400DB01
# Satellitare: fix presente, 11 satelliti, HDOP 1,2.
cansend "$interface" 302#010B0C0000000000
# Comando diretto al nodo: deve essere ignorato, non interpretato.
cansend "$interface" 018#0000000000000000

# Battito ripetuto. Un solo battito farebbe scadere il nodo durante
# l'osservazione, e la prova misurerebbe l'invecchiamento invece della lettura:
# il nodo va tenuto vivo come lo sarebbe davvero.
# Il contatore e' little-endian: il byte basso viene per primo, e per valori
# sotto 256 il byte alto resta zero.
i=1
while [ "$i" -le 12 ]; do
    low=$(printf '%02X' "$i")
    cansend "$interface" "200#${low}00640000D20000"
    i=$((i + 1))
    sleep 0.25
done

wait "$monitor_pid" || true

echo "--- uscita dell'osservatore ---"
cat "$output"

fail() { echo "FALLITO: $1" >&2; exit 1; }

grep -q "mode=READ_ONLY" "$output" || fail "l'osservatore non ha dichiarato la sola lettura"
grep -q "4.12" "$output" || fail "il rollio non e' stato letto dal bus"
grep -q "87.40" "$output" || fail "la velocita' indicata non e' stata letta dal bus"
grep -q "1013.20" "$output" || fail "la pressione statica non e' stata letta dal bus"
grep -q "11 satelliti" "$output" || fail "lo stato satellitare non e' arrivato"
# Il comando 0x018 non deve essere applicato. Contare gli ignorati non basta su
# un bus condiviso, dove passa anche traffico di altri: si contano invece i frame
# applicati, che devono essere esattamente i quindici del protocollo inviati qui.
# Se il comando venisse interpretato ne risulterebbero sedici.
grep -q "applied=15" "$output" || fail "il comando 0x018 non doveva essere applicato"
grep -qE "adapter: [0-9]+ applicati, [1-9][0-9]* ignorati" "$output" ||
    fail "i frame di altri identificativi devono essere contati come ignorati"

echo "PROVA SUPERATA: i frame iniettati su $interface sono stati letti e interpretati"
