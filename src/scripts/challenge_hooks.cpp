#include "ChallengeManager.h"
#include "AuctionHouseMgr.h"
#include "AccountMgr.h"
#include "Chat.h"
#include "CharacterCache.h"
#include "Creature.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Duration.h"
#include "GameTime.h"
#include "Group.h"
#include "GuildScript.h"
#include "Mail.h"
#include "MailScript.h"
#include "MiscScript.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ServerScript.h"

#include <cctype>
#include <sstream>
#include <vector>

void AddChallengeSystemCommands();

namespace
{
std::string GetConfigMessage(char const* key, char const* fallback)
{
    return sConfigMgr->GetOption<std::string>(key, fallback);
}

void SendPlayerError(Player* player, std::string const& message)
{
    if (!player || !player->GetSession())
        return;

    ChatHandler(player->GetSession()).SendSysMessage(message.c_str());
}

void SendPlayerNotification(Player* player, std::string const& message)
{
    if (!player || !player->GetSession())
        return;

    ChatHandler(player->GetSession()).SendNotification(message.c_str());
}

std::vector<std::string> SplitWhitespace(std::string const& input)
{
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}

std::vector<std::string> SplitComma(std::string const& input)
{
    std::vector<std::string> tokens;
    std::string current;
    for (char c : input)
    {
        if (c == ',')
        {
            if (!current.empty())
                tokens.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    if (!current.empty())
        tokens.push_back(current);
    return tokens;
}

std::string ToLower(std::string value)
{
    for (char& c : value)
        c = static_cast<char>(std::tolower(c));
    return value;
}

bool ExtractChatMessage(WorldPacket& packet, uint32 type, uint32 lang, std::string& outMsg)
{
    try
    {
        switch (type)
        {
            case CHAT_MSG_SAY:
            case CHAT_MSG_YELL:
            case CHAT_MSG_PARTY:
            case CHAT_MSG_RAID:
            case CHAT_MSG_GUILD:
            case CHAT_MSG_OFFICER:
            case CHAT_MSG_EMOTE:
            case CHAT_MSG_TEXT_EMOTE:
            case CHAT_MSG_RAID_LEADER:
            case CHAT_MSG_RAID_WARNING:
            case CHAT_MSG_BATTLEGROUND:
            case CHAT_MSG_BATTLEGROUND_LEADER:
            case CHAT_MSG_PARTY_LEADER:
            case CHAT_MSG_WHISPER:
            {
                if (type == CHAT_MSG_WHISPER)
                {
                    std::string to;
                    packet >> to;
                }
                outMsg = packet.ReadCString(lang != LANG_ADDON);
                return true;
            }
            case CHAT_MSG_CHANNEL:
            {
                std::string channel;
                packet >> channel;
                outMsg = packet.ReadCString(lang != LANG_ADDON);
                return true;
            }
            case CHAT_MSG_AFK:
            case CHAT_MSG_DND:
            {
                outMsg = packet.ReadCString(lang != LANG_ADDON);
                return true;
            }
            default:
                return false;
        }
    }
    catch (...)
    {
        return false;
    }
}

bool IsHardcoreBotAllowed(Player* master, std::string const& name)
{
    if (!master)
        return false;

    ObjectGuid guid = sCharacterCache->GetCharacterGuidByName(name);
    if (!guid)
        return false;

    return ChallengeManager::Instance().IsHardcoreGuid(guid.GetCounter());
}

bool AreAccountBotsHardcore(std::string const& accountOrCharacter)
{
    uint32 accountId = AccountMgr::GetId(accountOrCharacter);
    if (!accountId)
    {
        ObjectGuid guid = sCharacterCache->GetCharacterGuidByName(accountOrCharacter);
        if (!guid)
            return false;
        accountId = sCharacterCache->GetCharacterAccountIdByGuid(guid);
    }

    if (!accountId)
        return false;

    QueryResult result = CharacterDatabase.Query(
        "SELECT guid FROM characters WHERE account = {}", accountId);
    if (!result)
        return false;

    do
    {
        Field* fields = result->Fetch();
        uint32 guid = fields[0].Get<uint32>();
        if (!ChallengeManager::Instance().IsHardcoreGuid(guid))
            return false;
    } while (result->NextRow());

    return true;
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
            SendPlayerNotification(player,
                GetConfigMessage("ChallengeSystem.Message.Permadeath.Lockout",
                    "This character is permanently dead. You may keep it as a memorial or delete it."));
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
            SendPlayerError(inviter,
                GetConfigMessage("ChallengeSystem.Message.GroupBlocked",
                    "Grouping is disabled by active Challenge restrictions."));
            return false;
        }

        return true;
    }

    bool OnPlayerCanGroupAccept(Player* player, Group* group) override
    {
        if (group && group->isLFGGroup())
        {
            if (!ChallengeManager::Instance().HandleGroupAccept(player, group))
            {
                SendPlayerError(player,
                    GetConfigMessage("ChallengeSystem.Message.GroupBlocked",
                        "Grouping is disabled by active Challenge restrictions."));
                return false;
            }
            return true;
        }

        if (!ChallengeManager::Instance().HandleGroupAccept(player, group))
        {
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.GroupBlocked",
                    "Grouping is disabled by active Challenge restrictions."));
            return false;
        }

        return true;
    }

    bool OnPlayerCanInitTrade(Player* player, Player* target) override
    {
        if (!ChallengeManager::Instance().HandleTradeAttempt(player, target))
        {
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.TradeBlocked",
                    "Trading is disabled by active Challenge restrictions."));
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
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.MailBlocked",
                    "Mail is disabled by active Challenge restrictions."));
            return false;
        }

        return true;
    }

    bool OnPlayerCanPlaceAuctionBid(Player* player, AuctionEntry* /*auction*/) override
    {
        if (!ChallengeManager::Instance().HandleAuctionAction(player))
        {
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.AuctionBlocked",
                    "Auction House access is disabled by active Challenge restrictions."));
            return false;
        }

        return true;
    }

    void OnPlayerLogout(Player* player) override
    {
        ChallengeManager::Instance().HandlePlayerLogout(player);
    }

    bool OnPlayerCanEquipItem(Player* player, uint8 slot, uint16& /*dest*/, Item* pItem, bool /*swap*/, bool /*not_loading*/) override
    {
        if (!ChallengeManager::Instance().HandleEquipItem(player, pItem, slot, false))
        {
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.EquipBlocked",
                    "Equipping this item is disabled by active Challenge restrictions."));
            return false;
        }

        return true;
    }

    void OnPlayerMoneyChanged(Player* player, int32& amount) override
    {
        ChallengeManager::Instance().HandleMoneyChange(player, amount);
    }

    void OnPlayerCalculateTalentsPoints(Player const* player, uint32& points) override
    {
        ChallengeManager::Instance().HandleTalentPoints(const_cast<Player*>(player), points);
    }

    void OnPlayerBeforeInitTalentForLevel(Player* player, uint8& /*level*/, uint32& talentPointsForLevel) override
    {
        ChallengeManager::Instance().HandleTalentPoints(player, talentPointsForLevel);
    }

    void OnPlayerFreeTalentPointsChanged(Player* player, uint32 /*points*/) override
    {
        ChallengeManager::Instance().EnforceNoTalents(player);
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* /*victim*/, uint8 xpSource) override
    {
        ChallengeManager::Instance().HandleGiveXP(player, amount, xpSource);
    }

    void OnPlayerQuestComputeXP(Player* player, Quest const* /*quest*/, uint32& xpValue) override
    {
        ChallengeManager::Instance().HandleQuestXP(player, xpValue);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        ChallengeManager::Instance().HandlePlayerUpdate(player, diff);
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
            SendPlayerNotification(player,
                GetConfigMessage("ChallengeSystem.Message.ResurrectBlocked",
                    "Resurrection is disabled while permadeath is pending."));
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
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.SummonBlocked",
                    "Summons are disabled by active Challenge restrictions."));
            return false;
        }

        return true;
    }

    void OnPlayerJustDied(Player* player) override
    {
        ChallengeManager::Instance().HandleDeath(player);
    }
};

class ChallengeSystemGuildHooks : public GuildScript
{
public:
    ChallengeSystemGuildHooks() : GuildScript("ip_challengesystem_guild") {}

    bool CanGuildSendBankList(Guild const* /*guild*/, WorldSession* session, uint8 /*tabId*/, bool /*sendAllSlots*/) override
    {
        if (!session)
            return true;

        Player* player = session->GetPlayer();
        if (!ChallengeManager::Instance().HandleGuildBankAccess(player))
        {
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.GuildBankBlocked",
                    "Guild Bank access is disabled by active Challenge restrictions."));
            return false;
        }

        return true;
    }
};

class ChallengeSystemPlayerbotBlocker : public ServerScript
{
public:
    ChallengeSystemPlayerbotBlocker() : ServerScript("ip_challengesystem_playerbot_blocker", { SERVERHOOK_CAN_PACKET_RECEIVE }) {}

    bool CanPacketReceive(WorldSession* session, WorldPacket& packet) override
    {
        if (!session || packet.GetOpcode() != CMSG_MESSAGECHAT)
            return true;

        Player* player = session->GetPlayer();
        if (!player)
            return true;

        bool noBots = ChallengeManager::Instance().HasRestriction(player, "NO_BOTS");
        bool soloOnly = ChallengeManager::Instance().HasRestriction(player, "SOLO_ONLY");
        bool hardcoreBlocked = sConfigMgr->GetOption<bool>("ChallengeSystem.Hardcore.BlockPlayerBots", true) &&
                               ChallengeManager::Instance().HasRestriction(player, "HC_MANUAL_GROUP_ONLY_WITH_HC_TIER");
        bool blockAllBots = noBots || soloOnly;

        if (!blockAllBots && !hardcoreBlocked)
            return true;

        WorldPacket data(packet);
        uint32 type = 0;
        uint32 lang = 0;
        data >> type;
        data >> lang;

        std::string msg;
        if (!ExtractChatMessage(data, type, lang, msg))
            return true;

        if (msg.empty())
            return true;

        char prefix = msg.front();
        if (prefix != '.' && prefix != '!')
            return true;

        std::string command = msg.substr(1);
        auto tokens = SplitWhitespace(command);
        if (tokens.empty())
            return true;

        if (ToLower(tokens[0]) != "playerbots")
            return true;

        if (tokens.size() < 2)
            return true;

        std::string sub = ToLower(tokens[1]);
        if (sub == "rndbot")
        {
            SendPlayerError(player,
                GetConfigMessage(blockAllBots ? "ChallengeSystem.Message.BotsBlocked" : "ChallengeSystem.Message.RndBotsBlocked",
                    blockAllBots ? "Player bots are disabled by active Challenge restrictions."
                                 : "Random bot summoning is disabled by active Challenge restrictions."));
            return false;
        }

        if (sub != "bot")
            return true;

        if (tokens.size() < 3)
            return true;

        std::string botCmd = ToLower(tokens[2]);
        if (botCmd != "add" && botCmd != "addaccount" && botCmd != "login" && botCmd != "addclass")
            return true;

        if (botCmd == "addclass")
        {
            if (blockAllBots)
            {
                SendPlayerError(player,
                    GetConfigMessage("ChallengeSystem.Message.BotsBlocked",
                        "Player bots are disabled by active Challenge restrictions."));
                return false;
            }

            if (hardcoreBlocked)
            {
                SendPlayerError(player,
                    GetConfigMessage("ChallengeSystem.Message.RndBotsBlocked",
                        "Random bot summoning is disabled by active Challenge restrictions."));
                return false;
            }

            return true;
        }

        if (blockAllBots)
        {
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.BotsBlocked",
                    "Player bots are disabled by active Challenge restrictions."));
            return false;
        }

        if (!hardcoreBlocked)
            return true;

        if (tokens.size() < 4)
        {
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.BotsRequireHardcore",
                    "Only Hardcore characters may be summoned as bots."));
            return false;
        }

        std::string target = tokens[3];
        if (target == "*" || target == "!")
        {
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.BotsRequireHardcore",
                    "Only Hardcore characters may be summoned as bots."));
            return false;
        }

        if (botCmd == "addaccount")
        {
            if (!AreAccountBotsHardcore(target))
            {
                SendPlayerError(player,
                    GetConfigMessage("ChallengeSystem.Message.BotsRequireHardcore",
                        "Only Hardcore characters may be summoned as bots."));
                return false;
            }

            return true;
        }

        for (std::string const& name : SplitComma(target))
        {
            if (!IsHardcoreBotAllowed(player, name))
            {
                SendPlayerError(player,
                    GetConfigMessage("ChallengeSystem.Message.BotsRequireHardcore",
                        "Only Hardcore characters may be summoned as bots."));
                return false;
            }
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
            SendPlayerError(player,
                GetConfigMessage("ChallengeSystem.Message.AuctionBlocked",
                    "Auction House access is disabled by active Challenge restrictions."));
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
            SendPlayerError(receiverPlayer,
                GetConfigMessage("ChallengeSystem.Message.MailBlocked",
                    "Mail is disabled by active Challenge restrictions."));
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
    new ChallengeSystemGuildHooks();
    new ChallengeSystemPlayerbotBlocker();
    AddChallengeSystemCommands();
}
