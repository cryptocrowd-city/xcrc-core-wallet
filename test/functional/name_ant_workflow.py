#!/usr/bin/env python3
# Copyright (c) 2020 Daniel Kraft
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Tests the full workflow of doing atomic name trades using the
# suggested RPC/PSBT interface.

from test_framework.names import NameTestFramework, val
from test_framework.util import *


class NameAntWorkflowTest (NameTestFramework):

  def set_test_params (self):
    self.setup_clean_chain = True
    self.setup_name_test ([[]] * 2)

  def run_test (self):
    self.nodes[1].generate (10)
    self.sync_blocks ()
    self.nodes[0].generate (150)

    self.nodes[0].name_register ("x/name", val ("value"))
    self.nodes[0].generate (5)
    self.sync_blocks ()
    self.checkName (0, "x/name", val ("value"))

    # fundrawtransaction and friends does not support non-wallet provided
    # inputs.  Thus we have to first build up the funded transaction without
    # the name input and a slightly higher fee, and then combine it with the
    # name input to obtain the final raw transaction.
    addrNm = self.nodes[1].getnewaddress ()
    addrPayment = self.nodes[0].getnewaddress ()
    outs = [{addrNm: 0.01}, {addrPayment: 10}]
    part1 = self.nodes[1].createrawtransaction ([], outs)
    options = {
      "feeRate": 0.001,
      "changePosition": 2,
    }
    part1 = self.nodes[1].fundrawtransaction (part1, options)["hex"]

    nmData = self.nodes[0].name_show ("x/name")
    part2 = self.nodes[0].createrawtransaction ([nmData], [])

    fullIns = []
    fullOuts = []
    for p in [part1, part2]:
      data = self.nodes[0].decoderawtransaction (p)
      fullIns.extend (data["vin"])
      for vout in data["vout"]:
        fullOuts.append ({vout["scriptPubKey"]["addresses"][0]: vout["value"]})
    combined = self.nodes[1].createrawtransaction (fullIns, fullOuts)

    nameOp = {
      "op": "name_update",
      "name": "x/name",
      "value": val ("updated"),
    }
    combined = self.nodes[1].namerawtransaction (combined, 0, nameOp)["hex"]
    combined = self.nodes[1].converttopsbt (combined)

    # Sign and broadcast the partial tx.
    sign1 = self.nodes[0].walletprocesspsbt (combined)
    sign2 = self.nodes[1].walletprocesspsbt (combined)
    combined = self.nodes[1].combinepsbt ([sign1["psbt"], sign2["psbt"]])
    tx = self.nodes[1].finalizepsbt (combined)
    assert_equal (tx["complete"], True)
    self.nodes[0].sendrawtransaction (tx["hex"])
    self.nodes[0].generate (1)
    data = self.checkName (0, "x/name", val ("updated"))
    assert_equal (data["address"], addrNm)


if __name__ == '__main__':
  NameAntWorkflowTest ().main ()
