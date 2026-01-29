#ifndef MOD_IP_CHALLENGESYSTEM_CHALLENGE_RESTRICTION_H
#define MOD_IP_CHALLENGESYSTEM_CHALLENGE_RESTRICTION_H

#include <string>
#include <cstdint>

class Player;

/**
 * Atomic restriction interface.
 *
 * Each restriction represents ONE enforceable rule.
 * Restrictions are composed into challenge presets.
 *
 * Enforcement philosophy:
 *  - Prefer BLOCK
 *  - WARN when blocking is not possible
 *  - GRACE TIMER only as last resort
 */
class ChallengeRestriction
{
public:
    virtual ~ChallengeRestriction() = default;

    // Unique string identifier (e.g. "NO_TRADE", "PERMADEATH")
    virtual std::string GetId() const = 0;

    // Called when a tier run starts
    virtual void OnTierStart(Player* /*player*/) {}

    // Called when a tier run ends (completed or failed)
    virtual void OnTierEnd(Player* /*player*/) {}

    // --- Optional hooks ---
    // Restrictions override only what they need.

    virtual bool OnTradeAttempt(Player* /*player*/) { return true; }
    virtual bool OnMailSend(Player* /*player*/) { return true; }
    virtual bool OnAuctionAction(Player* /*player*/) { return true; }
    virtual bool OnGroupInvite(Player* /*player*/, Player* /*target*/) { return true; }
    virtual bool OnGroupAccept(Player* /*player*/) { return true; }
    virtual bool OnSummonAccept(Player* /*player*/) { return true; }
    virtual bool OnItemUse(Player* /*player*/, uint32 /*itemId*/) { return true; }
    virtual void OnDeath(Player* /*player*/) {}
};

#endif // MOD_IP_CHALLENGESYSTEM_CHALLENGE_RESTRICTION_H
