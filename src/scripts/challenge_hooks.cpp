#include "ChallengeManager.h"
#include "Chat.h"
#include "Group.h"
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
};

void AddChallengeSystemScripts()
{
    new ChallengeSystemHooks();
}
