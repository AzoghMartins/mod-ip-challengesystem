#ifndef MOD_IP_CHALLENGESYSTEM_CHALLENGE_MANAGER_H
#define MOD_IP_CHALLENGESYSTEM_CHALLENGE_MANAGER_H

#include "Define.h"

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Player;
class Group;
class Unit;
class Item;
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
    bool IsEnabled() const;

    // Challenge flags (bitmask)
    static constexpr uint32 FLAG_HARDCORE   = 1;
    static constexpr uint32 FLAG_SOLO_ONLY  = 2;
    static constexpr uint32 FLAG_NO_TRADE   = 4;
    static constexpr uint32 FLAG_NO_MAIL    = 8;
    static constexpr uint32 FLAG_NO_AUCTION = 16;
    static constexpr uint32 FLAG_NO_SUMMONS = 32;
    static constexpr uint32 FLAG_PERMADEATH = 64;
    static constexpr uint32 FLAG_LOW_QUALITY_ONLY = 128;
    static constexpr uint32 FLAG_SELF_CRAFTED = 256;
    static constexpr uint32 FLAG_POVERTY = 512;
    static constexpr uint32 FLAG_NO_GUILD_BANK = 1024;
    static constexpr uint32 FLAG_NO_MOUNTS = 2048;
    static constexpr uint32 FLAG_NO_BUFFS = 4096;
    static constexpr uint32 FLAG_NO_TALENTS = 8192;
    static constexpr uint32 FLAG_NO_QUEST_XP = 16384;
    static constexpr uint32 FLAG_ONLY_QUEST_XP = 32768;
    static constexpr uint32 FLAG_HALF_XP = 65536;
    static constexpr uint32 FLAG_QUARTER_XP = 131072;
    static constexpr uint32 FLAG_NO_BOTS = 262144;

    // Lifecycle
    void OnTierStart(Player* player);
    void OnTierEnd(Player* player);
    void HandlePlayerLogin(Player* player);
    void HandlePlayerLogout(Player* player);

    // Restriction registry
    void RegisterRestriction(std::shared_ptr<ChallengeRestriction> restriction);

    // Query
    bool HasRestriction(Player* player, const std::string& restrictionId);
    uint8 GetActiveTier(Player* player);
    uint32 GetActiveFlags(Player* player);
    void SetActiveTierFlags(Player* player, uint8 tier, uint32 flags);
    void ClearActiveTierFlags(Player* player);
    bool IsPermadead(Player* player);
    bool IsPermadeathPending(Player* player) const;
    void ClearPermadeathPending(uint32 guid);
    void RecordPvPDeath(Player* killed);
    void RecordPvEDeath(Player* killed);
    void UpsertChallengeRunActive(Player* player, uint8 tier, uint32 flags);
    bool IsHardcoreGuid(uint32 guid);
    uint32 EnforceEquipmentRestrictions(Player* player);
    void EnforceNoTalents(Player* player);
    void EnforcePovertyCap(Player* player);
    void HandlePlayerUpdate(Player* player, uint32 diff);
    void HandleTalentPoints(Player* player, uint32& points);
    void HandleGiveXP(Player* player, uint32& amount, uint8 xpSource);
    void HandleQuestXP(Player* player, uint32& xpValue);
    void HandleMoneyChange(Player* player, int32& amount);
    bool HandleEquipItem(Player* player, Item* item, uint8 slot, bool isLoading);
    bool HandleGuildBankAccess(Player* player);

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

    struct ActiveState
    {
        uint8 tier = 0;
        uint32 flags = 0;
    };

    ActiveState LoadActiveState(uint32 guid);
    ActiveState& GetOrLoadActiveState(uint32 guid);

    std::vector<std::shared_ptr<ChallengeRestriction>> _restrictions;
    std::unordered_map<uint32, ActiveState> _activeStates;
    std::unordered_set<uint32> _permadeathPendingKick;
    std::unordered_set<uint32> _permadeathCache;
    std::unordered_map<uint32, uint32> _pvpDeathMarks;
    std::unordered_map<uint32, uint32> _pveDeathMarks;
    std::unordered_map<uint32, uint32> _noBuffsUpdateAccumulator;
    std::unordered_map<uint32, uint32> _groupViolationGraceDeadline;
    std::unordered_map<uint32, uint32> _groupViolationLastWarningAt;
};

#endif // MOD_IP_CHALLENGESYSTEM_CHALLENGE_MANAGER_H
