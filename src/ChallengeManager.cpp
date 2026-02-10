#include "ChallengeManager.h"
#include "ChallengeRestriction.h"
#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Duration.h"
#include "GameTime.h"
#include "Group.h"
#include "GuildMgr.h"
#include "Item.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "UpdateFields.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <sstream>

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
constexpr char kRestrictionLowQualityOnly[] = "LOW_QUALITY_ONLY";
constexpr char kRestrictionSelfCrafted[] = "SELF_CRAFTED";
constexpr char kRestrictionPoverty[] = "POVERTY";
constexpr char kRestrictionNoGuildBank[] = "NO_GUILD_BANK";
constexpr char kRestrictionNoMounts[] = "NO_MOUNTS";
constexpr char kRestrictionNoBuffs[] = "NO_BUFFS";
constexpr char kRestrictionNoTalents[] = "NO_TALENTS";
constexpr char kRestrictionNoQuestXP[] = "NO_QUEST_XP";
constexpr char kRestrictionOnlyQuestXP[] = "ONLY_QUEST_XP";
constexpr char kRestrictionHalfXP[] = "HALF_XP";
constexpr char kRestrictionQuarterXP[] = "QUARTER_XP";
constexpr char kRestrictionNoBots[] = "NO_BOTS";

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

bool IsChallengeSystemEnabled()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.Enable", true);
}

uint32 GetGroupGracePeriodSeconds()
{
    return sConfigMgr->GetOption<uint32>("ChallengeSystem.GroupGracePeriod", 45);
}

uint32 GetPermadeathKickDelaySeconds()
{
    return sConfigMgr->GetOption<uint32>("ChallengeSystem.Permadeath.KickDelaySeconds", 30);
}

bool ShouldBroadcastPermadeath()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.Permadeath.Broadcast", true);
}

std::string GetPermadeathBroadcastMessage(Player* player)
{
    std::string message = sConfigMgr->GetOption<std::string>(
        "ChallengeSystem.Permadeath.BroadcastMessage",
        "Hardcore runner {name} (Level {level}) has been slain. All hail the fallen!");

    if (!player)
        return message;

    std::string::size_type pos = 0;
    while ((pos = message.find("{name}", pos)) != std::string::npos)
    {
        message.replace(pos, 6, player->GetName());
        pos += player->GetName().size();
    }

    std::string levelToken = "{level}";
    std::string levelValue = Acore::ToString(player->GetLevel());
    pos = 0;
    while ((pos = message.find(levelToken, pos)) != std::string::npos)
    {
        message.replace(pos, levelToken.size(), levelValue);
        pos += levelValue.size();
    }

    return message;
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

bool ShouldAllowHardcoreLfg()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.Hardcore.AllowLfg", true);
}

bool ShouldAllowSoloLfg()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.SoloOnly.AllowLfg", false);
}

std::string GetMessage(Player* player, char const* key, char const* fallback)
{
    if (!player || !player->GetSession())
        return {};

    return sConfigMgr->GetOption<std::string>(key, fallback);
}

void SendMessage(Player* player, char const* key, char const* fallback)
{
    std::string message = GetMessage(player, key, fallback);
    if (message.empty() || !player || !player->GetSession())
        return;

    ChatHandler(player->GetSession()).SendSysMessage(message.c_str());
}

void TryAutoJoinHardcoreGuild(Player* player)
{
    if (!player)
        return;

    if (!sConfigMgr->GetOption<bool>("ChallengeSystem.Hardcore.AutoReinviteGuild", true))
        return;

    std::string guildName = sConfigMgr->GetOption<std::string>("ChallengeSystem.Hardcore.GuildName", "");
    if (guildName.empty())
        return;

    Guild* guild = sGuildMgr->GetGuildByName(guildName);
    if (!guild)
    {
        SendMessage(player, "ChallengeSystem.Message.HardcoreGuildMissing",
            "Hardcore guild not found. Contact a GM.");
        return;
    }

    if (player->GetGuildId() == guild->GetId())
        return;

    if (player->GetGuildId() != 0)
    {
        SendMessage(player, "ChallengeSystem.Message.HardcoreGuildOtherGuild",
            "You are already in another guild. Leave it to join the Hardcore guild.");
        return;
    }

    if (guild->AddMember(player->GetGUID()))
    {
        SendMessage(player, "ChallengeSystem.Message.HardcoreGuildJoined",
            "You have been added to the Hardcore guild.");
    }
    else
    {
        SendMessage(player, "ChallengeSystem.Message.HardcoreGuildJoinFailed",
            "Failed to join the Hardcore guild. Contact a GM.");
    }
}

uint32 GetPovertyGoldCap(uint8 tier)
{
    if (tier == 1)
        return sConfigMgr->GetOption<uint32>("ChallengeSystem.Poverty.GoldCap.Tier1", 0);
    if (tier == 2)
        return sConfigMgr->GetOption<uint32>("ChallengeSystem.Poverty.GoldCap.Tier2", 0);
    if (tier == 3)
        return sConfigMgr->GetOption<uint32>("ChallengeSystem.Poverty.GoldCap.Tier3", 0);
    return 0;
}

float GetHalfXPMultiplier()
{
    return sConfigMgr->GetOption<float>("ChallengeSystem.XP.HalfMultiplier", 0.5f);
}

float GetQuarterXPMultiplier()
{
    return sConfigMgr->GetOption<float>("ChallengeSystem.XP.QuarterMultiplier", 0.25f);
}

uint8 GetLowQualityMaxQuality()
{
    uint32 value = sConfigMgr->GetOption<uint32>("ChallengeSystem.LowQualityOnly.MaxQuality", 1);
    return static_cast<uint8>(value);
}

uint32 GetNoBuffsScanIntervalMs()
{
    return sConfigMgr->GetOption<uint32>("ChallengeSystem.NoBuffs.ScanIntervalMs", 1000);
}

bool AllowPassiveBuffs()
{
    return sConfigMgr->GetOption<bool>("ChallengeSystem.NoBuffs.AllowPassive", true);
}

const std::unordered_set<uint32>& GetNoBuffsAllowList()
{
    static std::string cached;
    static std::unordered_set<uint32> allowList;

    std::string current = sConfigMgr->GetOption<std::string>("ChallengeSystem.NoBuffs.AllowSpells", "");
    if (current == cached)
        return allowList;

    allowList.clear();
    cached = current;

    std::stringstream ss(current);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        if (auto parsed = Acore::StringTo<uint32>(token))
            allowList.insert(*parsed);
    }

    return allowList;
}

bool IsQuestXPSource(uint8 xpSource)
{
    return xpSource == XPSOURCE_QUEST || xpSource == XPSOURCE_QUEST_DF;
}
}

ChallengeManager& ChallengeManager::Instance()
{
    static ChallengeManager instance;
    return instance;
}

bool ChallengeManager::IsEnabled() const
{
    return IsChallengeSystemEnabled();
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

    if (!IsEnabled())
        return;

    uint32 guid = player->GetGUID().GetCounter();
    _activeStates[guid] = LoadActiveState(guid);

    EnforceEquipmentRestrictions(player);
    EnforceNoTalents(player);
    EnforcePovertyCap(player);
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
    _noBuffsUpdateAccumulator.erase(guid);
    _groupViolationGraceDeadline.erase(guid);
    _groupViolationLastWarningAt.erase(guid);
}

uint32 ChallengeManager::EnforceEquipmentRestrictions(Player* player)
{
    if (!player)
        return 0;

    if (!HasRestriction(player, kRestrictionLowQualityOnly) && !HasRestriction(player, kRestrictionSelfCrafted))
        return 0;

    uint32 removed = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;

        if (HandleEquipItem(player, item, slot, true))
            continue;

        ItemPosCountVec dest;
        InventoryResult msg = player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false);
        if (msg == EQUIP_ERR_OK)
        {
            player->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
            player->StoreItem(dest, item, true);
            ++removed;
            continue;
        }

        // If inventory is full, force-unequip and mail the item so restrictions cannot be bypassed.
        player->MoveItemFromInventory(INVENTORY_SLOT_BAG_0, slot, true);
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        item->DeleteFromInventoryDB(trans);
        item->SaveToDB(trans);

        MailDraft("Challenge restriction: item unequipped",
            "An equipped item violated active Challenge restrictions and was mailed to you because your bags were full.")
            .AddItem(item)
            .SendMailTo(trans, player, MailSender(player, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);

        CharacterDatabase.CommitTransaction(trans);
        ++removed;
    }

    return removed;
}

void ChallengeManager::EnforceNoTalents(Player* player)
{
    if (!player)
        return;

    if (!HasRestriction(player, kRestrictionNoTalents))
        return;

    if (player->GetFreeTalentPoints() == 0)
        return;

    player->SetFreeTalentPoints(0);
    player->SendTalentsInfoData(false);
}

void ChallengeManager::EnforcePovertyCap(Player* player)
{
    if (!player)
        return;

    if (!HasRestriction(player, kRestrictionPoverty))
        return;

    uint8 tier = GetActiveTier(player);
    uint32 cap = GetPovertyGoldCap(tier);
    if (cap == 0)
        return;

    if (player->GetMoney() > cap)
        player->SetMoney(cap);
}

void ChallengeManager::HandlePlayerUpdate(Player* player, uint32 diff)
{
    if (!player)
        return;

    uint32 guid = player->GetGUID().GetCounter();

    if (!IsEnabled())
    {
        _noBuffsUpdateAccumulator.erase(guid);
        _groupViolationGraceDeadline.erase(guid);
        _groupViolationLastWarningAt.erase(guid);
        return;
    }

    if (HasRestriction(player, kRestrictionNoMounts))
    {
        if (player->IsMounted())
            player->Dismount();
    }

    Group* group = player->GetGroup();
    bool invalidManualGroup = group && !group->isLFGGroup() && !HandleGroupAccept(player, group);
    if (!invalidManualGroup)
    {
        _groupViolationGraceDeadline.erase(guid);
        _groupViolationLastWarningAt.erase(guid);
    }
    else
    {
        uint32 gracePeriod = GetGroupGracePeriodSeconds();
        if (gracePeriod == 0)
        {
            player->RemoveFromGroup(GROUP_REMOVEMETHOD_LEAVE);
            SendMessage(player, "ChallengeSystem.Message.GroupBlocked",
                "Grouping is disabled by active Challenge restrictions.");
            _groupViolationGraceDeadline.erase(guid);
            _groupViolationLastWarningAt.erase(guid);
        }
        else
        {
            uint32 now = GameTime::GetGameTime().count();
            uint32& deadline = _groupViolationGraceDeadline[guid];
            uint32& lastWarnAt = _groupViolationLastWarningAt[guid];

            if (deadline == 0 || now > deadline)
            {
                deadline = now + gracePeriod;
                lastWarnAt = 0;
            }

            if (now >= deadline)
            {
                player->RemoveFromGroup(GROUP_REMOVEMETHOD_LEAVE);
                SendMessage(player, "ChallengeSystem.Message.GroupBlocked",
                    "Grouping is disabled by active Challenge restrictions.");
                _groupViolationGraceDeadline.erase(guid);
                _groupViolationLastWarningAt.erase(guid);
            }
            else if ((lastWarnAt == 0 || now - lastWarnAt >= 10) && player->GetSession())
            {
                ChatHandler(player->GetSession()).SendSysMessage(
                    Acore::StringFormat("Challenge restriction: leave your group within {} seconds or you will be removed.",
                        deadline - now).c_str());
                lastWarnAt = now;
            }
        }
    }

    if (!HasRestriction(player, kRestrictionNoBuffs))
    {
        _noBuffsUpdateAccumulator.erase(guid);
        return;
    }

    uint32& accumulator = _noBuffsUpdateAccumulator[guid];
    accumulator = (accumulator > 60000 ? 60000 : accumulator) + diff;
    uint32 interval = GetNoBuffsScanIntervalMs();
    if (interval == 0)
        interval = 1000;

    if (accumulator < interval)
        return;

    accumulator = 0;

    const auto& allowList = GetNoBuffsAllowList();
    bool allowPassive = AllowPassiveBuffs();

    std::vector<uint32> toRemove;
    Unit::AuraApplicationMap const& auras = player->GetAppliedAuras();
    toRemove.reserve(auras.size());

    for (auto const& itr : auras)
    {
        AuraApplication const* app = itr.second;
        if (!app)
            continue;

        Aura* aura = app->GetBase();
        if (!aura)
            continue;

        SpellInfo const* spellInfo = aura->GetSpellInfo();
        if (!spellInfo)
            continue;

        if (!spellInfo->IsPositive())
            continue;

        if (allowPassive && spellInfo->IsPassive())
            continue;

        if (allowList.find(spellInfo->Id) != allowList.end())
            continue;

        toRemove.push_back(spellInfo->Id);
    }

    for (uint32 spellId : toRemove)
        player->RemoveAura(spellId);
}

void ChallengeManager::HandleTalentPoints(Player* player, uint32& points)
{
    if (!player)
        return;

    if (!HasRestriction(player, kRestrictionNoTalents))
        return;

    points = 0;
}

void ChallengeManager::HandleGiveXP(Player* player, uint32& amount, uint8 xpSource)
{
    if (!player)
        return;

    if (amount == 0)
        return;

    if (HasRestriction(player, kRestrictionOnlyQuestXP) && !IsQuestXPSource(xpSource))
    {
        amount = 0;
        return;
    }

    if (HasRestriction(player, kRestrictionNoQuestXP) && IsQuestXPSource(xpSource))
    {
        amount = 0;
        return;
    }

    float multiplier = 1.0f;
    if (HasRestriction(player, kRestrictionQuarterXP))
        multiplier = GetQuarterXPMultiplier();
    else if (HasRestriction(player, kRestrictionHalfXP))
        multiplier = GetHalfXPMultiplier();

    if (multiplier < 0.0f)
        multiplier = 0.0f;

    amount = static_cast<uint32>(amount * multiplier);
}

void ChallengeManager::HandleQuestXP(Player* player, uint32& xpValue)
{
    if (!player)
        return;

    if (!HasRestriction(player, kRestrictionNoQuestXP))
        return;

    xpValue = 0;
}

void ChallengeManager::HandleMoneyChange(Player* player, int32& amount)
{
    if (!player)
        return;

    if (amount <= 0)
        return;

    if (!HasRestriction(player, kRestrictionPoverty))
        return;

    uint8 tier = GetActiveTier(player);
    uint32 cap = GetPovertyGoldCap(tier);
    if (cap == 0)
        return;

    uint64 current = player->GetMoney();
    if (current >= cap)
    {
        amount = 0;
        return;
    }

    uint64 incoming = static_cast<uint64>(amount);
    if (current + incoming > cap)
        amount = static_cast<int32>(cap - current);
}

bool ChallengeManager::HandleEquipItem(Player* player, Item* item, uint8 /*slot*/, bool /*isLoading*/)
{
    if (!player || !item)
        return true;

    if (HasRestriction(player, kRestrictionLowQualityOnly))
    {
        ItemTemplate const* proto = item->GetTemplate();
        if (!proto || proto->Quality > GetLowQualityMaxQuality())
            return false;
    }

    if (HasRestriction(player, kRestrictionSelfCrafted))
    {
        // Missing creator GUID means the item wasn't crafted by this character.
        if (item->GetGuidValue(ITEM_FIELD_CREATOR) != player->GetGUID())
            return false;
    }

    return true;
}

bool ChallengeManager::HandleGuildBankAccess(Player* player)
{
    if (!player)
        return true;

    if (HasRestriction(player, kRestrictionNoGuildBank))
        return false;

    return true;
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

    if (tier > 0 && (flags & FLAG_HARDCORE))
        TryAutoJoinHardcoreGuild(player);
}

void ChallengeManager::ClearActiveTierFlags(Player* player)
{
    SetActiveTierFlags(player, 0, 0);
}

bool ChallengeManager::IsPermadead(Player* player)
{
    if (!player)
        return false;

    if (!IsEnabled())
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

    if (!IsEnabled())
        return false;

    uint32 guid = player->GetGUID().GetCounter();
    return _permadeathPendingKick.find(guid) != _permadeathPendingKick.end();
}

void ChallengeManager::ClearPermadeathPending(uint32 guid)
{
    _permadeathPendingKick.erase(guid);
}

bool ChallengeManager::IsHardcoreGuid(uint32 guid)
{
    auto itr = _activeStates.find(guid);
    if (itr != _activeStates.end())
    {
        if (itr->second.tier == 0)
            return false;
        return (itr->second.flags & FLAG_HARDCORE) != 0;
    }

    ActiveState state = LoadActiveState(guid);
    if (state.tier == 0)
        return false;
    return (state.flags & FLAG_HARDCORE) != 0;
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

    if (!IsEnabled())
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
    if (restrictionId == kRestrictionLowQualityOnly)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.LowQualityOnly"))
            return true;
        return (flags & FLAG_LOW_QUALITY_ONLY) != 0;
    }
    if (restrictionId == kRestrictionSelfCrafted)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.SelfCrafted"))
            return true;
        return (flags & FLAG_SELF_CRAFTED) != 0;
    }
    if (restrictionId == kRestrictionPoverty)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.Poverty"))
            return true;
        return (flags & FLAG_POVERTY) != 0;
    }
    if (restrictionId == kRestrictionNoGuildBank)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoGuildBank"))
            return true;
        return (flags & FLAG_NO_GUILD_BANK) != 0;
    }
    if (restrictionId == kRestrictionNoMounts)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoMounts"))
            return true;
        return (flags & FLAG_NO_MOUNTS) != 0;
    }
    if (restrictionId == kRestrictionNoBuffs)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoBuffs"))
            return true;
        return (flags & FLAG_NO_BUFFS) != 0;
    }
    if (restrictionId == kRestrictionNoTalents)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoTalents"))
            return true;
        return (flags & FLAG_NO_TALENTS) != 0;
    }
    if (restrictionId == kRestrictionNoQuestXP)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoQuestXP"))
            return true;
        return (flags & FLAG_NO_QUEST_XP) != 0;
    }
    if (restrictionId == kRestrictionOnlyQuestXP)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.OnlyQuestXP"))
            return true;
        return (flags & FLAG_ONLY_QUEST_XP) != 0;
    }
    if (restrictionId == kRestrictionHalfXP)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.HalfXP"))
            return true;
        return (flags & FLAG_HALF_XP) != 0;
    }
    if (restrictionId == kRestrictionQuarterXP)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.QuarterXP"))
            return true;
        return (flags & FLAG_QUARTER_XP) != 0;
    }
    if (restrictionId == kRestrictionNoBots)
    {
        if (HasTestAura(player, "ChallengeSystem.TestAura.NoBots"))
            return true;
        return (flags & FLAG_NO_BOTS) != 0;
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

    if (group->isLFGGroup())
    {
        if (HasRestriction(player, kRestrictionSoloOnly) && !ShouldAllowSoloLfg())
            return false;
        if (HasRestriction(player, kRestrictionHardcoreManualGroup) && !ShouldAllowHardcoreLfg())
            return false;

        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member)
                continue;

            if (HasRestriction(member, kRestrictionSoloOnly) && !ShouldAllowSoloLfg())
                return false;
            if (HasRestriction(member, kRestrictionHardcoreManualGroup) && !ShouldAllowHardcoreLfg())
                return false;
        }

        return true;
    }

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
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, GetPermadeathBroadcastMessage(player));
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
