#include "ChallengeManager.h"
#include "AuctionHouseMgr.h"
#include "Chat.h"
#include "Creature.h"
#include "Config.h"
#include "Duration.h"
#include "GameTime.h"
#include "Group.h"
#include "Mail.h"
#include "MailScript.h"
#include "MiscScript.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"

void AddChallengeSystemCommands();

namespace
{
void SendPlayerError(Player* player, char const* message)
{
    if (!player || !player->GetSession())
        return;

    ChatHandler(player->GetSession()).SendSysMessage(message);
}

void SendPlayerNotification(Player* player, char const* message)
{
    if (!player || !player->GetSession())
        return;

    ChatHandler(player->GetSession()).SendNotification(message);
}

}

class ChallengeSystemHooks : public PlayerScript
{
public:
    ChallengeSystemHooks() : PlayerScript("ip_challengesystem_hooks") {}

    void OnPlayerLogin(Player* player) override
    {
        if (ChallengeManager::Instance().IsPermadead(player))
        {
            SendPlayerNotification(player, "This character is permanently dead. You may keep it as a memorial or delete it.");
            if (player && player->GetSession())
            {
                time_t now = GameTime::GetGameTime().count();
                player->GetSession()->SetLogoutStartTime(now > 20 ? now - 20 : 0);
            }
            return;
        }

        ChallengeManager::Instance().HandlePlayerLogin(player);
    }

    bool OnPlayerCanGroupInvite(Player* inviter, std::string& membername) override
    {
        Player* target = ObjectAccessor::FindPlayerByName(membername, false);
        if (!ChallengeManager::Instance().HandleGroupInvite(inviter, target))
        {
            SendPlayerError(inviter, "Grouping is disabled by active Challenge restrictions.");
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
            SendPlayerError(player, "Grouping is disabled by active Challenge restrictions.");
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

    void OnPlayerLogout(Player* player) override
    {
        ChallengeManager::Instance().HandlePlayerLogout(player);
    }

    bool OnPlayerCanRepopAtGraveyard(Player* player) override
    {
        if (ChallengeManager::Instance().IsPermadeathPending(player))
        {
            return false;
        }

        return true;
    }

    void OnPlayerReleasedGhost(Player* player) override
    {
        if (!player)
            return;

        if (!ChallengeManager::Instance().IsPermadeathPending(player))
            return;

        uint32 mapId = sConfigMgr->GetOption<uint32>("ChallengeSystem.Permadeath.GhostMap", 0);
        float x = sConfigMgr->GetOption<float>("ChallengeSystem.Permadeath.GhostX", -11075.463f);
        float y = sConfigMgr->GetOption<float>("ChallengeSystem.Permadeath.GhostY", -1795.9891f);
        float z = sConfigMgr->GetOption<float>("ChallengeSystem.Permadeath.GhostZ", 52.717907f);
        float o = sConfigMgr->GetOption<float>("ChallengeSystem.Permadeath.GhostO", 0.043097086f);

        player->TeleportTo(mapId, x, y, z, o);
    }

    bool OnPlayerCanResurrect(Player* player) override
    {
        if (ChallengeManager::Instance().IsPermadeathPending(player))
        {
            SendPlayerNotification(player, "Resurrection is disabled while permadeath is pending.");
            return false;
        }

        return true;
    }

    void OnPlayerPVPKill(Player* /*killer*/, Player* killed) override
    {
        ChallengeManager::Instance().RecordPvPDeath(killed);
    }

    void OnPlayerKilledByCreature(Creature* /*killer*/, Player* killed) override
    {
        ChallengeManager::Instance().RecordPvEDeath(killed);
    }

    bool OnPlayerBeforeTeleport(Player* player, uint32 /*mapid*/, float /*x*/, float /*y*/, float /*z*/,
                                float /*orientation*/, uint32 options, Unit* target) override
    {
        if (!ChallengeManager::Instance().HandleSummonAccept(player, target, options))
        {
            SendPlayerError(player, "Summons are disabled by active Challenge restrictions.");
            return false;
        }

        return true;
    }

    void OnPlayerJustDied(Player* player) override
    {
        ChallengeManager::Instance().HandleDeath(player);
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
    AddChallengeSystemCommands();
}
