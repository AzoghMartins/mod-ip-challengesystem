#include "ChallengeManager.h"
#include "ChallengeRestriction.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Duration.h"
#include "GameTime.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "WorldSessionMgr.h"

#include <algorithm>

namespace
{
constexpr char kSettingTierSource[] = "mod-ip-challengesystem-tier";
constexpr char kSettingFlagsSource[] = "mod-ip-challengesystem-flags";

constexpr char kRestrictionHardcoreManualGroup[] = "HC_MANUAL_GROUP_ONLY_WITH_HC_TIER";
constexpr char kRestrictionSoloOnly[] = "SOLO_ONLY";
constexpr char kRestrictionNoTrade[] = "NO_TRADE";
constexpr char kRestrictionNoMail[] = "NO_MAIL";
constexpr char kRestrictionNoAuction[] = "NO_AUCTION";
constexpr char kRestrictionNoSummons[] = "NO_SUMMONS";
constexpr char kRestrictionPermadeath[] = "PERMADEATH";

constexpr uint8 kTierMax = 3;

constexpr uint8 kRunStateActive = 2;
constexpr uint8 kRunStateFailed = 3;

enum class PermadeathReason : uint8
{
    Unknown = 0,
    PvE = 1,
    PvP = 2,
    Battleground = 3,
    Arena = 4,
    Duel = 5,
    Environment = 6
};

uint32 GetSettingUInt(uint32 guid, char const* source)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT data FROM character_settings WHERE guid = {} AND source = '{}' LIMIT 1", guid, source);
    if (!result)
        return 0;

    Field* fields = result->Fetch();
    std::string data = fields[0].Get<std::string>();
    if (auto parsed = Acore::StringTo<uint32>(data))
        return *parsed;

    return 0;
}

void SetSettingUInt(uint32 guid, char const* source, uint32 value)
{
    std::string sourceStr(source);
    CharacterDatabase.EscapeString(sourceStr);
    CharacterDatabase.Execute(
        "REPLACE INTO character_settings (guid, source, data) VALUES ({}, '{}', '{}')",
        guid, sourceStr, value);
}

bool HasTestAura(Player* player, char const* configKey)
{
    if (!player)
        return false;

    uint32 auraId = sConfigMgr->GetOption<uint32>(configKey, 0);
    return auraId && player->HasAura(auraId);
}

bool IsPermadeathEnabled()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.Permadeath.Enable", true);
}

uint32 GetPermadeathKickDelaySeconds()
{
    return sConfigMgr->GetOption<uint32>("ChallengeSystem.Permadeath.KickDelaySeconds", 30);
}

bool ShouldBroadcastPermadeath()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.Permadeath.Broadcast", true);
}

bool ShouldCountPvPDeath()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.Permadeath.CountPvPDeaths", false);
}

bool ShouldCountBattlegroundDeath()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.Permadeath.CountBattlegroundDeaths", false);
}

bool ShouldCountArenaDeath()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.Permadeath.CountArenaDeaths", false);
}

bool ShouldCountDuelDeath()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.Permadeath.CountDuelDeaths", false);
}

bool IsDuelDeath(Player* player)
{
    if (!player || !player->duel)
        return false;

    return player->duel->State == DUEL_STATE_IN_PROGRESS;
}
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

void ChallengeManager::HandlePlayerLogin(Player* player)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    _activeStates[guid] = LoadActiveState(guid);
}

void ChallengeManager::HandlePlayerLogout(Player* player)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    _activeStates.erase(guid);
    _permadeathPendingKick.erase(guid);
    _permadeathCache.erase(guid);
    _pvpDeathMarks.erase(guid);
    _pveDeathMarks.erase(guid);
}

void ChallengeManager::RegisterRestriction(std::shared_ptr<ChallengeRestriction> restriction)
{
    _restrictions.push_back(restriction);
}

ChallengeManager::ActiveState ChallengeManager::LoadActiveState(uint32 guid)
{
    ActiveState state;
    uint32 tier = GetSettingUInt(guid, kSettingTierSource);
    uint32 flags = GetSettingUInt(guid, kSettingFlagsSource);

    if (tier > kTierMax)
        tier = 0;

    state.tier = static_cast<uint8>(tier);
    state.flags = flags;
    return state;
}

ChallengeManager::ActiveState& ChallengeManager::GetOrLoadActiveState(uint32 guid)
{
    auto itr = _activeStates.find(guid);
    if (itr != _activeStates.end())
        return itr->second;

    auto result = _activeStates.emplace(guid, LoadActiveState(guid));
    return result.first->second;
}

uint8 ChallengeManager::GetActiveTier(Player* player)
{
    if (!player)
        return 0;

    uint32 guid = player->GetGUID().GetCounter();
    return GetOrLoadActiveState(guid).tier;
}

uint32 ChallengeManager::GetActiveFlags(Player* player)
{
    if (!player)
        return 0;

    uint32 guid = player->GetGUID().GetCounter();
    ActiveState& state = GetOrLoadActiveState(guid);
    if (state.tier == 0)
        return 0;
    return state.flags;
}

void ChallengeManager::SetActiveTierFlags(Player* player, uint8 tier, uint32 flags)
{
    if (!player)
        return;

    if (tier > kTierMax)
        tier = 0;

    uint32 guid = player->GetGUID().GetCounter();
    SetSettingUInt(guid, kSettingTierSource, tier);
    SetSettingUInt(guid, kSettingFlagsSource, flags);
    _activeStates[guid] = { tier, flags };
}

void ChallengeManager::ClearActiveTierFlags(Player* player)
{
    SetActiveTierFlags(player, 0, 0);
}

bool ChallengeManager::IsPermadead(Player* player)
{
    if (!player)
        return false;

    uint32 guid = player->GetGUID().GetCounter();
    if (_permadeathCache.find(guid) != _permadeathCache.end())
        return true;

    QueryResult result = CharacterDatabase.Query(
        "SELECT is_dead FROM ip_permadeath WHERE guid = {} LIMIT 1", guid);
    if (!result)
        return false;

    Field* fields = result->Fetch();
    if (fields[0].Get<uint8>() == 0)
        return false;

    _permadeathCache.insert(guid);
    return true;
}

bool ChallengeManager::IsPermadeathPending(Player* player) const
{
    if (!player)
        return false;

    uint32 guid = player->GetGUID().GetCounter();
    return _permadeathPendingKick.find(guid) != _permadeathPendingKick.end();
}

void ChallengeManager::ClearPermadeathPending(uint32 guid)
{
    _permadeathPendingKick.erase(guid);
}

void ChallengeManager::RecordPvPDeath(Player* killed)
{
    if (!killed)
        return;

    _pvpDeathMarks[killed->GetGUID().GetCounter()] = GameTime::GetGameTime().count();
}

void ChallengeManager::RecordPvEDeath(Player* killed)
{
    if (!killed)
        return;

    _pveDeathMarks[killed->GetGUID().GetCounter()] = GameTime::GetGameTime().count();
}

bool ChallengeManager::HasRestriction(Player* player, const std::string& restrictionId)
{
    if (!player)
        return false;

    uint32 flags = GetActiveFlags(player);

    if (restrictionId == kRestrictionHardcoreManualGroup)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.Hardcore"))
            return true;
        return (flags & FLAG_HARDCORE) != 0;
    }
    if (restrictionId == kRestrictionSoloOnly)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.SoloOnly"))
            return true;
        return (flags & FLAG_SOLO_ONLY) != 0;
    }
    if (restrictionId == kRestrictionNoTrade)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoTrade"))
            return true;
        return (flags & FLAG_NO_TRADE) != 0;
    }
    if (restrictionId == kRestrictionNoMail)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoMail"))
            return true;
        return (flags & FLAG_NO_MAIL) != 0;
    }
    if (restrictionId == kRestrictionNoAuction)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoAuction"))
            return true;
        return (flags & FLAG_NO_AUCTION) != 0;
    }
    if (restrictionId == kRestrictionNoSummons)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoSummons"))
            return true;
        return (flags & FLAG_NO_SUMMONS) != 0;
    }
    if (restrictionId == kRestrictionPermadeath)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.Permadeath"))
            return true;
        return (flags & FLAG_PERMADEATH) != 0;
    }

    return false;
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

    uint32 guid = player->GetGUID().GetCounter();
    uint32 now = GameTime::GetGameTime().count();

    auto consumeRecent = [now, guid](std::unordered_map<uint32, uint32>& marks)
    {
        auto itr = marks.find(guid);
        if (itr == marks.end())
            return false;

        bool recent = (now >= itr->second) && (now - itr->second <= 10);
        marks.erase(itr);
        return recent;
    };

    bool wasPvp = consumeRecent(_pvpDeathMarks);
    bool wasPve = consumeRecent(_pveDeathMarks);

    if (!IsPermadeathEnabled())
        return false;

    if (!HasRestriction(player, kRestrictionPermadeath))
        return false;

    if (_permadeathPendingKick.find(guid) != _permadeathPendingKick.end())
        return false;

    if (IsPermadead(player))
        return false;

    PermadeathReason reason = PermadeathReason::Unknown;
    bool counts = true;

    if (player->InArena())
    {
        reason = PermadeathReason::Arena;
        counts = ShouldCountArenaDeath();
    }
    else if (player->InBattleground())
    {
        reason = PermadeathReason::Battleground;
        counts = ShouldCountBattlegroundDeath();
    }
    else if (IsDuelDeath(player))
    {
        reason = PermadeathReason::Duel;
        counts = ShouldCountDuelDeath();
    }
    else if (wasPvp)
    {
        reason = PermadeathReason::PvP;
        counts = ShouldCountPvPDeath();
    }
    else if (wasPve)
    {
        reason = PermadeathReason::PvE;
    }
    else
    {
        reason = PermadeathReason::Environment;
    }

    if (!counts)
        return false;

    uint32 deathTime = GameTime::GetGameTime().count();
    CharacterDatabase.Execute(
        "INSERT INTO ip_permadeath (guid, is_dead, death_time, death_map, death_x, death_y, death_z, death_o, death_reason) "
        "VALUES ({}, 1, {}, {}, {}, {}, {}, {}, {}) "
        "ON DUPLICATE KEY UPDATE is_dead = 1, death_time = VALUES(death_time), death_map = VALUES(death_map), "
        "death_x = VALUES(death_x), death_y = VALUES(death_y), death_z = VALUES(death_z), death_o = VALUES(death_o), "
        "death_reason = VALUES(death_reason)",
        guid, deathTime, player->GetMapId(), player->GetPositionX(), player->GetPositionY(),
        player->GetPositionZ(), player->GetOrientation(), static_cast<uint8>(reason));

    _permadeathCache.insert(guid);
    _permadeathPendingKick.insert(guid);

    uint8 tier = GetActiveTier(player);
    uint32 flags = GetActiveFlags(player);
    if (tier > 0)
    {
        CharacterDatabase.Execute(
            "INSERT INTO ip_challenge_runs (guid, tier, state, picked_flags, failed_flags, ended_at) "
            "VALUES ({}, {}, {}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE state = {}, failed_flags = failed_flags | {}, ended_at = {}",
            guid, tier, kRunStateFailed, flags, FLAG_PERMADEATH, deathTime,
            kRunStateFailed, FLAG_PERMADEATH, deathTime);
    }

    ClearActiveTierFlags(player);

    if (ShouldBroadcastPermadeath())
    {
        std::string message = Acore::StringFormat(
            "Hardcore runner {} (Level {}) has been slain. All hail the fallen!",
            player->GetName(), player->GetLevel());
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message);
    }

    uint32 kickDelay = GetPermadeathKickDelaySeconds();
    player->m_Events.AddEventAtOffset([guid]()
        {
            bool kicked = false;
            if (Player* victim = ObjectAccessor::FindPlayerByLowGUID(guid))
            {
                if (WorldSession* session = victim->GetSession())
                {
                    time_t now = GameTime::GetGameTime().count();
                    session->SetLogoutStartTime(now > 20 ? now - 20 : 0);
                    kicked = true;
                }
            }

            if (kicked)
                ChallengeManager::Instance().ClearPermadeathPending(guid);
        }, Seconds(kickDelay));

    return true;
}

bool ChallengeManager::HandleItemUse(Player* /*player*/, uint32 /*itemId*/)
{
    return true;
}

void ChallengeManager::UpsertChallengeRunActive(Player* player, uint8 tier, uint32 flags)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();
    uint32 now = GameTime::GetGameTime().count();
    CharacterDatabase.Execute(
        "INSERT INTO ip_challenge_runs (guid, tier, state, picked_flags, failed_flags, successful_flags, started_at, ended_at) "
        "VALUES ({}, {}, {}, {}, 0, 0, {}, 0) "
        "ON DUPLICATE KEY UPDATE state = {}, picked_flags = {}, failed_flags = 0, successful_flags = 0, started_at = {}, ended_at = 0",
        guid, tier, kRunStateActive, flags, now,
        kRunStateActive, flags, now);
}
