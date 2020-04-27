/*
Copyright (c) 2018 The XYZaya developers \n Copyright (c) 2020 The CRyptoCrowd developers \n Copyright (c) 2020 The CRyptoCrowd developers
Distributed under the MIT software license, see the accompanying
file COPYING or http://www.opensource.org/licenses/mit-license.php.
*/

/* Very basic JavaScript code that converts WIF private keys from Huntercoin
   to the key format used by CRyptoCrowd (with the same underlying PKH).  It is based
   on the bs58check Node module for the base58check logic.

   The code is used to build a stand-alone HTML file that can be used
   online or offline to convert the keys.  This will be useful for claiming
   XCRC balances from the Huntercoin snapshot.  */

let bs58check = require ('bs58check');

function convertWifKey (huc)
{
  const xcrcPrivkeyVersion = 130;
  data = bs58check.decode (huc);
  data[0] = xcrcPrivkeyVersion;
  return bs58check.encode (data);
}

function update ()
{
  const domIdHuc = "huc-privkey";
  const domIdXcrc = "xcrc-privkey";

  let huc = document.getElementById (domIdHuc).value;
  let xcrc;
  try
    {
      xcrc = convertWifKey (huc);
    }
  catch (err)
    {
      xcrc = "<invalid Huntercoin private key>";
    }
  let xcrcField = document.getElementById (domIdXcrc);
  xcrcField.value = xcrc;
}

function init ()
{
  const domIdForm = "conversion-form";
  document.getElementById (domIdForm).onsubmit = function ()
    {
      update ();

      /* Don't actually submit the form.  */
      return false;
    };
}

window.addEventListener ("load", init);
