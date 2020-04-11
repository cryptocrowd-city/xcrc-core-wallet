#!/bin/sh -e

case $1 in
  xcrc-tx)
    exec "$@"
    ;;

  xcrc-cli)
    shift
    exec xcrc-cli \
      --datadir="/var/lib/xcrc" \
      --rpcconnect="${HOST}" \
      --rpcpassword="${RPC_PASSWORD}" \
      --rpcport="${RPC_PORT}" \
      "$@"
    ;;

  xcrcd)
    bin=$1
    shift
    ;;

  *)
    bin=xcrcd
    ;;
esac

if [ -z "${RPC_PASSWORD}" ]
then
  echo "RPC_PASSWORD must be set"
  exit 1
fi

exec $bin \
  --datadir="/var/lib/xcrc" \
  --rpcpassword="${RPC_PASSWORD}" \
  --rpcbind="${HOST}" \
  --rpcallowip="${RPC_ALLOW_IP}" \
  --rpcport="${RPC_PORT}" \
  --port="${P2P_PORT}" \
  --zmqpubgameblocks="tcp://${HOST}:${ZMQ_PORT}" \
  --zmqpubgamepending="tcp://${HOST}:${ZMQ_PORT}" \
  "$@"
