#ifndef MOD_IP_CHALLENGESYSTEM_CHALLENGE_MANAGER_H
#define MOD_IP_CHALLENGESYSTEM_CHALLENGE_MANAGER_H

#include "Define.h"

#include <vector>
#include <memory>
#include <string>
#include <unordered_set>

class Player;
class Group;
class Unit;
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
    void HandlePlayerUpdate(Player* player);
    void HandlePlayerLogout(Player* player);

    // Restriction registry
    void RegisterRestriction(std::shared_ptr<ChallengeRestriction> restriction);

    // Query
    bool HasRestriction(Player* player, const std::string& restrictionId) const;

    // Event dispatch (called from hooks)
    bool HandleTradeAttempt(Player* player, Player* target);
    bool HandleMailSend(Player* player);
    bool HandleAuctionAction(Player* player);
    bool HandleGroupInvite(Player* player, Player* target);
    bool HandleGroupAccept(Player* player, Group* group);
    bool HandleSummonAccept(Player* player, Unit* target, uint32 options);
    bool HandleItemUse(Player* player, uint32 itemId);
    bool HandleDeath(Player* player);

private:
    ChallengeManager() = default;

    std::vector<std::shared_ptr<ChallengeRestriction>> _restrictions;
    std::unordered_set<uint64> _permadeathActive;
    std::unordered_set<uint64> _permadeathFailures;
};

#endif // MOD_IP_CHALLENGESYSTEM_CHALLENGE_MANAGER_H
