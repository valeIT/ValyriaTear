local ns = {}
setmetatable(ns, {__index = _G})
banesore_layna_village_ai = ns;
setfenv(1, ns);

-- Banesore (in the village battle) has got the following skills:
-- Total SP: almost infinite
-- 1006,   -- Sword attack - 0 SP
-- 10001,  -- Shield - 3 SP
-- 10100,  -- Fire burst - 7 SP
-- 10120   -- Wave - 7 SP

-- Instance local vars
local atk_spell_ratio = 0;

-- The battle_actor parameter is the enemy thinking, useful to exclude itself from other opponents.
function DecideAction(battle_instance, battle_actor)
    local Battle = battle_instance;

    -- Get actors
    local Banesore = battle_actor;
    local Bronann = Battle:GetCharacterActor(0);

    -- Test whether Banesore has attacked once with a spell
    if (Bronann:GetHitPoints() > 80 and atk_spell_ratio > 3) then
        -- Triggers a random spell
        local spell_skill_id = 10100; -- Fire burst
        if (math.random(0, 100) >= 51) then
            spell_skill_id = 10120; -- Wave
        end
        Banesore:SetAction(spell_skill_id, Bronann);
        atk_spell_ratio = 0;
        return;
    end

    -- Test the current Shield spell strength
    if (Banesore:GetActiveStatusEffectIntensity(vt_global.GameGlobal.GLOBAL_STATUS_PHYS_DEF)
            >= vt_global.GameGlobal.GLOBAL_INTENSITY_POS_GREATER
            or Bronann:GetHitPoints() < 60) then

        -- Default attack on the hero
        Banesore:SetAction(1006, Bronann)
    else
        -- Triggers a Shield spell
        Banesore:SetAction(10001, Banesore)
    end

    -- Since no atk spell has been done, increase this value for the next action.
    atk_spell_ratio = atk_spell_ratio + 1

end
