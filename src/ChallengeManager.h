#ifndef MOD_IP_CHALLENGESYSTEM_CHALLENGE_MANAGER_H
#define MOD_IP_CHALLENGESYSTEM_CHALLENGE_MANAGER_H

#include <vector>
#include <memory>

class Player;
class ChallengeRestriction;

/**
 * ChallengeManager
 *
 * Central authority for:
 *  - Active tier state
 *  - Registered restrictions
 *  - Dispatching events to restrictions
 *
 * This class intentionally contains NO game logic yet.
 */
class ChallengeManager
{
public:
    static ChallengeManager& Instance();

    // Lifecycle
    void OnTierStart(Player* player);
    void OnTierEnd(Player* player);

    // Restriction registry
    void RegisterRestriction(std::shared_ptr<ChallengeRestriction> restriction);

    // Query
    bool HasRestriction(Player* player, const std::string& restrictionId) const;

    // Event dispatch (called from hooks)
    bool HandleTradeAttempt(Player* player);
    bool HandleMailSend(Player* player);
    bool HandleAuctionAction(Player* player);
    bool HandleGroupInvite(Player* player, Player* target);
    bool HandleGroupAccept(Player* player);
    bool HandleSummonAccept(Player* player);
    bool HandleItemUse(Player* player, uint32 itemId);
    void HandleDeath(Player* player);

private:
    ChallengeManager() = default;

    std::vector<std::shared_ptr<ChallengeRestriction>> _restrictions;
};

#endif // MOD_IP_CHALLENGESYSTEM_CHALLENGE_MANAGER_H
