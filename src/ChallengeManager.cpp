#include "ChallengeManager.h"
#include "ChallengeRestriction.h"
#include "Config.h"
#include "Group.h"
#include "Player.h"

namespace
{
constexpr char kRestrictionHardcoreManualGroup[] = "HC_MANUAL_GROUP_ONLY_WITH_HC_TIER";
constexpr char kRestrictionNoTrade[] = "NO_TRADE";
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

bool ChallengeManager::HandleMailSend(Player* /*player*/)
{
    return true;
}

bool ChallengeManager::HandleAuctionAction(Player* /*player*/)
{
    return true;
}

bool ChallengeManager::HandleGroupInvite(Player* player, Player* target)
{
    if (!player || !target)
        return true;

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

    bool playerHardcore = HasRestriction(player, kRestrictionHardcoreManualGroup);

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member)
            continue;

        bool memberHardcore = HasRestriction(member, kRestrictionHardcoreManualGroup);
        if (memberHardcore != playerHardcore)
            return false;
    }

    return true;
}

bool ChallengeManager::HandleSummonAccept(Player* /*player*/)
{
    return true;
}

bool ChallengeManager::HandleItemUse(Player* /*player*/, uint32 /*itemId*/)
{
    return true;
}

void ChallengeManager::HandleDeath(Player* /*player*/)
{
}
