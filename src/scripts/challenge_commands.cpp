#include "ChallengeManager.h"
#include "Chat.h"
#include "CommandScript.h"
#include "Player.h"
#include "StringConvert.h"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace Acore::ChatCommands;

namespace
{
bool ParseTierFlags(std::string_view args, uint32& tier, uint32& flags)
{
    std::istringstream iss;
    iss.str(std::string(args));
    std::string token;
    bool tierSet = false;
    bool flagsSet = false;

    while (iss >> token)
    {
        if (token == "tier")
        {
            if (!(iss >> token))
                return false;

            if (auto parsed = Acore::StringTo<uint32>(token))
            {
                tier = *parsed;
                tierSet = true;
            }
            else
            {
                return false;
            }
        }
        else if (token == "flags")
        {
            if (!(iss >> token))
                return false;

            if (auto parsed = Acore::StringTo<uint32>(token))
            {
                flags = *parsed;
                flagsSet = true;
            }
            else
            {
                return false;
            }
        }
        else if (!tierSet)
        {
            if (auto parsed = Acore::StringTo<uint32>(token))
            {
                tier = *parsed;
                tierSet = true;
            }
            else
            {
                return false;
            }
        }
        else if (!flagsSet)
        {
            if (auto parsed = Acore::StringTo<uint32>(token))
            {
                flags = *parsed;
                flagsSet = true;
            }
            else
            {
                return false;
            }
        }
    }

    return tierSet && flagsSet;
}

std::string DescribeFlags(uint32 flags)
{
    std::vector<std::string> parts;

    if (flags & ChallengeManager::FLAG_HARDCORE)
        parts.emplace_back("Hardcore");
    if (flags & ChallengeManager::FLAG_SOLO_ONLY)
        parts.emplace_back("SoloOnly");
    if (flags & ChallengeManager::FLAG_NO_TRADE)
        parts.emplace_back("NoTrade");
    if (flags & ChallengeManager::FLAG_NO_MAIL)
        parts.emplace_back("NoMail");
    if (flags & ChallengeManager::FLAG_NO_AUCTION)
        parts.emplace_back("NoAuction");
    if (flags & ChallengeManager::FLAG_NO_SUMMONS)
        parts.emplace_back("NoSummons");
    if (flags & ChallengeManager::FLAG_PERMADEATH)
        parts.emplace_back("Permadeath");

    if (parts.empty())
        return "none";

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i > 0)
            result.append(", ");
        result.append(parts[i]);
    }

    return result;
}
}

class ip_challenge_commandscript : public CommandScript
{
public:
    ip_challenge_commandscript() : CommandScript("ip_challenge_commandscript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable ipChallengeCommandTable =
        {
            { "set",    HandleIpChallengeSet,    SEC_GAMEMASTER, Console::No },
            { "clear",  HandleIpChallengeClear,  SEC_GAMEMASTER, Console::No },
            { "status", HandleIpChallengeStatus, SEC_GAMEMASTER, Console::No }
        };

        static ChatCommandTable commandTable =
        {
            { "ipchallenge", ipChallengeCommandTable }
        };

        return commandTable;
    }

    static Player* ResolveTarget(ChatHandler* handler)
    {
        Optional<PlayerIdentifier> target = PlayerIdentifier::FromTargetOrSelf(handler);
        if (!target || !target->IsConnected())
            return nullptr;

        return target->GetConnectedPlayer();
    }

    static bool HandleIpChallengeSet(ChatHandler* handler, Tail args)
    {
        Player* player = ResolveTarget(handler);
        if (!player)
            return false;

        uint32 tier = 0;
        uint32 flags = 0;
        if (!ParseTierFlags(args, tier, flags))
        {
            handler->SendSysMessage("Usage: .ipchallenge set tier <0-3> flags <mask>");
            return false;
        }

        if (tier > 3)
        {
            handler->SendSysMessage("Tier must be between 0 and 3.");
            return false;
        }

        if (tier == 0 || flags == 0)
        {
            ChallengeManager::Instance().ClearActiveTierFlags(player);
            handler->PSendSysMessage("Active challenge cleared for {}.", player->GetName());
            return true;
        }

        ChallengeManager::Instance().SetActiveTierFlags(player, static_cast<uint8>(tier), flags);
        ChallengeManager::Instance().UpsertChallengeRunActive(player, static_cast<uint8>(tier), flags);

        handler->PSendSysMessage("Active challenge set for {}: tier {} flags {} ({})",
            player->GetName(), tier, flags, DescribeFlags(flags));
        return true;
    }

    static bool HandleIpChallengeClear(ChatHandler* handler)
    {
        Player* player = ResolveTarget(handler);
        if (!player)
            return false;

        ChallengeManager::Instance().ClearActiveTierFlags(player);
        handler->PSendSysMessage("Active challenge cleared for {}.", player->GetName());
        return true;
    }

    static bool HandleIpChallengeStatus(ChatHandler* handler)
    {
        Player* player = ResolveTarget(handler);
        if (!player)
            return false;

        uint8 tier = ChallengeManager::Instance().GetActiveTier(player);
        uint32 flags = ChallengeManager::Instance().GetActiveFlags(player);
        bool permadead = ChallengeManager::Instance().IsPermadead(player);

        handler->PSendSysMessage("Active tier: {} | Active flags: {} ({})", tier, flags, DescribeFlags(flags));
        handler->PSendSysMessage("Permadeath: {}", permadead ? "YES" : "NO");
        return true;
    }
};

void AddChallengeSystemCommands()
{
    new ip_challenge_commandscript();
}
