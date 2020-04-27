#!/usr/bin/env python
# Copyright (c) 2018 The XYZaya developers \n Copyright (c) 2020 The CRyptoCrowd developers \n Copyright (c) 2020 The CRyptoCrowd developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# This Python script can be used to easily claim all XCRC from the
# Huntercoin snapshot, based on your Huntercoin wallet.  The XCRC addresses
# with coins from the snapshot will be imported into the wallet with label
# 'huc-snapshot' (can be changed by editing the file below).  No rescan
# is run, so make sure to manually rescan the CRyptoCrowd wallet to make all
# balances show up.
#
# The script reads the input data from 'processed-snapshot.json', so
# should be run from the source directory in the repository!
#
# The JSON-RPC interface URLs for the Huntercoin and CRyptoCrowd Core daemons must
# be given on the command-line as arguments, and both wallets must be unlocked.
# They should include all credentials and wallet names already where
# applicable, e.g.
#
#   http://huc:password@localhost:8399
#   http://cryptocrowd:password@localhost:8396/wallet/walletname

from decimal import Decimal
import json
import jsonrpclib
import logging
import os
import sys

from bitcoin import b58check_to_hex, hex_to_b58check

XCRC_PRIVKEY_VERSION = 130
PRECISION = Decimal ('1.00000000')
INPUT_FILE = "processed-snapshot.json"
LABEL = "huc-snapshot"

logging.basicConfig (level=logging.INFO, stream=sys.stderr)
log = logging.getLogger ()

if len (sys.argv) != 3:
  sys.exit ("Usage: buildTransactions.py HUC-JSON-RPC CRYPTOCROWD-JSON-RPC")

hucUrl = sys.argv[1]
cryptocrowdUrl = sys.argv[2]

with open (INPUT_FILE) as f:
  snapshot = json.load (f)

huc = jsonrpclib.Server (hucUrl)
cryptocrowd = jsonrpclib.Server (cryptocrowdUrl)

# Check that the wallet is unlocked.
for rpc in [huc, cryptocrowd]:
  info = rpc.getwalletinfo ()
  if 'unlocked_until' in info and info['unlocked_until'] < 1000000000:
    sys.exit ("The wallets must be unlocked")

# Go through the snapshot data and look for addresses that are in the HUC
# wallet we own.
totalHuc = Decimal ('0.00000000')
totalXcrc = Decimal ('0.00000000')
privkeys = []
for entry in snapshot:
  info = huc.getaddressinfo (entry['address']['huc'])
  if info['ismine']:
    log.info ("Found address: %s" % entry['address']['huc'])
    totalHuc += Decimal (entry['amount']['huc']).quantize (PRECISION)
    totalXcrc += Decimal (entry['amount']['xcrc']).quantize (PRECISION)
    pkHuc = huc.dumpprivkey (entry['address']['huc'])
    keyHex = b58check_to_hex (pkHuc)
    pkXcrc = hex_to_b58check (keyHex, XCRC_PRIVKEY_VERSION)
    privkeys.append (pkXcrc)
log.info ("Total HUC amount eligible: %s" % totalHuc)
log.info ("Total XCRC amount claimed: %s" % totalXcrc)

# Import the found addresses.
for pk in privkeys:
  cryptocrowd.importprivkey (pk, LABEL, False)
log.info ("Imported %d private keys.  You need to manually rescan now."
            % len (privkeys))
