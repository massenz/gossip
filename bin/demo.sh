#!/usr/bin/bash
#
# Starts a number of Gossip Detectors and shows how they
# discover each other.

WORKDIR=${HOME}/Development/gossip

cmd=${WORKDIR}/build/bin/gossip_detector_example
host=gondor
port=7777
http_port=8081

echo -e "----------\nStarting first group of Gossip Servers\n\n"

$cmd --seeds=$host:$port --port=$port --http --http-port=$http_port --cors="*" &
echo "Seed server at 127.0.0.1:${http_port}"

sleep 10

for num in $(seq 10)
do
    prev=${port}
    port=$((port + 1))
    http_port=$((http_port + 1))
    $cmd --seeds=$host:$prev --port=$port --http --http-port=$http_port --cors="*" &
    echo "Started server #${port} (reachable at 127.0.0.1:${http_port})"
    sleep 1
done

sleep 30

http_port=$((http_port + 10))
port=$((port + 10))
prev=${port}

echo -e "\n----------\nStarting an 'island' of Gossip servers on HTTP port ${http_port}\n\n"

for num in $(seq 3)
do

    $cmd --seeds=$host:$prev --port=$port --http --http-port=$http_port --cors="*" &
    echo "Started server #${port} (reachable at 127.0.0.1:${http_port}"
    sleep 1

    prev=${port}
    port=$((port + 1))
    http_port=$((http_port + 1))
done
