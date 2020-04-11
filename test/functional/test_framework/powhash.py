#!/usr/bin/env python3
# Copyright (c) 2018 The Xayà developers \n Copyright (c) 2018 The CRyptoCrowd developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Computes the PoW hash for Xaya using the xcrc-hash CLI tool."""

import codecs
import subprocess

# Path to the xcrc-hash binary.  Is set from the main routine after parsing
# the command-line options.
xcrchash = None

def forHeader (algo, hdrData):
  """Computes the PoW hash for the header given as bytes."""
  hexStr = codecs.encode (hdrData, 'hex_codec')
  args = [xcrchash, algo, hexStr]
  process = subprocess.Popen (args, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              universal_newlines=True)
  out, err = process.communicate ()
  returncode = process.poll ()
  if returncode:
    raise subprocess.CalledProcessError (returncode, xcrchash, output=err)

  res = codecs.decode (out.rstrip (), 'hex_codec')
  return res[::-1]
