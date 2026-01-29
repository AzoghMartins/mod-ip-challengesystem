#include "ChallengeManager.h"
#include "ChallengeRestriction.h"

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

bool ChallengeManager::HasRestriction(Player* /*player*/, const std::string& /*restrictionId*/) const
{
    return false;
}

bool ChallengeManager::HandleTradeAttempt(Player* /*player*/)
{
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

bool ChallengeManager::HandleGroupInvite(Player* /*player*/, Player* /*target*/)
{
    return true;
}

bool ChallengeManager::HandleGroupAccept(Player* /*player*/)
{
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
