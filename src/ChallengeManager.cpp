#include "ChallengeManager.h"
#include "ChallengeRestriction.h"
#include "Config.h"
#include "Group.h"
#include "Player.h"

namespace
{
constexpr char kRestrictionHardcoreManualGroup[] = "HC_MANUAL_GROUP_ONLY_WITH_HC_TIER";
constexpr char kRestrictionSoloOnly[] = "SOLO_ONLY";
constexpr char kRestrictionNoTrade[] = "NO_TRADE";
constexpr char kRestrictionNoMail[] = "NO_MAIL";
constexpr char kRestrictionNoAuction[] = "NO_AUCTION";
constexpr char kRestrictionNoSummons[] = "NO_SUMMONS";
constexpr char kRestrictionPermadeath[] = "PERMADEATH";
}

ChallengeManager& ChallengeManager::Instance()
{
    static ChallengeManager instance;
    return instance;
}

void ChallengeManager::OnTierStart(Player* /*player*/)
{
}

void ChallengeManager::OnTierEnd(Player* /*player*/)
{
}

void ChallengeManager::HandlePlayerUpdate(Player* player)
{
    if (!player)
        return;

    if (!player->IsAlive())
        return;

    uint64 guid = player->GetGUID().GetRawValue();
    if (HasRestriction(player, kRestrictionPermadeath))
        _permadeathActive.insert(guid);
    else
        _permadeathActive.erase(guid);
}

void ChallengeManager::HandlePlayerLogout(Player* player)
{
    if (!player)
        return;

    uint64 guid = player->GetGUID().GetRawValue();
    _permadeathActive.erase(guid);
    _permadeathFailures.erase(guid);
}

void ChallengeManager::RegisterRestriction(std::shared_ptr<ChallengeRestriction> restriction)
{
    _restrictions.push_back(restriction);
}

bool ChallengeManager::HasRestriction(Player* player, const std::string& restrictionId) const
{
    if (!player)
        return false;

    uint32 auraId = 0;
    if (restrictionId == kRestrictionHardcoreManualGroup)
    {
        auraId = sConfigMgr->GetOption<uint32>("ChallengeSystem.TestAura.Hardcore", 0);
    }
    else if (restrictionId == kRestrictionNoTrade)
    {
        auraId = sConfigMgr->GetOption<uint32>("ChallengeSystem.TestAura.NoTrade", 0);
    }
    else if (restrictionId == kRestrictionNoMail)
    {
        auraId = sConfigMgr->GetOption<uint32>("ChallengeSystem.TestAura.NoMail", 0);
    }
    else if (restrictionId == kRestrictionNoAuction)
    {
        auraId = sConfigMgr->GetOption<uint32>("ChallengeSystem.TestAura.NoAuction", 0);
    }
    else if (restrictionId == kRestrictionSoloOnly)
    {
        auraId = sConfigMgr->GetOption<uint32>("ChallengeSystem.TestAura.SoloOnly", 0);
    }
    else if (restrictionId == kRestrictionNoSummons)
    {
        auraId = sConfigMgr->GetOption<uint32>("ChallengeSystem.TestAura.NoSummons", 0);
    }
    else if (restrictionId == kRestrictionPermadeath)
    {
        auraId = sConfigMgr->GetOption<uint32>("ChallengeSystem.TestAura.Permadeath", 0);
    }
    else
    {
        return false;
    }

    // TODO: Replace aura-based testing with real tier/restriction state.
    if (!auraId)
        return false;

    return player->HasAura(auraId);
}

bool ChallengeManager::HandleTradeAttempt(Player* player, Player* target)
{
    if (!player)
        return true;

    if (HasRestriction(player, kRestrictionNoTrade))
        return false;

    if (target && HasRestriction(target, kRestrictionNoTrade))
        return false;

    return true;
}

bool ChallengeManager::HandleMailSend(Player* player)
{
    if (!player)
        return true;

    if (HasRestriction(player, kRestrictionNoMail))
        return false;

    return true;
}

bool ChallengeManager::HandleAuctionAction(Player* player)
{
    if (!player)
        return true;

    if (HasRestriction(player, kRestrictionNoAuction))
        return false;

    return true;
}

bool ChallengeManager::HandleGroupInvite(Player* player, Player* target)
{
    if (!player || !target)
        return true;

    if (HasRestriction(player, kRestrictionSoloOnly) || HasRestriction(target, kRestrictionSoloOnly))
        return false;

    bool playerHardcore = HasRestriction(player, kRestrictionHardcoreManualGroup);
    bool targetHardcore = HasRestriction(target, kRestrictionHardcoreManualGroup);

    if (playerHardcore != targetHardcore)
        return false;

    return true;
}

bool ChallengeManager::HandleGroupAccept(Player* player, Group* group)
{
    if (!player || !group)
        return true;

    if (HasRestriction(player, kRestrictionSoloOnly))
        return false;

    bool playerHardcore = HasRestriction(player, kRestrictionHardcoreManualGroup);

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member)
            continue;

        if (HasRestriction(member, kRestrictionSoloOnly))
            return false;

        bool memberHardcore = HasRestriction(member, kRestrictionHardcoreManualGroup);
        if (memberHardcore != playerHardcore)
            return false;
    }

    return true;
}

bool ChallengeManager::HandleSummonAccept(Player* player, Unit* target, uint32 /*options*/)
{
    if (!player)
        return true;

    if (!HasRestriction(player, kRestrictionNoSummons))
        return true;

    // Summon teleports pass the summoner as target; other teleports often do not.
    // TODO: Extend detection if additional summon/portal sources need coverage.
    if (!target || target == player)
        return true;

    return false;
}

bool ChallengeManager::HandleDeath(Player* player)
{
    if (!player)
        return false;

    uint64 guid = player->GetGUID().GetRawValue();
    bool permadeathActive = (_permadeathActive.find(guid) != _permadeathActive.end()) ||
        HasRestriction(player, kRestrictionPermadeath);

    if (!permadeathActive)
        return false;

    if (_permadeathFailures.find(guid) != _permadeathFailures.end())
        return false;

    _permadeathFailures.insert(guid);
    return true;
}

bool ChallengeManager::HandleItemUse(Player* /*player*/, uint32 /*itemId*/)
{
    return true;
}
