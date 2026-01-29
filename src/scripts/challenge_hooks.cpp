#include "ChallengeManager.h"
#include "AuctionHouseMgr.h"
#include "Chat.h"
#include "Group.h"
#include "Mail.h"
#include "MailScript.h"
#include "MiscScript.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"

namespace
{
void SendPlayerError(Player* player, char const* message)
{
    if (!player || !player->GetSession())
        return;

    ChatHandler(player->GetSession()).SendSysMessage(message);
}
}

class ChallengeSystemHooks : public PlayerScript
{
public:
    ChallengeSystemHooks() : PlayerScript("ip_challengesystem_hooks") {}

    bool OnPlayerCanGroupInvite(Player* inviter, std::string& membername) override
    {
        Player* target = ObjectAccessor::FindPlayerByName(membername, false);
        if (!ChallengeManager::Instance().HandleGroupInvite(inviter, target))
        {
            SendPlayerError(inviter, "Hardcore characters may only group with other Hardcore tier members.");
            return false;
        }

        return true;
    }

    bool OnPlayerCanGroupAccept(Player* player, Group* group) override
    {
        if (group && group->isLFGGroup())
            return true;

        if (!ChallengeManager::Instance().HandleGroupAccept(player, group))
        {
            SendPlayerError(player, "Hardcore characters may only group with other Hardcore tier members.");
            return false;
        }

        return true;
    }

    bool OnPlayerCanInitTrade(Player* player, Player* target) override
    {
        if (!ChallengeManager::Instance().HandleTradeAttempt(player, target))
        {
            SendPlayerError(player, "Trading is disabled by active Challenge restrictions.");
            return false;
        }

        return true;
    }

    bool OnPlayerCanSendMail(Player* player, ObjectGuid /*receiverGuid*/, ObjectGuid /*mailbox*/,
                             std::string& /*subject*/, std::string& /*body*/, uint32 /*money*/, uint32 /*COD*/,
                             Item* /*item*/) override
    {
        if (!ChallengeManager::Instance().HandleMailSend(player))
        {
            SendPlayerError(player, "Mail is disabled by active Challenge restrictions.");
            return false;
        }

        return true;
    }

    bool OnPlayerCanPlaceAuctionBid(Player* player, AuctionEntry* /*auction*/) override
    {
        if (!ChallengeManager::Instance().HandleAuctionAction(player))
        {
            SendPlayerError(player, "Auction House access is disabled by active Challenge restrictions.");
            return false;
        }

        return true;
    }
};

class ChallengeSystemMiscHooks : public MiscScript
{
public:
    ChallengeSystemMiscHooks() : MiscScript("ip_challengesystem_misc") {}

    bool CanSendAuctionHello(WorldSession const* session, ObjectGuid /*guid*/, Creature* /*creature*/) override
    {
        if (!session)
            return true;

        Player* player = session->GetPlayer();
        if (!ChallengeManager::Instance().HandleAuctionAction(player))
        {
            SendPlayerError(player, "Auction House access is disabled by active Challenge restrictions.");
            return false;
        }

        return true;
    }
};

class ChallengeSystemMailHooks : public MailScript
{
public:
    ChallengeSystemMailHooks() : MailScript("ip_challengesystem_mail") {}

    void OnBeforeMailDraftSendMailTo(MailDraft* /*mailDraft*/, MailReceiver const& receiver, MailSender const& /*sender*/,
                                     MailCheckMask& /*checked*/, uint32& /*deliver_delay*/, uint32& /*custom_expiration*/,
                                     bool& deleteMailItemsFromDB, bool& sendMail) override
    {
        Player* receiverPlayer = receiver.GetPlayer();
        if (!receiverPlayer)
            return;

        if (!ChallengeManager::Instance().HandleMailSend(receiverPlayer))
        {
            SendPlayerError(receiverPlayer, "Mail is disabled by active Challenge restrictions.");
            sendMail = false;
            deleteMailItemsFromDB = false;
        }
    }
};

void AddChallengeSystemScripts()
{
    new ChallengeSystemHooks();
    new ChallengeSystemMiscHooks();
    new ChallengeSystemMailHooks();
}
