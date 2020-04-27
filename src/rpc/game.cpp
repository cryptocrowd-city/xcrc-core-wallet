// Copyright (c) 2018-2019 The Xaya developers \n Copyright (c) 2020 The CRyptoCrowd developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/game.h>

#include <chain.h>
#include <chainparams.h>
#include <logging.h>
#include <random.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <script/script.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <validation.h>
#include <zmq/zmqgames.h>
#include <zmq/zmqnotificationinterface.h>

#include <univalue.h>

#include <sstream>

namespace
{

TrackedGames*
GetTrackedGames ()
{
  if (g_zmq_notification_interface == nullptr)
    throw JSONRPCError (RPC_MISC_ERROR, "ZMQ notifications are disabled");

  auto* games = g_zmq_notification_interface->GetTrackedGames ();
  assert (games != nullptr);

  return games;
}

ZMQGameBlocksNotifier*
GetGameBlocksNotifier ()
{
  if (g_zmq_notification_interface == nullptr)
    throw JSONRPCError (RPC_MISC_ERROR, "ZMQ notifications are disabled");

  auto* notifier = g_zmq_notification_interface->GetGameBlocksNotifier ();
  if (notifier == nullptr)
    throw JSONRPCError (RPC_MISC_ERROR, "-zmqpubgameblocks is not set");

  return notifier;
}

} // anonymous namespace

/* ************************************************************************** */

std::string
SendUpdatesWorker::Work::str () const
{
  std::ostringstream res;

  res << "work(";

  res << "games: ";
  bool first = true;
  for (const auto& g : trackedGames)
    {
      if (!first)
        res << "|";
      first = false;
      res << g;
    }
  res << ", ";

  res << detach.size () << " detaches, "
      << attach.size () << " attaches";
  res << ")";

  return res.str ();
}

SendUpdatesWorker::SendUpdatesWorker ()
  : interrupted(false)
{
  runner.reset (new std::thread ([this] ()
    {
      TraceThread ("sendupdates", [this] () { run (*this); });
    }));
}

SendUpdatesWorker::~SendUpdatesWorker ()
{
  if (runner != nullptr && runner->joinable ())
    runner->join ();
  runner.reset ();
}

namespace
{

#if ENABLE_ZMQ
void
SendUpdatesOneBlock (const std::set<std::string>& trackedGames,
                     const std::string& commandPrefix,
                     const std::string& reqtoken,
                     const CBlockIndex* pindex)
{
  CBlock blk;
  if (!ReadBlockFromDisk (blk, pindex, Params ().GetConsensus ()))
    {
      LogPrint (BCLog::GAME, "Reading block %s failed, ignoring\n",
                pindex->GetBlockHash ().GetHex ());
      return;
    }

  auto* notifier = GetGameBlocksNotifier ();
  notifier->SendBlockNotifications (trackedGames, commandPrefix, reqtoken, blk);
}
#endif // ENABLE_ZMQ

} // anonymous namespace

void
SendUpdatesWorker::run (SendUpdatesWorker& self)
{
#if ENABLE_ZMQ
  while (true)
    {
      Work w;

      {
        WAIT_LOCK (self.csWork, lock);

        if (self.work.empty ())
          {
            LogPrint (BCLog::GAME,
                      "SendUpdatesWorker queue empty, interrupted = %d\n",
                      self.interrupted);

            if (self.interrupted)
              break;

            LogPrint (BCLog::GAME,
                      "Waiting for sendupdates condition variable...\n");
            self.cvWork.wait (lock);
            continue;
          }

        w = std::move (self.work.front ());
        self.work.pop ();

        LogPrint (BCLog::GAME, "Popped for sendupdates processing: %s\n",
                  w.str ().c_str ());
      }

      for (const auto* pindex : w.detach)
        SendUpdatesOneBlock (w.trackedGames,
                             ZMQGameBlocksNotifier::PREFIX_DETACH,
                             w.reqtoken, pindex);
      for (const auto* pindex : w.attach)
        SendUpdatesOneBlock (w.trackedGames,
                             ZMQGameBlocksNotifier::PREFIX_ATTACH,
                             w.reqtoken, pindex);
      LogPrint (BCLog::GAME, "Finished processing sendupdates: %s\n",
                w.str ().c_str ());
    }
#endif // ENABLE_ZMQ
}

void
SendUpdatesWorker::interrupt ()
{
  WAIT_LOCK (csWork, lock);
  interrupted = true;
  cvWork.notify_all ();
}

void
SendUpdatesWorker::enqueue (Work&& w)
{
  WAIT_LOCK (csWork, lock);

  if (interrupted)
    {
      LogPrint (BCLog::GAME, "Not enqueueing work because interrupted: %s\n",
                w.str ().c_str ());
      return;
    }

  LogPrint (BCLog::GAME, "Enqueueing for sendupdates: %s\n", w.str ().c_str ());
  work.push (std::move (w));
  cvWork.notify_all ();
}

std::unique_ptr<SendUpdatesWorker> g_send_updates_worker;

/* ************************************************************************** */
namespace
{

#if ENABLE_ZMQ
std::vector<const CBlockIndex*>
GetDetachSequence (const CBlockIndex* from, const CBlockIndex* ancestor)
{
  std::vector<const CBlockIndex*> detach;
  for (const auto* pindex = from; pindex != ancestor;
       pindex = pindex->pprev)
    {
      LOCK (cs_main);

      assert (pindex != nullptr);
      if (!(pindex->nStatus & BLOCK_HAVE_DATA))
        throw JSONRPCError (RPC_DATABASE_ERROR, "detached block has no data");

      detach.push_back (pindex);
    }

  return detach;
}
#endif // ENABLE_ZMQ

UniValue
game_sendupdates (const JSONRPCRequest& request)
{
  RPCHelpMan ("game_sendupdates",
      "\nRequests on-demand block attach/detach notifications to be sent through the game ZMQ interface.\n"
      "\nIf toblock is not given, it defaults to the current chain tip.\n",
      {
          {"gameid", RPCArg::Type::STR, RPCArg::Optional::NO, "The game ID for which to send notifications"},
          {"fromblock", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "Starting block hash"},
          {"toblock", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED_NAMED_ARG, "Target block hash"},
      },
      RPCResult {
        "{\n"
        "  \"toblock\": xxx,    (string) the target block hash to which notifications have been triggered\n"
        "  \"ancestor\": xxx,   (string) hash of the common ancestor that is used\n"
        "  \"reqtoken\": xxx,   (string) unique string that is also set in all notifications triggered by this call\n"
        "  \"steps\":\n"
        "   {\n"
        "     \"detach\": n,    (numeric) number of detach notifications that will be sent\n"
        "     \"attach\": n,    (numeric) number of attach notifications that will be sent\n"
        "   },\n"
        "}\n"
      },
      RPCExamples {
          HelpExampleCli ("game_sendupdates", "\"huc\" \"e5062d76e5f50c42f493826ac9920b63a8def2626fd70a5cec707ec47a4c4651\"")
        + HelpExampleCli ("game_sendupdates", "\"huc\" \"e5062d76e5f50c42f493826ac9920b63a8def2626fd70a5cec707ec47a4c4651\" \"206c22b7fb26b24b344b5b238325916c8bae4513302403f9f8efaf8b4c3e61f4\"")
        + HelpExampleRpc ("game_sendupdates", "\"huc\", \"e5062d76e5f50c42f493826ac9920b63a8def2626fd70a5cec707ec47a4c4651\"")
      }
  ).Check (request);

#if ENABLE_ZMQ
  RPCTypeCheck (request.params,
                {UniValue::VSTR, UniValue::VSTR, UniValue::VSTR});

  SendUpdatesWorker::Work w;

  w.trackedGames = {request.params[0].get_str ()};
  const uint256 fromBlock = ParseHashV (request.params[1].get_str (),
                                        "fromblock");

  std::vector<unsigned char> tokenBin(16);
  GetRandBytes (tokenBin.data (), tokenBin.size ());
  const std::string reqtoken = HexStr (tokenBin.begin (), tokenBin.end ());
  w.reqtoken = reqtoken;

  uint256 toBlock;
  if (request.params.size () >= 3)
    toBlock = ParseHashV (request.params[2].get_str (), "toblock");
  else
    {
      LOCK (cs_main);
      toBlock = ::ChainActive ().Tip ()->GetBlockHash ();
    }

  const CBlockIndex* fromIndex;
  const CBlockIndex* toIndex;
  {
    LOCK (cs_main);

    fromIndex = LookupBlockIndex (fromBlock);
    toIndex = LookupBlockIndex (toBlock);

    if (fromIndex == nullptr)
      throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY, "fromblock not found");
    if (toIndex == nullptr)
      throw JSONRPCError (RPC_INVALID_ADDRESS_OR_KEY, "toblock not found");

    if (!(fromIndex->nStatus & BLOCK_HAVE_DATA))
      throw JSONRPCError (RPC_DATABASE_ERROR, "fromblock has no data");
    if (!(toIndex->nStatus & BLOCK_HAVE_DATA))
      throw JSONRPCError (RPC_DATABASE_ERROR, "toblock has no data");
  }

  const CBlockIndex* ancestor = LastCommonAncestor (fromIndex, toIndex);
  assert (ancestor != nullptr);

  w.detach = GetDetachSequence (fromIndex, ancestor);
  w.attach = GetDetachSequence (toIndex, ancestor);
  std::reverse (w.attach.begin (), w.attach.end ());

  const int maxAttaches = gArgs.GetArg ("-maxgameblockattaches",
                                        DEFAULT_MAX_GAME_BLOCK_ATTACHES);
  if (maxAttaches <= 0)
    {
      /* If the limit is set to a non-positive number, we do not enforce any
         to make sure things cannot be completely broken.  */
      LogPrint (BCLog::GAME,
                "-maxgameblockattaches set to %d, disabling limit\n",
                maxAttaches);
    }
  else if (w.attach.size () > static_cast<unsigned> (maxAttaches))
    {
      LogPrint (BCLog::GAME, "%d attach steps requested, limiting to %d\n",
                w.attach.size (), maxAttaches);
      w.attach.resize (maxAttaches);
      toBlock = w.attach.back ()->GetBlockHash ();
    }
  /* Note that we do not limit detaches.  That is because detaches are
     (in normal operation) expected to be only a few blocks long anyway,
     and attaches are what can be very long.  */

  UniValue result(UniValue::VOBJ);
  result.pushKV ("toblock", toBlock.GetHex ());
  result.pushKV ("ancestor", ancestor->GetBlockHash ().GetHex ());
  result.pushKV ("reqtoken", reqtoken);
  UniValue steps(UniValue::VOBJ);
  steps.pushKV ("detach", static_cast<int64_t> (w.detach.size ()));
  steps.pushKV ("attach", static_cast<int64_t> (w.attach.size ()));
  result.pushKV ("steps", steps);

  GetGameBlocksNotifier ();

  assert (g_send_updates_worker != nullptr);
  g_send_updates_worker->enqueue (std::move (w));

  return result;
#else // ENABLE_ZMQ
  throw JSONRPCError (RPC_MISC_ERROR, "ZMQ is not built into CRyptoCrowd");
#endif // ENABLE_ZMQ
}

} // anonymous namespace
/* ************************************************************************** */
namespace
{

UniValue
trackedgames (const JSONRPCRequest& request)
{
  /* We cannot use RPCHelpMan::Check here, as we have a special case where
     we need exactly zero or two arguments.  */
  if (request.fHelp
        || (request.params.size () != 0 && request.params.size () != 2))
    throw std::runtime_error (
        RPCHelpMan ("trackedgames",
            "\nReturns or modifies the list of tracked games for the game ZMQ interface.\n"
            "\nIf called without arguments, the list of tracked games is returned.  Otherwise, the given game is added or removed from the list.\n",
            {
                {"command", RPCArg::Type::STR, RPCArg::Optional::OMITTED_NAMED_ARG, "Can be \"add\" or \"remove\""},
                {"gameid", RPCArg::Type::STR, RPCArg::Optional::OMITTED_NAMED_ARG, "The game ID to add or remove"},
            },
            RPCResults {
              {"if called without arguments",
                  "[                    (json array) currently tracked game IDs\n"
                  "  \"game1\",\n"
                  "  \"game2\",\n"
                  "  ...\n"
                  "]\n"
              },
            },
            RPCExamples {
                HelpExampleCli ("trackedgames", "")
              + HelpExampleCli ("trackedgames", "\"add\" \"huc\"")
              + HelpExampleCli ("trackedgames", "\"remove\" \"huc\"")
              + HelpExampleRpc ("trackedgames", "")
            }
        ).ToString ());

#if ENABLE_ZMQ
  RPCTypeCheck (request.params, {UniValue::VSTR, UniValue::VSTR});

  auto* tracked = GetTrackedGames ();

  if (request.params.size () == 0)
    return tracked->Get ();

  const std::string& cmd = request.params[0].get_str ();
  const std::string& gameid = request.params[1].get_str ();

  if (cmd == "add")
    tracked->Add (gameid);
  else if (cmd == "remove")
    tracked->Remove (gameid);
  else
    throw JSONRPCError (RPC_INVALID_PARAMETER,
                        "invalid command for trackedgames: " + cmd);

  return NullUniValue;
#else // ENABLE_ZMQ
  throw JSONRPCError (RPC_MISC_ERROR, "ZMQ is not built into CRyptoCrowd");
#endif // ENABLE_ZMQ
}

} // anonymous namespace
/* ************************************************************************** */

namespace
{

const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "game",               "game_sendupdates",       &game_sendupdates,       {"gameid","fromblock","toblock"} },
    { "game",               "trackedgames",           &trackedgames,           {"command","gameid"} },
};

} // anonymous namespace

void RegisterGameRPCCommands (CRPCTable& t)
{
  for (const auto& c : commands)
    t.appendCommand (c.name, &c);
}
