#include "psa/command_table.hpp"

#include "psa/overrides.hpp"

namespace psax {

namespace {
// legacy_name preserves the PSAC / historical display name (used with
// --legacy). method_name is the new dotted-syntax method, rendered as
// "<module_shorthand>.<method_name>(args)" or bare "<method_name>(args)"
// when the top byte has no assigned module (0x00 flow, 0x01 loopRest).
struct NameEntry {
    uint32_t id;
    const char* legacy_name;
    const char* method_name;
    const char* desc;
};

// Curated names for commands we've either verified against PSAC or pulled from
// the PSA Guide v5 reference. Each addition should be checked against PSAC
// where possible before landing. Sort by id for readability.
constexpr NameEntry kNames[] = {
    {
        0x00010100u,
        "SynchronousTimer",
        "synchronousTimer",
        "Pause the current flow of events until the set time is reached. Synchronous timers count down when they are reached in the code."
    },
    {
        0x00020000u,
        "NoEvent",
        "noEvent",
        "No Event."
    },
    {
        0x00020100u,
        "AsynchronousTimer",
        "asynchronousTimer",
        "Pause the current flow of events until the set time is reached. Asynchronous Timers start counting from the beginning of the animation."
    },
    {
        0x00040100u,
        "SetLoop",
        "setLoop",
        "Set a loop for X iterations. The starting point of the loop. Repeat the events from after this Event to before \"Execute Loop\"."
    },
    {
        0x00050000u,
        "ExecuteLoop",
        "executeLoop",
        "Execute the the previously set loop. The ending point of the loop."
    },
    {
        0x00060000u,
        "LoopBreak",
        "loopBreak",
        "Breaks out of all current loop."
    },
    {
        0x00070100u,
        "SubRoutine",
        "subRoutine",
        "Enter the event routine specified and return after ending."
    },
    {
        0x00090100u,
        "Goto",
        "goto",
        "Goto the event location specified and execute (does not return afterwards)."
    },
    {
        0x00080000u,
        "Return",
        "return",
        "Return from a Subroutine."
    },
    {
        0x000A0100u,
        "If",
        "if",
        "Start an If block until an Else or an End If is reached. Use this If to check for a requirement. Read the contents of the If block only if the requirements are met."
    },
    {
        0x000A0200u,
        "IfValue",
        "if",
        "Start an If block until an Else or an End If is reached. Use this If to check for a requirement with a specified value. Read the contents of the If block only if the requirements are met."
    },
    {
        0x000A0400u,
        "IfComparison",
        "if",
        "Start an If block until an Else or an End If is reached. Use this If to compare values. Read the contents of the If block only if the requirements are met."
    },
    {
        0x000B0100u,
        "And",
        "and",
        "Insert an And statement to an If statement. Use this And to check for a requirement. Read the contents of the If block only if all requirements are met."
    },
    {
        0x000B0200u,
        "AndValue",
        "and",
        "Insert an And statement to an If statement. Use this And to check for a requirement with a specified value. Read the contents of the If block only if all requirements are met."
    },
    {
        0x000B0400u,
        "AndComparison",
        "and",
        "Insert an And statement to an If statement. Use this And to compare values. Read the contents of the If block only if all requirements are met."
    },
    {
        0x000C0100u,
        "Or",
        "or",
        "Insert an Or statement to an If statement. Use this Or to check for a requirement. Read the contents of the If block only if any of the requirements are met."
    },
    {
        0x000C0200u,
        "OrValue",
        "or",
        "Insert an Or statement to an If statement. Use this Or to check for a requirement with a specified value. Read the contents of the If block only if any of the requirements are met."
    },
    {
        0x000C0400u,
        "OrComparison",
        "or",
        "Insert an Or statement to an If statement. Use this Or to compare values. Read the contents of the If block only if any of the requirements are met."
    },
    {
        0x000D0100u,
        "ElseIf",
        "elseIf",
        "Insert an Else If block inside an If block. Use this Else If to check for a requirement. Reads the contents of the Else If block only if all If series requirements have not been met before this event in the current If block, and this requirement is met."
    },
    {
        0x000D0200u,
        "ElseIfValue",
        "elseIf",
        "Insert an Else If block inside an If block. Use this Else If to check for a requirement with a specified value. Reads the contents of the Else If block only if all If series requirements have not been met before this event in the current If block, and this requirement is met."
    },
    {
        0x000D0400u,
        "ElseIfComparison",
        "elseIf",
        "Insert an Else If block inside an If block. Use this Else If to compare values. Reads the contents of the Else If block only if all If series requirements have not been met before this event in the current If block, and this requirement is met."
    },
    {
        0x000E0000u,
        "Else",
        "else",
        "Reads the contents of the Else block only if the requirements of all If series up to this event within the current If block have not been met."
    },
    {
        0x000F0000u,
        "EndIf",
        "endIf",
        "End an If block."
    },
    {
        0x00100200u,
        "Switch",
        "switch",
        "Begin a multiple case Switch block."
    },
    {
        0x00110100u,
        "Case",
        "case",
        "Handler for if the variable in the switch block equals the specified value."
    },
    {
        0x00120000u,
        "DefaultCase",
        "defaultCase",
        "The case chosen if none of the others are executed in a switch block."
    },
    {
        0x00130000u,
        "EndSwitch",
        "endSwitch",
        "End a Switch block."
    },
    {
        0x00180000u,
        "Break",
        "break",
        "Appears within Case statements; exits the switch event completely. All code located in the same case block after this event will not be executed."
    },
    {
        0x02010200u,
        "ChangeAction",
        "changeAction",
        "Change the current action upon the specified requirement being met. Even if the requirements are not met when this Event is loaded, the requirements are met before changing to another Action, the Action will change at that moment. This Change Action is based on a requirement being met."
    },
    {
        0x02010300u,
        "ChangeActionValue",
        "changeActionValue",
        "Change the current action upon the specified requirement being met. Even if the requirements are not met when this Event is loaded, the requirements are met before changing to another Action, the Action will change at that moment. This Change Action is based on a requirement with a specified value being met."
    },
    {
        0x02010500u,
        "ChangeActionComparison",
        "changeActionComparison",
        "Change the current action upon the specified requirement being met. Even if the requirements are not met when this Event is loaded, the requirements are met before changing to another Action, the Action will change at that moment. This Change Action is based on a comparison."
    },
    {
        0x02000300u,
        "ChangeActionStatus",
        "changeActionStatus",
        "Change the current action upon the specified requirement being met. Even if the requirements are not met when this Event is loaded, the requirements are met before changing to another Action, the Action will change at that moment. Status ID is used when such as disabling the Change Action itself by \"Disable Action Status ID\". This Change Action Status is based on a requirement being met."
    },
    {
        0x02000400u,
        "ChangeActionStatusValue",
        "changeActionStatusValue",
        "Change the current action upon the specified requirement being met. Even if the requirements are not met when this Event is loaded, the requirements are met before changing to another Action, the Action will change at that moment. Status ID is used when such as disabling the Change Action itself by \"Disable Action Status ID\". This Change Action Status is based on a requirement with a specified value being met."
    },
    {
        0x02000600u,
        "ChangeActionStatusComparison",
        "changeActionStatusComparison",
        "Change the current action upon the specified requirement being met. Even if the requirements are not met when this Event is loaded, the requirements are met before changing to another Action, the Action will change at that moment. Status ID is used when such as disabling the Change Action itself by \"Disable Action Status ID\". This Change Action Status is based on a comparison."
    },
    {
        0x02040100u,
        "AdditionalRequirement",
        "additionalRequirement",
        "Add an additional requirement to the preceding Change Action statement. This Additional Action Requirement is based on a requirement being met."
    },
    {
        0x02040200u,
        "AdditionalRequirementValue",
        "additionalRequirementValue",
        "Add an additional requirement to the preceding Change Action statement. This Additional Action Requirement is based on a requirement with a specified value being met."
    },
    {
        0x02040400u,
        "AdditionalRequirementComparison",
        "additionalRequirementComparison",
        "Add an additional requirement to the preceding Change Action statement. This Additional Action Requirement is based on a comparison."
    },
    {
        0x02080100u,
        "DisableActionStatusId",
        "disableActionStatusId",
        "Disables the \"Change Action Status\" associated with the given Status ID. To re-enable, load \"Enable Action Status ID\"."
    },
    {
        0x02060100u,
        "EnableActionStatusId",
        "enableActionStatusId",
        "Enables the \"Change Action Status\" associated with the given Status ID."
    },
    {
        0x64000000u,
        "AllowInterrupt",
        "allowInterrupt",
        "Allow the current action to be interrupted by another action."
    },
    {
        0x020A0100u,
        "AllowSpecificInterrupt",
        "allowSpecificInterrupt",
        "Allows interruption only by specific commands. See parameters for list of possible interrupts."
    },
    {
        0x02090200u,
        "InvertActionStatusId",
        "invertActionStatusId",
        "Appears to invert (or possibly only disable) a specific Status ID's enabled/disabled status. For example, if a character can crawl, this is used to disable the ability to dash when crouched, even though naturally crouching allows dashing through 020A (Allow Specific Interrupt)."
    },
    {
        0x64010000u,
        "DisallowInterrupt",
        "disallowInterrupt",
        "Disable all interrupts on the current action."
    },
    {
        0x020B0100u,
        "PreventSpecificInterrupt",
        "preventSpecificInterrupt",
        "Disable the specific interruption window. Must be set to the same thing as the allow specific interrupt that you wish to cancel."
    },
    {
        0x04000100u,
        "ChangeSubAction",
        "changeSubAction",
        "Change the current sub action."
    },
    {
        0x04000200u,
        "ChangeSubActionFrame",
        "changeSubAction",
        "Change the current sub action. Can specify whether or not to pass the current frame or start the animation over."
    },
    {
        0x04060100u,
        "SetAnimationFrame",
        "setAnimationFrame",
        "Changes the current frame of the animation. Does not change the frame of the sub action (i.e. timers and such are unaffected)."
    },
    {
        0x04070100u,
        "FrameSpeedModifier",
        "frameSpeedModifier",
        "Change the frame speed of the Sub Action. Example: Setting to 2 makes the animation and timers occur twice as fast."
    },
    {
        0x04140100u,
        "SetAnimationAndTimerFrame",
        "setAnimationAndTimerFrame",
        "Changes the current frame of the animation and timers. Unlike \"Set Animation Frame\", it affects the timer within the Sub Action. For this reason, can create an infinite loop by this Event within the Sub Action."
    },
    {
        0x040C0100u,
        "ChangeSubActionReversePlay",
        "changeSubActionReversePlay",
        "Change the current sub action (plays in reverse). The reading order of the Events is normal. Not in reverse order."
    },
    {
        0x04080300u,
        "SetBoneMotionOverride",
        "setBoneMotionOverride",
        "Overrides all motions following the specified bone by the motion of the specified sub action. Normally, this effect lasts until the end event \"End Bone Motion Override\" is loaded (It may be ended by getting, using or discarding of the item). It also loads the event of the sub action specified by this event. So, it can end this effect from the specified subaction. Of course, can also load different motion here, but in this case the event in the subaction cannot be loaded."
    },
    {
        0x040D0100u,
        "EndBoneMotionOverride",
        "endBoneMotionOverride",
        "End \"Set Bone Motion Override\". Nothing happens if the Command ID is different from the set value."
    },
    {
        0x040E0200u,
        "SetBoneAnimationFrame",
        "setBoneAnimationFrame",
        "Changes the current frame of the \"Set Bone Motion Override\" animation. Does not change the frame of the sub action. Nothing happens if the Command ID is different from the set value."
    },
    {
        0x040F0200u,
        "BoneAnimationFrameSpeedModifier",
        "boneAnimationFrameSpeedModifier",
        "Change the frame speed of the \"Set Bone Motion Override\" Sub Action. Nothing happens if the Command ID is different from the set value. Example: Setting to 2 makes the animation and timers occur twice as fast."
    },
    {
        0x04150200u,
        "SetBoneAnimationAndTimerFrame",
        "setBoneAnimationAndTimerFrame",
        "Changes the current frame of the \"Set Bone Motion Override\" animation and timers. Nothing happens if the Command ID is different from the set value. Unlike \"Set Bone Animation Frame\", it affects the timer within the Sub Action."
    },
    {
        0x04100200u,
        "SetTextureAnimationOverride",
        "setTextureAnimationOverride",
        "Override animations other than CHR by the specified Sub Action. (Such as eye movements animation. Target: SRT, SHP, PAT, VIS etc.) This effect remains until a new Sub Action with movement other than CHR is loaded or \"End Texture Animation Override\" is loaded. Unlike \"Set Bone Motion Override\", Events in the specified Sub Action by this event is not loaded."
    },
    {
        0x04110100u,
        "EndTextureAnimationOverride",
        "endTextureAnimationOverride",
        "End \"Set Texture Animation Override\". This effect is valid even if the command ID is different from the set value. Even if end \"Set Texture Animation Override\" by this Event, the animation other than CHR that were played before not return to their original."
    },
    {
        0x04120200u,
        "SetTextureAnimationFrame",
        "setTextureAnimationFrame",
        "Changes the current frame of the \"Set Texture Animation Override\" animation. This effect is valid even if the command ID is different from the set value."
    },
    {
        0x04130200u,
        "TextureAnimationFrameSpeedModifier",
        "textureAnimationFrameSpeedModifier",
        "Change the speed of the \"Set Bone Motion Override\" animation. This effect is valid even if the command ID is different from the set value."
    },
    {
        0x01010000u,
        "LoadRest",
        "loadRest",
        "Stops loading subsequent events until any of the requirements set in \"Set Requirement\" are met. If no \"Set Requirement\" Events have been loaded, subsequent events cannot be loaded. When loaded in Sub Action, the Event after this cannot be read even if the condition is met."
    },
    {
        0x04020100u,
        "SetRequirement",
        "setLoopResumeCondition",
        "Set requirement for reading \"Load Rest\" or later. If multiple \"Set Requirement\" are loaded, if one of them is achieved, load events that exist after \"Load Rest\"."
    },
    {
        0x04020200u,
        "SetRequirementValue",
        "setLoopResumeCondition",
        "Set requirement for reading \"Load Rest\" or later. If multiple \"Set Requirement\" are loaded, if one of them is achieved, load events that exist after \"Load Rest\"."
    },
    {
        0x04020400u,
        "SetRequirementComparison",
        "setLoopResumeCondition",
        "Set requirement for reading \"Load Rest\" or later. If multiple \"Set Requirement\" are loaded, if one of them is achieved, load events that exist after \"Load Rest\"."
    },
    {
        0x04030100u,
        "SetExtraRequirement",
        "additionalLoopResumeCondition",
        "Add an additional requirement to the preceding \"Set Requirement\". When all the requirements set in these are met, load events that exist after \"Load Rest\"."
    },
    {
        0x04030200u,
        "SetExtraRequirementValue",
        "additionalLoopResumeCondition",
        "Add an additional requirement to the preceding \"Set Requirement\". When all the requirements set in these are met, load events that exist after \"Load Rest\"."
    },
    {
        0x04030400u,
        "SetExtraRequirementComparison",
        "additionalLoopResumeCondition",
        "Add an additional requirement to the preceding \"Set Requirement\". When all the requirements set in these are met, load events that exist after \"Load Rest\"."
    },
    {
        0x04010200u,
        "SetRequirementStatus",
        "setRequirementStatus",
        "Set requirement for reading \"Load Rest\" or later. This seems to be able to be disabled by \"Disable Set Requirement Status ID\". However, it seems to be a little unstable, so it is recommended to use \"Set Requirement\"."
    },
    {
        0x04050100u,
        "DisableSetRequirementStatusId",
        "disableSetRequirementStatusId",
        "Disables the \"Set Requirement Status\" associated with the given Status ID. To re-enable, load \"Enable Set Requirement Status ID\"."
    },
    {
        0x04040100u,
        "EnableSetRequirementStatusId",
        "enableSetRequirementStatusId",
        "Enables the \"Set Requirement Status\" associated with the given Status ID."
    },
    {
        0x06000D00u,
        "OffensiveCollision",
        "offensiveCollision",
        "Generate an offensive collision bubble with the specified parameters. If a hitbox exists, can set multiple hitboxes by setting a different ID from the existing one. Also, if set the same ID as an existing one, it will be replaced with the specified parameters loaded later."
    },
    {
        0x06150F00u,
        "SpecialOffensiveCollision",
        "specialOffensiveCollision",
        "Generate an offensive collision bubble with the specified parameters. Can generate more special collisions than \"Offensive Collision\". If a hitbox exists, can set multiple hitboxes by setting a different ID from the existing one. Also, if set the same ID as an existing one, it will be replaced with the specified parameters loaded later."
    },
    {
        0x06010200u,
        "ChangeHitboxDamage",
        "changeHitboxDamage",
        "Changes an existing hitbox damage to the new value. Only guaranteed to work on Offensive Collisions."
    },
    {
        0x06140200u,
        "AddHitboxDamage",
        "addHitboxDamage",
        "Adds an existing hitbox damage."
    },
    {
        0x06020200u,
        "ChangeHitboxSize",
        "changeHitboxSize",
        "Changes an existing hitbox size to the new Scale. Only guaranteed to work on Offensive Collisions."
    },
    {
        0x061B0500u,
        "MoveHitbox",
        "moveHitbox",
        "Repositions an existing hitbox."
    },
    {
        0x062F0200u,
        "ChangeHitboxHitBit",
        "changeHitboxHitBit",
        "Changes an existing hitbox Hit Bit to the new Flags."
    },
    {
        0x06030100u,
        "DeleteHitbox",
        "deleteHitbox",
        "Deletes only one specified hitbox among existing hitboxes. Only guaranteed to work on Offensive Collisions."
    },
    {
        0x06040000u,
        "TerminateCollisions",
        "terminateCollisions",
        "Remove all currently existing hitboxes. Also, if regenerate the offensive collision bubble after loading this, the hitbox will be hit again."
    },
    {
        0x060A0800u,
        "CatchCollision",
        "catchCollision",
        "Generate a grabbing collision bubble with the specified parameters."
    },
    {
        0x060B0200u,
        "ChangeCatchCollisionSize",
        "changeCatchCollisionSize",
        "Changes an existing grab collision size to the new Scale."
    },
    {
        0x060C0100u,
        "DeleteCatchCollision",
        "deleteCatchCollision",
        "Deletes only one specified grab collision among existing grab collisions."
    },
    {
        0x060D0000u,
        "TerminateCatchCollisions",
        "terminateCatchCollisions",
        "Remove all currently existing grab collision bubbles."
    },
    {
        0x060E1100u,
        "ThrowAttackCollision",
        "throwAttackCollision",
        "Specify the properties of the throw to be used when \"Throw Collision\" (060F0500) is executed. Used for other things as well, such as some Final Smashes."
    },
    {
        0x060F0500u,
        "ThrowCollision",
        "throwCollision",
        "Throws an opponent based on data provided by \"Throw Attack Collision\" (060E1100)."
    },
    {
        0x06050100u,
        "BodyCollision",
        "bodyCollision",
        "Change how the character's own collision bubbles act."
    },
    {
        0x06080200u,
        "BoneCollision",
        "boneCollision",
        "Changes body collision type for a specific bone. However, can't be set to Invincible."
    },
    {
        0x06070200u,
        "HurtBoxCollision",
        "hurtBoxCollision",
        "Changes body collision type for a specific HurtBox. However, can't be set to Invincible."
    },
    {
        0x06060100u,
        "UndoBoneCollision",
        "undoBoneCollision",
        "Reset all specific bones collision. Can set to be intangible state by this Event too. If set intangible state by this Event, the character will not flashing."
    },
    {
        0x1E000200u,
        "SuperHeavyArmor",
        "superHeavyArmor",
        "Starts super armor or heavy armor. Set both parameters to 0 to end the armor. Can't prevent sleep and flinchless knockback."
    },
    {
        0x1E010100u,
        "PreventDamage",
        "preventDamage",
        "When set to True, prevents increased damage when take a attacked. Can't prevent Flower damage and knockback."
    },
    {
        0x1E020100u,
        "SetDamage",
        "setDamage",
        "Set current damage to the specified value. Damage display does not change."
    },
    {
        0x1E030100u,
        "AddSubtractDamage",
        "addSubtractDamage",
        "Adds or subtracts the specified amount of damage from the character's current percentage. + Values = Damage and - Values = Recover."
    },
    {
        0x1E040100u,
        "SubtractDamage",
        "subtractDamage",
        "Adds or subtracts the specified amount of damage from the character's current percentage. + Values = Recover and - Values = Damage. If set the value to IC-Basic[2], the damage will be 0."
    },
    {
        0x06170300u,
        "DefensiveCollision",
        "defensiveCollision",
        "Generate a defensive collision bubble. This event can't set such as size. Only type can be set."
    },
    {
        0x06180300u,
        "DeleteDefensiveCollision",
        "deleteDefensiveCollision",
        "Removes the specified defensive collisions."
    },
    {
        0x06241000u,
        "GenerateDefensiveCollisionBubble",
        "generateDefensiveCollisionBubble",
        "Generates a custom Defensive Collision bubble. It used nativey by Subspace enemies, but it can be used by Fighters. However, use by the Fighter will be used by rewriting the ability of effects such as Franklin Badge, so overwrite by the original status is required to work around it."
    },
    {
        0x06200A00u,
        "ChangeDefensiveCollisionScale",
        "changeDefensiveCollisionScale",
        "Generates a custom Defensive Collision bubble. (Can change only Offset and Size) It used nativey by Subspace enemies, but it can be used by Fighters. However, use by the Fighter will be used by rewriting the ability of effects such as Franklin Badge, so overwrite by the original status is required to work around it."
    },
    {
        0x061E0300u,
        "DefensiveCollisionProperty",
        "defensiveCollisionProperty",
        "Modify a property of defensive collision."
    },
    {
        0x06160100u,
        "HitboxDamageMultiplier",
        "hitboxDamageMultiplier",
        "Multiplies the damage of every hitbox spawned from the character by the specified value. Persists until changed."
    },
    {
        0x06101100u,
        "InertCollision",
        "inertCollision",
        "Generates a collision only used to detect with other characters, object such as items etc."
    },
    {
        0x06110200u,
        "ChangeInertCollisionSize",
        "changeInertCollisionSize",
        "Changes existing inert collision size to the new Scale."
    },
    {
        0x06120100u,
        "DeleteInertCollision",
        "deleteInertCollision",
        "Deletes existing inert collision. This effect is valid even if the ID is different from the set value."
    },
    {
        0x06130000u,
        "TerminateInertCollision",
        "terminateInertCollision",
        "Remove existing inert collision."
    },
    {
        0x06091E00u,
        "OffensiveCollisionAddScaleSimpleEnemyHitbox",
        "offensiveCollisionAddScaleSimpleEnemyHitbox",
        "Generate an offensive collision bubble similar to \"Offensive Collision\". It used nativey by Subspace enemies, but it can be used by Fighters. The difference from \"Offensive Collision\" is that there are many parameters, with damage, knockback, and size each having additional values. However, the main reason for having many parameters, Flags is not summarized into 1 parameter."
    },
    {
        0x06192F00u,
        "SpecialOffensiveCollisionAddScaleEnemyHitbox",
        "specialOffensiveCollisionAddScaleEnemyHitbox",
        "Generate an offensive collision bubble similar to \"Special Offensive Collision\". It used nativey by Subspace enemies, but it can be used by Fighters. The difference from \"Special Offensive Collision\" is that there are many parameters, with damage, knockback, and size each having additional values. However, the main reason for having many parameters, Flags is not summarized into 2 parameters."
    },
    {
        0x06220100u,
        "DisableOffensiveCollision",
        "disableOffensiveCollision",
        "Configure the effectiveness of hitbox. If set to False, all generated offensive collision bubble will not work. To undo, change the Action or load this event again and set to True."
    },
    {
        0x06250100u,
        "TakeTeammatesAttacks",
        "takeTeammatesAttacks",
        "If set to True, the character or object will be hit by attacks from all allies too. All allies here include projectiles and items released by oneself. However, it does not allow the character to attack allies, or block, reflect, or absorb attacks from allies. (It is possible to avoid attacks by Intangible state) To undo, change the Action or load this event again and set to False. If the character is grabbed by an ally, hitboxes with Team Damager flag set to false will not hit, but will be taken by a throw attack that depends on the Throw Attack Collision parameters. It was originally only used to allow his own attacks to destroy the mine placed by the Snake's Down Smash."
    },
    {
        0x062D0000u,
        "ResetHitboxHasConnected",
        "resetHitboxHasConnected",
        "If the Requirement \"Hitbox has Connected\" is True, set it to False. It has no effect on \"Hitbox Connects\" etc."
    },
    {
        0x0C1C0200u,
        "SetAttackId",
        "setAttackId",
        "Set the Attack ID. There is no relation to hitbox ID. Increases the Attack Count when no attack ID is set or when the current attack ID is changed. And, affects stale-move negation. Only if set the event ID to \"0C1C0300\" and set the increased parameter to True, even if load the same Attack ID as the one currently loaded, it will affect the stale-move negation and attack count in the same way as when changing it."
    },
    {
        0x062B0D00u,
        "ThrownCollision",
        "thrownCollision",
        "Generates a damage collision bubble surrounding the character being thrown."
    },
    {
        0x062C0F00u,
        "SpecialCollateralCollision",
        "specialCollateralCollision",
        "Used for the \"bump\" collisions that occur when a character in knockback collides with another body."
    },
    {
        0x05000000u,
        "ReverseDirection",
        "reverseDirection",
        "Reverse the direction of the character. The effect occurs when the current Action is changed. To change the direction immediately, requires load \"Decide Direction\" Event after loading this Event."
    },
    {
        0x05010000u,
        "LeftDirection",
        "leftDirection",
        "Set the character direction to the left. The effect occurs when the current Action is changed. To change the direction immediately, requires load \"Decide Direction\" Event after loading this Event."
    },
    {
        0x05020000u,
        "RightDirection",
        "rightDirection",
        "Set the character direction to the right. The effect occurs when the current Action is changed. To change the direction immediately, requires load \"Decide Direction\" Event after loading this Event."
    },
    {
        0x05030000u,
        "SetDirection",
        "setDirection",
        "Set the character direction according to control stick X axis position. The effect occurs when the current Action is changed. To change the direction immediately, requires load \"Decide Direction\" Event after loading this Event."
    },
    {
        0x05040000u,
        "DecideDirection",
        "decideDirection",
        "Immediately change the direction of the character in the direction set by \"Set Direction\" etc."
    },
    {
        0x050C0000u,
        "ReverseModelDirectionTransient",
        "reverseModelDirectionTransient",
        "Reverse Model Direction. This effect ends when current action is changed."
    },
    {
        0x05050100u,
        "ChangeModelSize",
        "changeModelSize",
        "Change the character size. Resizing by this event does not change the ability or damage done. Also, when get an item that changes size such as mushrooms, it will change from the base size. (In other words, the size changed by this event will be invalid.)"
    },
    {
        0x05060300u,
        "RotateCharacterModel",
        "rotateCharacterModel",
        "Rotates character's model by amount specified. Persists until changed."
    },
    {
        0x08000100u,
        "SetEdgeSlide",
        "setEdgeSlide",
        "Determines whether or not the character will slide off the edge. (Set Aerial/Onstage State)"
    },
    {
        0x0E000100u,
        "SetAirGround",
        "setAirGround",
        "Sets the current physics state. (Set Kinetic State)"
    },
    {
        0x0E080400u,
        "SetAddMomentum",
        "setAddMomentum",
        "Controls the character's current momentum. It can set whether to add or override the specified parameters to the current speed. (Note: Can't use variables as parameters.)"
    },
    {
        0x0E010200u,
        "AddSubtractMomentum",
        "addSubtractMomentum",
        "Adds or subtracts speed to the character's current momentum."
    },
    {
        0x0E020100u,
        "ResetCertainMomentum",
        "resetCertainMomentum",
        "When set to 1, reset vertical momentum. When set to 2, reset horizontal momentum."
    },
    {
        0x0E060100u,
        "DisallowCertainMovements",
        "disallowCertainMovements",
        "When set to 1, disables vertical gravity. When set to 2, horizontal acceleration and deceleration are disallowed. The speed will not be 0 and will continue to move at the speed it was loaded until it is re-enabled."
    },
    {
        0x0E070100u,
        "ReallowCertainMovements",
        "reallowCertainMovements",
        "This must be set to the same value as Disallow Certain Movements to work. (1 = vertical speed, 2 = horizontal speed)"
    },
    {
        0x0E020000u,
        "ResetMomentum",
        "resetMomentum",
        "Reset vertical and horizontal momentum."
    },
    {
        0x0E080200u,
        "SetMomentum",
        "setMomentum",
        "Controls the character's current momentum. It is only possible override the current speed. (Note: Can't use variables as parameters.)"
    },
    {
        0x0E050100u,
        "DisableSpecifiedMovement",
        "disableSpecifiedMovement",
        "Temporarily disable certain movements related to speed."
    },
    {
        0x0E040100u,
        "EnableSpecifiedMovement",
        "enableSpecifiedMovement",
        "Re-enable certain movements related to speed. In other words, Disable the effect of previously loaded \"Disable Specified Movement\". The value same as \"Disable Specified Movement\"."
    },
    {
        0x0C040000u,
        "SetAerialState",
        "setAerialState",
        "Move to aerial state when on the ground."
    },
    {
        0x05090300u,
        "TeleportationStageBased",
        "teleportationStageBased",
        "Teleport to the specified position based on the stage. Normally, unaffected by terrain. However, usually cannot move normally during ground actions. (Even during ground action, if the specified position exceeds the blast line, the character will self-destruct)"
    },
    {
        0x050A0300u,
        "TeleportationStageBased2",
        "teleportationStageBased2",
        "Teleport to the specified position based on the stage. If there are walls, floors, or ceilings before to the specified position, There is a case that stops at that place. And, usually cannot move normally during ground actions. (Even during ground action, if the specified position exceeds the blast line, the character will self-destruct)"
    },
    {
        0x050B0300u,
        "TeleportationCharacterBased",
        "teleportationCharacterBased",
        "Teleport to the specified position based on the character position. If there are walls, floors, or ceilings before to the specified position, There is a case that stops at that place. And, usually cannot move normally during ground actions. (Even during ground action, if the specified position exceeds the blast line, the character will self-destruct)"
    },
    {
        0x0C090100u,
        "AllowDisallowLedgegrab",
        "allowDisallowLedgegrab",
        "Allow or disallow grabbing ledges during the current action. 0 is Cannot, 1 is Only in front, 2 or 4 is Always."
    },
    {
        0x09000100u,
        "Module09_00",
        "module09_00",
        "Setting of Air/ground. 0 is on ground. (When in air, Character is landing) 2 is in air. (When on ground, Character is fall) Setting this to 0 or 1 while in the air, jump count is reset. And, when over upper boundary, character will self-destruct."
    },
    {
        0x08070000u,
        "ThroughPassableFloor",
        "throughPassableFloor",
        "When on a passable floor, pass through that floor."
    },
    {
        0x0A000100u,
        "SoundEffect",
        "soundEffect",
        "Play a specified sound effect."
    },
    {
        0x0A010100u,
        "SteppingSoundEffect",
        "steppingSoundEffect",
        "Play a specified sound effect plus a sound of stepping on the current terrain."
    },
    {
        0x0A030100u,
        "StopSoundEffect",
        "stopSoundEffect",
        "Stops the specified sound effect immediately. The sound effect occured by \"Sound Effect (Transient)\" can't be stopped by this Event, but can stop them all at once \"Stop Transient Sound Effect\"."
    },
    {
        0x0A020100u,
        "SoundEffectTransient",
        "soundEffectTransient",
        "Play a specified sound effect. The sound effect ends with the action."
    },
    {
        0x0A060000u,
        "StopTransientSoundEffect",
        "stopTransientSoundEffect",
        "Stops all sound effects occured by \"Sound Effect (Transient)\"."
    },
    {
        0x0A070100u,
        "SoundEffectOccursWhenLanding",
        "soundEffectOccursWhenLanding",
        "Play a specified sound effect. sound effect occurs when landing. However, it has no effect by most Articles. Also, can call it by \"Call Setted Sound Effect\"."
    },
    {
        0x0A080000u,
        "CallSettedSoundEffect",
        "callSettedSoundEffect",
        "Plays the sound effect setted by \"Sound Effect (Occur Landing)\"."
    },
    {
        0x0A050100u,
        "VictorySoundEffect",
        "victorySoundEffect",
        "Appears to play a sound effect. Used during victories as well as other places like taunts."
    },
    {
        0x0A090100u,
        "LandingSoundEffect",
        "landingSoundEffect",
        "Play a specified sound effect plus a sound of landing on the current terrain."
    },
    {
        0x0A0A0100u,
        "ImpactSoundEffect",
        "impactSoundEffect",
        "Play a specified sound effect plus a sound of Impact on the current terrain."
    },
    {
        0x0C0B0000u,
        "LowVoiceClip",
        "lowVoiceClip",
        "Play voice clip selected from low voice clips randomly. (Sound List 2, 0-3)"
    },
    {
        0x0C190000u,
        "DamageVoiceClip",
        "damageVoiceClip",
        "Play voice clip selected from damage voice clips randomly. (Sound List 0, 0-1; Sound List 1, 0-1)"
    },
    {
        0x0C1D0000u,
        "OttottoVoiceClip",
        "ottottoVoiceClip",
        "Plays the voice clip for ottotto (ledge teeter). PM appears to repurpose this for an extra set of random voice clips."
    },
    {
        0x0C1E0000u,
        "VoiceClip1E",
        "voiceClip1E",
        "It is used to play voice clip for Attack or Ottotto."
    },
    {
        0x0C1F0000u,
        "EatingVoiceClip",
        "eatingVoiceClip",
        "Play voice clip selected from eating voice clips randomly."
    },
    {
        0x0C1A0200u,
        "ShootingItemSoundEffect",
        "shootingItemSoundEffect",
        "If holding item has a bullet, the sound effect of parameter 0 is played, and if there is no bullet, the sound effect of parameter 1 is played. If not hold a item, nothing happens."
    },
    {
        0x10000100u,
        "GenerateArticle",
        "generateArticle",
        "Generates a character specific Article: a pre-made prop effect from the prop library."
    },
    {
        0x10000200u,
        "GenerateArticleActionExclusive",
        "generateArticleActionExclusive",
        "Generates a character specific Article: a pre-made prop effect from the prop library. Has option to Article to terminate with Action ends."
    },
    {
        0x10140100u,
        "GenerateAvailableArticle",
        "generateAvailableArticle",
        "Generates a character specific Article: a pre-made prop effect from the prop library. Article cannot be generated if the maximum number already exists."
    },
    {
        0x10030100u,
        "RemoveArticle",
        "removeArticle",
        "Remove all currently existing Articles with the same Article ID as the specified article. However, some articles can't be removed. There are some Articles that can't be removed if after detached."
    },
    {
        0x10010100u,
        "SetDetachArticle",
        "event02",
        "Release or detach currently existing Articles with the same Article ID as the specified article. However, it basically has no effect on Articles that have already been released or cannot be released or detached. (BrawlCrate labels this 'Article Event 02' — its exact semantics are not fully understood.)"
    },
    {
        0x10010200u,
        "SetDetachArticle02",
        "setDetachArticle02",
        "Release or detach currently existing Articles with the same Article ID as the specified article. However, it basically has no effect on Articles that have already been released or cannot be released or detached."
    },
    {
        0x10090100u,
        "SetDetachArticle09",
        "setDetachArticle09",
        "Release or detach currently existing Articles with the same Article ID as the specified article. It seems to be the same as \"Set Detach Article\"."
    },
    {
        0x10050200u,
        "ArticleVisibility",
        "visibility",
        "Makes an Article visible or invisible. Similar to \"Remove Article\", there are some case that do nothing even if loads this event."
    },
    {
        0x100C0200u,
        "ArticleFrameSpeed",
        "frameSpeed",
        "Change the animation speed of the Article. Similar to \"Remove Article\", there are some case that do nothing even if loads this event."
    },
    {
        0x10040200u,
        "SetArticleSubAction",
        "setArticleAction",
        "Change the Sub Action of the Article. Similar to \"Remove Article\", there are some case that do nothing even if loads this event."
    },
    {
        0x10040300u,
        "SetAnchoredArticleSubAction",
        "setAnchoredArticleSubAction",
        "Change the Sub Action of the Article. Similar to \"Remove Article\", there are some case that do nothing even if loads this event."
    },
    {
        0x10070200u,
        "SetArticleAction07",
        "setArticleAction07",
        "Change the Action of the Article. Similar to \"Remove Article\", there are some case that do nothing even if loads this event."
    },
    {
        0x10080200u,
        "SetRemoteArticleAction",
        "setRemoteArticleAction",
        "Change the Action of the Article. Similar to \"Remove Article\", there are some case that do nothing even if loads this event."
    },
    {
        0x10020100u,
        "RemoveArticle02",
        "removeArticle02",
        "Remove all currently existing Articles with the same Article ID as the specified article. It seems to be the same as \"Set Detach Article\"."
    },
    {
        0x100A0200u,
        "LinkArticleToBone",
        "linkArticleToBone",
        "Attach Article to specified bone. Used in Snake's taunts and victory to attach the cardboard box to bone. Similar to \"Remove Article\", there are some case that do nothing even if loads this event."
    },
    {
        0x100A0300u,
        "LinkArticleToBone03",
        "linkArticleToBone03",
        "Attach Article to specified bone. Used in Snake's Neutral B to attach the grenade to his hand. Similar to \"Remove Article\", there are some case that do nothing even if loads this event."
    },
    {
        0x10130100u,
        "LinkCharacterAndArticle",
        "linkCharacterAndArticle",
        "Seems to be used whenever a detached article needs to change its action."
    },
    {
        0x0C050000u,
        "TerminateInstance",
        "terminateInstance",
        "If used within an article, causes the acting article instance to terminate (if possible). Has other niche uses as well, such as loading secondary instance if available (i.e. character transformation)."
    },
    {
        0x0C080000u,
        "TerminateSelf",
        "terminateSelf",
        "Used by certain article instances to remove themselves instead of \"Terminate Instance\". (note: nearly all projectiles in the game will terminate fine with the terminate instance command)"
    },
    {
        0x0C090000u,
        "DetachSelf",
        "detachSelf",
        "If used within an Article, causes the Article detach from the character. If load this Event in Article, will be unable to confirm its existence with \"Article Exists\" from the character, or remove it by loading \"Remove Article\"."
    },
    {
        0x11001000u,
        "GraphicEffect",
        "graphicEffect",
        "Generate a graphical effect. Stays at the spot it was spawned."
    },
    {
        0x11010A00u,
        "GraphicEffectAttached",
        "attached",
        "Generate a graphical effect that moves with the bone."
    },
    {
        0x11020A00u,
        "GraphicEffectAttached2",
        "attachedJointRelative",
        "Generate a graphical effect that moves with the bone. The rotation and size don't move from the set values. In other words, it will not rotate or change size with the bone."
    },
    {
        0x111A1000u,
        "GraphicEffectStepping",
        "stepping",
        "Generate a graphical effect. Stays at the spot it was spawned. When in shallow water, it switches to a graphic effect of water ripples."
    },
    {
        0x111B1000u,
        "GraphicEffectLanding",
        "landing",
        "Generate a graphical effect. Stays at the spot it was spawned. When on ice or shallow water, it switches to a landing graphical effect."
    },
    {
        0x111C1000u,
        "GraphicEffectTumbling",
        "tumbling",
        "Generate a graphical effect. Stays at the spot it was spawned. When on ice or shallow water, it switches to a landing graphical effect. This is only used when falling lying down."
    },
    {
        0x11190A00u,
        "GraphicEffectAttached19",
        "attached19",
        "Generate a graphical effect that moves with the bone. Even if Hitlag occurs, the graphical effect doesn't stop moving."
    },
    {
        0x110E1200u,
        "GraphicEffectAddScaleDetached0E",
        "addScaleDetached0E",
        "Generate a graphical effect. Stays at the spot it was spawned. Additional values can be set for size."
    },
    {
        0x11100C00u,
        "GraphicEffectAddScaleAttached10",
        "addScaleAttached10",
        "Generate a graphical effect that moves with the bone. Additional values can be set for size."
    },
    {
        0x11110C00u,
        "GraphicEffectAddScaleAttached11",
        "addScaleAttached11",
        "Generate a graphical effect that moves with the bone. Additional values can be set for size. The rotation and size don't move from the set values."
    },
    {
        0x110B0500u,
        "GraphicEffectReoccursWhenMove",
        "reoccursWhenMove",
        "Generate a graphical effect. And, regenerates the same graphical effect each time it moves the specified distance. Stay at the spot where everything was spawned."
    },
    {
        0x110D0100u,
        "EndGraphicEffectReoccursWhenMove",
        "endGraphicEffectReoccursWhenMove",
        "Ends the effect generated each time the specified graphic effect moves a specified distance."
    },
    {
        0x11140200u,
        "TerminateAllGraphicEffect",
        "terminateAll",
        "Terminate all graphical effect."
    },
    {
        0x11150300u,
        "TerminateGraphicEffect",
        "terminate",
        "Terminate specified graphical effect."
    },
    {
        0x11031400u,
        "SwordGlow",
        "swordGlow",
        "Creates glow of sword. Graphic effects that move with the bones can be generated together. If the specified color can't be loaded successfully, the displayed color will be pure white."
    },
    {
        0x11041700u,
        "SwordHammerGlow",
        "swordHammerGlow",
        "Creates glow of Hammer. Graphic effects that move with the bones can be generated together. If the specified color can't be loaded successfully, the displayed color will be pure white."
    },
    {
        0x11050100u,
        "TerminateSwordGlow",
        "terminateSwordGlow",
        "Remove all Glow effects."
    },
    {
        0x21010400u,
        "FlashOverlayEffect",
        "flashOverlayEffect",
        "Generate a flash overlay effect over the character with the specified colors and opacity. Replaces any currently active flash effects."
    },
    {
        0x21020500u,
        "ChangeFlashOverlayColor",
        "changeFlashOverlayColor",
        "Changes the color of the current flash overlay effect."
    },
    {
        0x21050600u,
        "FlashLightEffect",
        "flashLightEffect",
        "Generate a flash lighting effect over the character with the specified colors, opacity and angle. Replaces any currently active flash effects."
    },
    {
        0x21070500u,
        "ChangeFlashLightColor",
        "changeFlashLightColor",
        "Changes the color of the current flash light effect."
    },
    {
        0x21000000u,
        "TerminateFlashEffect",
        "terminateFlashEffect",
        "Terminate all currently active flash effects."
    },
    {
        0x11170600u,
        "ScreenTintFull",
        "screenTintFull",
        "Tint the screen to the specified color."
    },
    {
        0x11170700u,
        "ScreenTint",
        "screenTint",
        "Tint the screen to the specified color. Mainly used to tint only the entire stage."
    },
    {
        0x11180200u,
        "EndScreenTint",
        "endScreenTint",
        "End \"Screen Tint\" effect."
    },
    {
        0x111E0100u,
        "SetFlashingEffect",
        "setFlashingEffect",
        "Set a flashing effect on the character. Depending on the type of flashing effect have set, the flashing effect will continue even if after change the action."
    },
    {
        0x111F0100u,
        "EndFlashingEffect",
        "endFlashingEffect",
        "Ends character's flashing effect. Can turn off the flashing effect such as when invincible, smash charging."
    },
    {
        0x11200000u,
        "EndAllFlashingEffect",
        "endAllFlashingEffect",
        "Ends all flashing effects currently set on the character."
    },
    {
        0x210B0100u,
        "LockShadowEffect",
        "lockShadowEffect",
        "Fixes the change in character color due to shadows, etc. to the specified value. The corresponding value is unknown. Loading \"End Shadow Effect\" returns it to normal. If load this without parameters, the value will be 1. It used for Snake's Final Smash, used only to fix the specified shadow."
    },
    {
        0x210C0000u,
        "EndLockShadowEffect",
        "endLockShadowEffect",
        "The fixation of character color changes due to shadows etc. by \"Lock Shadow Effect\" is returned to normal."
    },
    {
        0x12000200u,
        "BasicVariableSet",
        "basicSet",
        "Set a basic variable to the specified value."
    },
    {
        0x12010200u,
        "BasicVariableAdd",
        "basicAdd",
        "Add a specified value to a basic variable."
    },
    {
        0x12020200u,
        "BasicVariableSubtract",
        "basicSubtract",
        "Subtract a specified value from a basic variable."
    },
    {
        0x12030100u,
        "BasicVariableIncrement",
        "basicIncrement",
        "Variable++ (adds 1)"
    },
    {
        0x12040100u,
        "BasicVariableDecrement",
        "basicDecrement",
        "Variable-- (subtracts 1)"
    },
    {
        0x12060200u,
        "FloatVariableSet",
        "floatSet",
        "Set a floating point variable to the specified value."
    },
    {
        0x12070200u,
        "FloatVariableAdd",
        "floatAdd",
        "Add a specified value to a float variable."
    },
    {
        0x12080200u,
        "FloatVariableSubtract",
        "floatSubtract",
        "Subtract a specified value from a float variable."
    },
    {
        0x120A0100u,
        "BitVariableSet",
        "bitSet",
        "Set a bit variable to true."
    },
    {
        0x120B0100u,
        "BitVariableClear",
        "bitClear",
        "Set a bit variable to false."
    },
    {
        0x120D0200u,
        "BasicVariableMultiply",
        "basicMultiply",
        "Multiply a basic value by the specified value."
    },
    {
        0x120E0200u,
        "BasicVariableDivide",
        "basicDivide",
        "Divide a basic value by the specified value."
    },
    {
        0x120F0200u,
        "FloatVariableMultiply",
        "floatMultiply",
        "Multiply a specified value with a float variable."
    },
    {
        0x12100200u,
        "FloatVariableDivide",
        "floatDivide",
        "Divide a specified value with a float variable."
    },
    {
        0x12050300u,
        "BasicVariableRandom",
        "basicRandom",
        "Set a basic variable to a random value from the maximum and minimum values"
    },
    {
        0x12110200u,
        "BasicVariableSetAbsolute",
        "basicSetAbsolute",
        "Sets a Basic type variable to the absolute value of the specified value"
    },
    {
        0x12090300u,
        "FloatVariableRandom",
        "floatRandom",
        "Set a float variable to a random value from the maximum and minimum values"
    },
    {
        0x12120200u,
        "FloatVariableSetAbsolute",
        "floatSetAbsolute",
        "Set a float variable equal to the absolute value of some float."
    },
    {
        0x0B020100u,
        "Visibility",
        "visibility",
        "Set whether oneself is visible or not. True = Visible, False = Invisible"
    },
    {
        0x0B000200u,
        "ModelChanger",
        "modelChanger",
        "Changes the character's model in certain preset ways defined in the Misc section. Will revert back after action ends. (Examples: yellow eye, sheathe sword, retreat into shell, etc.)"
    },
    {
        0x0B010200u,
        "ModelChangerPermanent",
        "modelChangerPermanent",
        "Changes the character's model in certain preset ways defined in the Misc section. Will persist even after action ends. (Examples: yellow eye, sheathe sword, retreat into shell, etc.)"
    },
    {
        0x0B030100u,
        "UndoModelChanger",
        "undoModelChanger",
        "Undo the model pattern called by \"Model Changer\". It has no effect on \"Model Changer (Permanent)\" because this event is not an event to read return to the default."
    },
    {
        0x18000100u,
        "SlopeContourStand",
        "slopeContourStand",
        "Keeps such as feet properly on ground when using on the ground. 0 is none, 1 is entire body, 2 is Left foot, 4 is Right foot. Basically a combination of these."
    },
    {
        0x18010200u,
        "SlopeContourStand02",
        "slopeContourTimer",
        "Keeps such as feet properly on ground slowly when using on the ground. The value same as \"Slope Contour Stand\"."
    },
    {
        0x18010300u,
        "SlopeContourStand03",
        "slopeContourStand03",
        "Keeps such as feet properly on ground when using on the ground. Difference from \"Slope Contour Stand\" (18000100) is unknown."
    },
    {
        0x1A000100u,
        "Screenshake",
        "screenshake",
        "Shakes the screen. Normal value is 0 to 2. 0 to 2: A little time (Higher = Greater) 3: Endless (Note: Don't use it as it is irreversible) 4: Weaker than 0. 5 or more: No effect"
    },
    {
        0x1A030400u,
        "SetCameraBoundaries",
        "setCameraBoundaries",
        "Changes the camera boundaries of the character. Doesn't reset the camera boundaries; rather, it adds to them. Reverts to normal when change the Action or load \"Reset Camera Boundaries\"."
    },
    {
        0x1A020200u,
        "MoveCameraBoundaries",
        "moveCameraBoundaries",
        "Moves the camera boundaries of the character. Doesn't reset the camera boundaries; rather, it adds to them. Reverts to normal when change the Action or load \"Reset Camera Boundaries\"."
    },
    {
        0x1A010000u,
        "ResetCameraBoundaries",
        "resetCameraBoundaries",
        "Reset camera boundaries changed by \"Set Camera Boundaries\" etc."
    },
    {
        0x1A040500u,
        "CameraCloseup",
        "cameraCloseup",
        "Move the camera to the specified position based on the position where the character is. When loading continuously, the camera will not return to the normal state unless the \"Normal Camera\" is sandwiched between them. And, if the setting distance is too far, if don't set the camera to a close distance again using this Event when returning to the normal camera, the camera will become strange."
    },
    {
        0x1A080000u,
        "NormalCamera",
        "normalCamera",
        "Return the camera moved by \"Camera Closeup\" to its normal settings."
    },
    {
        0x1A060100u,
        "DetachAttachCameraClose",
        "detachAttachCameraClose",
        "Causes the camera to follow or stop following a character. False = Detached, True = Attached (Normal)."
    },
    {
        0x1A070100u,
        "DetachAttachCameraFar",
        "detachAttachCameraFar",
        "Set whether the camera recognizes characters. False = Deviate, True = Normal."
    },
    {
        0x1A0B0000u,
        "DisableCameraZoom",
        "disableCameraZoom",
        "Disable camera zoom such as training mode. (Note: It doesn't affect the \"Camera Closeup\" Event.)"
    },
    {
        0x1A0C0000u,
        "EnableCameraZoom",
        "enableCameraZoom",
        "Undo the effect of \"Disable Camera Zoom\". (Note: If read \"Disable Camera Zoom\" multiple times, you need to read the same or more times to get the effect)"
    },
    {
        0x1F000100u,
        "PickupItem",
        "pickupItem",
        "Pick up the closest item within the range where the character can pick up the item."
    },
    {
        0x1F000200u,
        "PickupItem2",
        "pickupItem2",
        "Pick up the closest item within the range where the character can pick up the item."
    },
    {
        0x1F010300u,
        "ThrowItem",
        "throwItem",
        "The character throws currently held item with the specified angle and momentum."
    },
    {
        0x1F0E0500u,
        "ThrowItemFar",
        "throwItemFar",
        "The character throws currently held item with the specified angle and momentum. In addition, the initial position can be set to a position far away from the character."
    },
    {
        0x1F020000u,
        "DropItem",
        "dropItem",
        "Cause the character to drop any currently held item. Some items disappear when it touches the floor."
    },
    {
        0x1F030100u,
        "ConsumeItem",
        "consumeItem",
        "Cause the character to consume the currently held item. There seems to be no problem if always specify 0 for the parameter in all cases. Some items, such as food and stickers, can be generated with \"Generate Item\" and then consumed immediately with \"Consume Item: 0x0\" (The effect of that item will also occur), but other items will simply disappear."
    },
    {
        0x1F0A0000u,
        "DeleteHeldItem",
        "deleteHeldItem",
        "Delete the currently held item."
    },
    {
        0x1F050000u,
        "FireWeapon",
        "fireWeapon",
        "Fires a bullet from the currently held item only if it has bullets."
    },
    {
        0x1F060100u,
        "FireProjectile",
        "fireProjectile",
        "Fires a bullet of the specified degree of power from the currently held item only if it has bullets."
    },
    {
        0x1F070100u,
        "RocketOperation",
        "rocketOperation",
        "Fires a bullet from the currently held item only if it has bullets. Can set the shooting angle only when held cracker launcher. (Value is Scalar/60000. 0 = Forward, 0.0015 = Upward. If the set type is \"Value\", can't fire.)"
    },
    {
        0x1F080100u,
        "GenerateItem",
        "generateItem",
        "Generate the specified item in the character's hand."
    },
    {
        0x1F090100u,
        "HeldItemsVisibility",
        "heldItemsVisibility",
        "Determines visibility of the currently held item. True = Visible, False = Invisible"
    },
    {
        0x1F0F0100u,
        "WearableItemsVisibility",
        "wearableItemsVisibility",
        "Visibility of wearable items (Bunny Hood, Franklin Badge, Gooey Bomb, etc) True = Visible, False = Invisible"
    },
    {
        0x1F0B0100u,
        "ChangeItemBoneId",
        "changeItemBoneId",
        "Change the bone where the character has the item."
    },
    {
        0x1F040200u,
        "ChangeBeamSwordSize",
        "changeBeamSwordSize",
        "Modify blade size of held Beam Sword. The value is transition time, Scalar is blade size."
    },
    {
        0x1F0C0100u,
        "WeaponOperation",
        "weaponOperation",
        "Change action of held Fan."
    },
    {
        0x1F0D0000u,
        "ReleaseAssist",
        "releaseAssist",
        "Release Assist Trophy or Poke Ball."
    },
    {
        0x0C060000u,
        "EnterFinalSmashState",
        "enterFinalSmashState",
        "When the character can use Final Smash, set to Final Smash state. When used in certain articles (Mario's Fireball etc), it loads in a value from article floating point parameter table and applies it (Mario's Fireball loads in velocity bounce multiplier, for example)."
    },
    {
        0x0C070000u,
        "ExitFinalSmashState",
        "exitFinalSmashState",
        "Exit Final Smash State."
    },
    {
        0x14050100u,
        "AcceptVariousGimmicksEffect",
        "acceptVariousGimmicksEffect",
        "When false, disable various gimmicks (Wind, Ladder, Swim, Catapult, Door etc). There are also effects that are not related to the stage gimmick, such as not being able to pick up items, ignore Negative zone, the spring cannot be used, and cannot Footstool."
    },
    {
        0x14000100u,
        "EnableSpecifiedEffect",
        "enableSpecifiedEffect",
        "Re-enable the effect disabled by \"Enable Specified Effect\". If increase 1 the parameter (Set Event ID to 14000200), the number of valid frames can be set. 0 = Negative zone and some stage gimmicks (Wind, Conveyor, Catapult, Door etc), 2 = Footstooled, 3 = Swim, 4 = Ladder."
    },
    {
        0x14010100u,
        "DisableSpecifiedEffect",
        "disableSpecifiedEffect",
        "Disable certain effects until load the \"Enable Specified Effect\". Even if the action is changed, it does not return. 0 = Negative zone and some stage gimmicks (Wind, Conveyor, Catapult, Door etc), 2 = Footstooled, 3 = Swim, 4 = Ladder."
    },
    {
        0x0C160000u,
        "DisableMagnifyingGlass",
        "disableMagnifyingGlass",
        "Disable the display and damage of magnifying glass."
    },
    {
        0x0C0D0000u,
        "ResetStageSpeed",
        "resetStageSpeed",
        "Return stage speed to default speed."
    },
    {
        0x0C0C0000u,
        "SlowStageSpeed",
        "slowStageSpeed",
        "Slow down the progress of the stage."
    },
    {
        0x0C0E0000u,
        "StopStageSpeed",
        "stopStageSpeed",
        "Stop the progress of the stage. (Note: If use it in Subspace, will not be able to proceed.)"
    },
    {
        0x0C0F0000u,
        "SlowStageSpeedQuestion",
        "slowStageSpeedQuestion",
        "Slow down the progress of the stage?"
    },
    {
        0x0C230200u,
        "TimeManipulation",
        "timeManipulation",
        "Slow down enemy movement. Mainly used for Final Smash. It is no influence other than fighter."
    },
    {
        0x0C250100u,
        "TagDisplay",
        "tagDisplay",
        "Disables or enables tag display for the current action. True = ON, False = OFF (Tag is the icon above your player)"
    },
    {
        0x01000000u,
        "GoToLoopRest01",
        "goToLoopRest01",
        "Used with \"Go to Loop Rest 02\" or \"Flow 03\" to reset the Event List timer when the animation loops."
    },
    {
        0x01020000u,
        "GoToLoopRest02",
        "goToLoopRest02",
        "Used with \"Go to Loop Rest 01\" to reset the Event List timer when the animation loops."
    },
    {
        0x00030000u,
        "Flow03",
        "flow03",
        "Used with \"Go to Loop Rest 01\" to reset the Event List timer when the animation loops."
    },
    {
        0x14070A00u,
        "AestheticWindEffect",
        "aestheticWindEffect",
        "Moves nearby movable model parts (capes, hair, etc) with a wind specified by the parameters."
    },
    {
        0x14040100u,
        "TerminateWindEffect",
        "terminateWindEffect",
        "Ends the wind effect spawned by \"Aesthetic Wind Effect\" Event."
    },
    {
        0x0D000200u,
        "ConcurrentInfiniteLoop",
        "concurrentInfiniteLoop",
        "Runs a subroutine once per frame for the current action in parallel. This subroutine loop will run independently of the code that comes after it in the action."
    },
    {
        0x0D010100u,
        "TerminateConcurrentInfiniteLoop",
        "terminateConcurrentInfiniteLoop",
        "Stop the execution of a loop created by 0D000200 (Concurrent Infinite Loop)."
    },
    {
        0x07000000u,
        "ResetFlickX",
        "resetFlickX",
        "Reset IC-Basic[21001] (FramesSinceNeutralStickX) but no effect to IC-Basic[21003]."
    },
    {
        0x07010000u,
        "ResetFlickY",
        "resetFlickY",
        "Reset IC-Basic[21002] (FramesSinceNeutralStickY) but no effect to IC-Basic[21004]."
    },
    {
        0x07020000u,
        "ResetButtonPress",
        "resetButtonPress",
        "Reset \"Requirement: Button Press\". No effect to \"Requirement: Button Pressed\"."
    },
    {
        0x070C0000u,
        "ClearBuffer",
        "clearBuffer",
        "Clears the controller buffer."
    },
    {
        0x07070200u,
        "Rumble",
        "rumble",
        "Controls the rumble on the controller."
    },
    {
        0x070B0200u,
        "RumbleLoop",
        "rumbleLoop",
        "Creates a rumble loop on the controller."
    },
    {
        0x0C140200u,
        "SetStaticArticle",
        "setStaticArticle",
        "Set a \"Static Article\" (Article not for battle use). Used in victories."
    },
    {
        0x0C150100u,
        "RemoveStaticArticle",
        "removeStaticArticle",
        "Remove Static Article. Used in victories."
    },
    {
        0x020D0100u,
        "ChangeReadingAction",
        "changeReadingAction",
        "Only used in Action Pre. Change the Action ID to be read."
    },
    {
        0x02050300u,
        "AdditionalActionInterruptsRequirement",
        "additionalActionInterruptsRequirement",
        "Only used in Extra Action Interrupts. Add an additional requirement to Change Action by Action Interrupts. In some cases, it be used to prevent switches to the specified Action. (Example: Donkey's Down B)"
    },
    {
        0x02050400u,
        "AdditionalActionInterruptsRequirementValue",
        "additionalActionInterruptsRequirementValue",
        "Only used in Extra Action Interrupts. Add an additional requirement to Change Action by Action Interrupts. In some cases, it be used to prevent switches to the specified Action. (Example: Donkey's Down B)"
    },
    {
        0x02050600u,
        "AdditionalActionInterruptsRequirementComparison",
        "additionalActionInterruptsRequirementComparison",
        "Only used in Extra Action Interrupts. Add an additional requirement to Change Action by Action Interrupts. In some cases, it be used to prevent switches to the specified Action. (Example: Donkey's Down B)"
    },
    {
        0x020C0100u,
        "ClearPreventInterrupt",
        "clearPreventInterrupt",
        "Possibly unregisters a previously created interrupt."
    },
    {
        0x03010400u,
        "OverrideBoneRotate",
        "overrideBoneRotate",
        "Change the rotation of the specified bone to the specified value for only 1 frame. In subactions, it can only be read in \"Main\" or \"Other\". Since the effect can only be kept for 1 frame, there is a problem that it can not be used if slow or hitlag is applied even if reading every frame. But, it is useful when used in combination with event of generate a graphical effect that stays at the spot it was spawned."
    },
    {
        0x03060400u,
        "OverrideBoneScale",
        "overrideBoneScale",
        "Change the scale of the specified bone to the specified value for only 1 frame. In subactions, it can only be read in \"Main\" or \"Other\". Since the effect can only be kept for 1 frame, there is a problem that it can not be used if slow or hitlag is applied even if reading every frame. But, it is useful when used in combination with event of generate a graphical effect that stays at the spot it was spawned. (Note: GFX size is not changed, instead each offset spacing is changed)"
    },
    {
        0x030B0400u,
        "OverrideBoneOffset",
        "overrideBoneOffset",
        "Change the offset of the specified bone to the specified value for only 1 frame. In subactions, it can only be read in \"Main\" or \"Other\". Since the effect can only be kept for 1 frame, there is a problem that it can not be used if slow or hitlag is applied even if reading every frame. But, it is useful when used in combination with event of generate a graphical effect that stays at the spot it was spawned."
    },
    {
        0x03020200u,
        "OverrideBoneRotateX",
        "overrideBoneRotateX",
        "Change the rotation of the specified bone to the specified value for only 1 frame. Only the X rotation can be specified, and Y and Z rotation will be 0."
    },
    {
        0x03030200u,
        "OverrideBoneRotateY",
        "overrideBoneRotateY",
        "Change the rotation of the specified bone to the specified value for only 1 frame. Only the Y rotation can be specified, and X and Z rotation will be 0."
    },
    {
        0x03040200u,
        "OverrideBoneRotateZ",
        "overrideBoneRotateZ",
        "Change the rotation of the specified bone to the specified value for only 1 frame. Only the Z rotation can be specified, and X and Y rotation will be 0."
    },
    {
        0x03070200u,
        "OverrideBoneScaleX",
        "overrideBoneScaleX",
        "Change the scale of the specified bone to the specified value for only 1 frame. Only the X scale can be specified, and Y and Z scale will be 0."
    },
    {
        0x03080200u,
        "OverrideBoneScaleY",
        "overrideBoneScaleY",
        "Change the scale of the specified bone to the specified value for only 1 frame. Only the Y scale can be specified, and X and Z scale will be 0."
    },
    {
        0x03090200u,
        "OverrideBoneScaleZ",
        "overrideBoneScaleZ",
        "Change the scale of the specified bone to the specified value for only 1 frame. Only the Z scale can be specified, and X and Y scale will be 0."
    },
    {
        0x030C0200u,
        "OverrideBoneOffsetZ0C",
        "overrideBoneOffsetZ0C",
        "Change the offset of the specified bone to the specified value for only 1 frame. Only the Z offset can be specified, and X and Y offset will be 0."
    },
    {
        0x030D0200u,
        "OverrideBoneOffsetZ0D",
        "overrideBoneOffsetZ0D",
        "Change the offset of the specified bone to the specified value for only 1 frame. Only the Z offset can be specified, and X and Y offset will be 0."
    },
    {
        0x030E0200u,
        "OverrideBoneOffsetZ0E",
        "overrideBoneOffsetZ0E",
        "Change the offset of the specified bone to the specified value for only 1 frame. Only the Z offset can be specified, and X and Y offset will be 0."
    },
    {
        0x04090100u,
        "SubAction09",
        "subAction09",
        "It seems to be an event to change the current sub action. It mainly used in Subspace enemies. Unstable and easy to crash the game when used in sub actions."
    },
    {
        0x040A0100u,
        "SubActions0A",
        "subActions0A",
        "Unknown. Set Sub Action ID?"
    },
    {
        0x040B0100u,
        "SubActions0B",
        "subActions0B",
        "Unknown. Set Frame Speed?"
    },
    {
        0x07060100u,
        "Controller06",
        "controller06",
        "Unknown."
    },
    {
        0x08010100u,
        "EdgeInteraction01",
        "edgeInteraction01",
        "Unknown."
    },
    {
        0x08020100u,
        "EdgeInteraction02",
        "edgeInteraction02",
        "Unknown."
    },
    {
        0x08040100u,
        "EdgeInteraction04",
        "edgeInteraction04",
        "Unknown."
    },
    {
        0x0C010000u,
        "CharacterSpecific01",
        "characterSpecific01",
        "Unknown."
    },
    {
        0x0C170100u,
        "CharacterSpecific17",
        "characterSpecific17",
        "Unknown. Often appears before 0C25 (Tag Display)"
    },
    {
        0x0C170200u,
        "CharacterSpecific17Variable",
        "characterSpecific17Variable",
        "Unknown. Often appears before 0C25 (Tag Display)"
    },
    {
        0x0C270000u,
        "CharacterSpecific27",
        "characterSpecific27",
        "Unknown. Often appears within Switch statements."
    },
    {
        0x0C2B0000u,
        "CharacterSpecific2B",
        "characterSpecific2B",
        "Unknown"
    },
    {
        0x0E0B0200u,
        "GraphicModelSpecf",
        "graphicModelSpecf",
        "Appears to control posture graphics."
    },
    {
        0x0F030200u,
        "Link03",
        "link03",
        "Unknown"
    },
    {
        0x17000000u,
        "PhysicsNormalize",
        "physicsNormalize",
        "Returns to normal physics."
    },
    {
        0x17010000u,
        "Physics01",
        "physics01",
        "Unknown"
    },
    {
        0x17050000u,
        "Physics05",
        "physics05",
        "Unknown"
    },
    {
        0x18030200u,
        "CharacterSpecific03",
        "characterSpecific03",
        "Unknown. Used in Samus."
    },
    {
        0x19010000u,
        "Module19_01",
        "module19_01",
        "Unknown"
    },

    // ---- 64 entries mined from CodecSMW/BrawlCrate/CustomLists/MovesetData/Events.txt
    // (fork master 2026-03; parent soopercool101/BrawlCrate; license BSD-3)
    // Thanks Duke
    {
        0x03000000u,
        "ResetLayerType2",
        "resetLayerType2",
        "Undefined."
    },
    {
        0x03000100u,
        "ResetLayerType",
        "resetLayerType",
        "Undefined"
    },
    {
        0x03050100u,
        "SetTransNBoneID",
        "setTransNBoneID",
        "Changes the TransN Bone ID reference."
    },
    {
        0x030A0400u,
        "StoreGlobalBonePosition",
        "storeGlobalBonePosition",
        "Stores the position of the given bone into 3 variables, one for each axis."
    },
    {
        0x03100400u,
        "GetNodeGlobalRotationCustom",
        "getNodeGlobalRotationCustom",
        "Functions similarly to Store Global Bone Position, except for rotation instead of position. Requires \"PSA Command 03100400: Get Node Global Rotation [MarioDox]\" to be in your codeset."
    },
    {
        0x04000300u,
        "ChangeSubActionPassFramePassFrameSpeed",
        "changeSubAction",
        "Change the current sub action. Can specify whether or not to pass the current frame or start the animation over. Also can specify the frame speed if you give it a scalar/variable, it reads it as the frame speed"
    },
    {
        0x04070200u,
        "MultiplyFrameSpeedModifier",
        "multiplyFrameSpeedModifier",
        "Multiplies the current Frame Speed. Example: Setting to 2 will multiply the current Frame Speed by 2."
    },
    {
        0x040C0200u,
        "ChangeSubActionReversePlayPassFrame",
        "changeSubActionReversePlay",
        "Change the current sub action (plays in reverse). Can specify whether or not to pass the current frame or start the animation over."
    },
    {
        0x040C0300u,
        "ChangeSubActionReversePlayPassFramePassFrameSpeed",
        "changeSubActionReversePlay",
        "Change the current sub action (plays in reverse). Can specify whether or not to pass the current frame or start the animation over. Also can specify the frame speed if you give it a scalar/variable, it reads it as the frame speed"
    },
    {
        0x04180100u,
        "SubActionAnimationOffset",
        "subActionAnimationOffset",
        "Offsets a Change Sub Action command by what's specified in this command."
    },
    {
        0x05070300u,
        "Posture07",
        "posture07",
        "Unknown."
    },
    {
        0x050D0100u,
        "Posture0D",
        "posture0D",
        "Unknown."
    },
    {
        0x061C0300u,
        "SetDefensiveCollisionFacingRestriction",
        "setDefensiveCollisionFacingRestriction",
        "Ignores attacks that face the same direction of the user."
    },
    {
        0x06210100u,
        "GrabToggle",
        "grabToggle",
        "When set to True, the user can be grabbed within the current action. When set to False, the user cannot be grabbed within the current action. Resets to default upon action change."
    },
    {
        0x08050400u,
        "ModifyECBShape",
        "modifyECBShape",
        "Modifies the collision points of a character's ECB."
    },
    {
        0x0A060100u,
        "StopSoundEffectTransient",
        "stopSoundEffectTransient",
        "Stops the specified sound effect immediately."
    },
    {
        0x0C0A0100u,
        "CharacterSpecific0A",
        "characterSpecific0A",
        "Unknown."
    },
    {
        0x0C130000u,
        "CharacterSpecific13",
        "characterSpecific13",
        "Undefined."
    },
    {
        0x0C1B0100u,
        "CharacterSpecific1B",
        "characterSpecific1B",
        "Unknown."
    },
    {
        0x0C1C0300u,
        "CharacterSpecific1CBoolean",
        "characterSpecific1CBoolean",
        "Unknown."
    },
    {
        0x0C200200u,
        "CharacterSpecific20",
        "characterSpecific20",
        "Unknown."
    },
    {
        0x0C240100u,
        "CharacterSpecific24",
        "characterSpecific24",
        "Unknown."
    },
    {
        0x0C260100u,
        "CharacterSpecific26",
        "characterSpecific26",
        "Unknown."
    },
    {
        0x0C290000u,
        "CharacterSpecific29",
        "characterSpecific29",
        "Undefined."
    },
    {
        0x0D050200u,
        "IndependentSubroutineCustom",
        "independentSubroutineCustom",
        "Makes and starts a new independent subroutine. (Requires the Independent Subroutines code by Mawootad.)"
    },
    {
        0x0D060100u,
        "TerminateIndependentSubroutineCustom",
        "terminateIndependentSubroutineCustom",
        "Stops a running independent subroutine. (Requires the relevant code by Mawootad.)"
    },
    {
        0x0D070200u,
        "SetThreadTypeCustom",
        "setThreadTypeCustom",
        "Sets a new thread type. (Requires the Independent Subroutines code by Mawootad.)"
    },
    {
        0x10040100u,
        "SetArticleAction",
        "setArticleAction",
        "Sets the specified article to execute the specified action immediately. Only works on anchored articles (Cape, FLUDD, not fireball, water)."
    },
    {
        0x11060000u,
        "PauseFighterGFX",
        "pauseFighterGFX",
        "Pauses the animation of any currently active GFX spawned from your fighter at the point this command is activated. Any GFX spawned afterward will still play normally."
    },
    {
        0x11070000u,
        "ResumeFighterGFX",
        "resumeFighterGFX",
        "Resumes any GFX which were previously paused by the Pause Fighter GFX command."
    },
    {
        0x111D0100u,
        "EffectID",
        "effectID",
        "Undefined."
    },
    {
        0x11210100u,
        "SetGFXAnimationIndexCustom",
        "setGFXAnimationIndexCustom",
        "Sets the animation ID for the chosen GFX, clears after GFX spawn. This command must be placed before using your desired Graphic Effect command. Requires Eon's \"Set Anim Index\" code."
    },
    {
        0x12150300u,
        "ReadFromPointerPathToVariableCustom",
        "readFromPointerPathToVariableCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x12160400u,
        "ReadFromPointerPathWithMaskToVariableCustom",
        "readFromPointerPathWithMaskToVariableCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x12170300u,
        "WriteIntegerToPointedAddressCustom",
        "writeIntegerToPointedAddressCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x12180300u,
        "WriteFloatToPointedAddressCustom",
        "writeFloatToPointedAddressCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x12190400u,
        "WriteBitUsingMaskToPointedAddressCustom",
        "writeBitUsingMaskToPointedAddressCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x121A0200u,
        "ReadFromPointerPathUsingMainIndexToVariableCustom",
        "readFromPointerPathUsingMainIndexToVariableCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x121A0F00u,
        "ReadFromPointerPathUsingMainIndexToVariableCustomD00",
        "readFromPointerPathUsingMainIndexToVariableCustomD00",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x121B0300u,
        "ReadFromPointerPathUsingMainIndexWithMaskToVariableCustom",
        "readFromPointerPathUsingMainIndexWithMaskToVariableCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x121C0200u,
        "WriteIntegerToPointedAddressUsingMainIndexCustom",
        "writeIntegerToPointedAddressUsingMainIndexCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x121D0200u,
        "WriteFloatToPointedAddressUsingMainIndexCustom",
        "writeFloatToPointedAddressUsingMainIndexCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x121E0300u,
        "WriteBitUsingMaskToPointedAddressUsingMainIndexCustom",
        "writeBitUsingMaskToPointedAddressUsingMainIndexCustom",
        "Requires Pointer Wizardry code by Eon"
    },
    {
        0x12200200u,
        "AttributeRangeSetCustom",
        "attributeRangeSetCustom",
        "Requires the On the Fly Attribute Modification code by Mawootad."
    },
    {
        0x12210200u,
        "AttributeRangeAddCustom",
        "attributeRangeAddCustom",
        "Requires the On the Fly Attribute Modification code by Mawootad."
    },
    {
        0x12220200u,
        "AttributeRangeSubtractCustom",
        "attributeRangeSubtractCustom",
        "Requires the On the Fly Attribute Modification code by Mawootad."
    },
    {
        0x12230200u,
        "AttributeRangeMultiplyCustom",
        "attributeRangeMultiplyCustom",
        "Requires the On the Fly Attribute Modification code by Mawootad."
    },
    {
        0x12240200u,
        "AttributeRangeDivideCustom",
        "attributeRangeDivideCustom",
        "Requires the On the Fly Attribute Modification code by Mawootad."
    },
    {
        0x12500200u,
        "SinCustom",
        "sinCustom",
        "Executes a sin equation, storing the result into the variable of your choice. Requires Maths code by Eon"
    },
    {
        0x12510200u,
        "CosCustom",
        "cosCustom",
        "Executes a cos equation, storing the result into the variable of your choice. Requires Maths code by Eon"
    },
    {
        0x12520200u,
        "AsinCustom",
        "asinCustom",
        "Executes an asin equation, storing the result into the variable of your choice. Requires Maths code by Eon"
    },
    {
        0x12530200u,
        "AcosCustom",
        "acosCustom",
        "Executes an acos equation, storing the result into the variable of your choice. Requires Maths code by Eon"
    },
    {
        0x12540300u,
        "ATan2Custom",
        "aTan2Custom",
        "Executes an aTan2 equation, storing the result into the variable of your choice. Requires Maths code by Eon"
    },
    {
        0x12550200u,
        "SqrtCustom",
        "sqrtCustom",
        "Executes a sqrt equation, storing the result into the variable of your choice. Requires Maths code by Eon"
    },
    {
        0x12560300u,
        "PowerCustom",
        "powerCustom",
        "Executes a power equation, storing the result into the variable of your choice. Requires Maths code by Eon"
    },
    {
        0x12570100u,
        "GetPiCustom",
        "getPiCustom",
        "Retrieves and stores pi into the variable of your choice. Requires Maths code by Eon"
    },
    {
        0x14070900u,
        "AestheticWindEffect2",
        "aestheticWindEffect2",
        "Moves nearby movable model parts (capes, hair, etc) with a wind specified by the parameters."
    },
    {
        0x1A090000u,
        "Camera09",
        "camera09",
        "Undefined."
    },
    {
        0x1C000200u,
        "SetHitlag",
        "setHitlag",
        "Sets the hitlag on the current move."
    },
    {
        0x1F080200u,
        "GenerateItemVariantCustom",
        "generateItemVariantCustom",
        "Generate an item while specifying its variant. Requires the \"PSA Command 1F080200 (spawn item variant)\" code by Sammi Husky."
    },
    {
        0x1F110100u,
        "Item11",
        "item11",
        "Undefined."
    },
    {
        0x1F120600u,
        "GenerateAndThrowItemCustom",
        "generateAndThrowItemCustom",
        "Generates and immediately throws a given item. Requires the \"Custom GenerateAndThrowItem PSA command\" code by Sammi Husky."
    },
    {
        0x20000200u,
        "Turn00",
        "turn00",
        "unknown."
    },
    {
        0xC0DE0100u,
        "ChangeHitboxSoundEffectCustom",
        "changeHitboxSoundEffectCustom",
        "Change the sound effect of the next hitbox. Requires the Hitbox Sound Effect Change System code by JOJI."
    },

};
} // namespace


// Process-global toggle set by main() when the user passes --legacy. When
// on, command_name() returns the PSAC/historical name (legacy_name); off,
// it returns the dotted-syntax method_name. See Event::to_pretty_string
// for the corresponding module.method wrapping.
namespace {
bool g_legacy_mode = false;
}

void set_legacy_display_mode(bool on)  { g_legacy_mode = on; }
bool is_legacy_display_mode()          { return g_legacy_mode; }

const char* command_name(uint32_t cmd_id) {
    if (const char* o = override_command_name(cmd_id)) return o;
    for (const auto& n : kNames) {
        if (n.id == cmd_id) {
            return g_legacy_mode ? n.legacy_name : n.method_name;
        }
    }
    return nullptr;
}

const char* command_legacy_name(uint32_t cmd_id) {
    for (const auto& n : kNames) {
        if (n.id == cmd_id) return n.legacy_name;
    }
    return nullptr;
}

const char* command_method_name(uint32_t cmd_id) {
    if (const char* o = override_command_name(cmd_id)) return o;
    for (const auto& n : kNames) {
        if (n.id == cmd_id) return n.method_name;
    }
    return nullptr;
}

const char* command_description(uint32_t cmd_id) {
    if (const char* o = override_command_description(cmd_id)) return o;
    for (const auto& n : kNames) {
        if (n.id == cmd_id) return n.desc;
    }
    return nullptr;
}

namespace {
struct FormatEntry { uint32_t id; const char* fmt; };

// Per-command argument display formats. Placeholders `{0}`, `{1}`, ... are
// replaced by each decoded arg's pretty-string. Only override the default
// "arg0, arg1, ..." rendering here when PSAC displays it differently.
constexpr FormatEntry kFormats[] = {
    // Variable ops (wire order: value/target, then variable being written).
    {0x12000200u, "{1} = {0}"},          // BasicVariableSet:      var = value
    {0x12040200u, "{1} += {0}"},         // BasicVariableAdd (if present)
    {0x12060200u, "{1} -= {0}"},         // BasicVariableSubtract (if present)
    {0x120A0100u, "{0} = true"},         // BitVariableSet
    {0x120B0100u, "{0} = false"},        // BitVariableClear
};

// Module table — keyed on the top byte of the cmd_id. Long names come from
// the game's decompiled module symbols (kept for tooltip / --verbose use);
// shorthand is what we render as the "<module>." prefix in dotted display.
// Absent entries (byte 0 = 0x00 flow, 0x01 loopRest, or anything else we
// don't map) render as bare methodName(args) with no dot prefix.
struct ModuleEntry {
    std::uint8_t top_byte;
    const char* shorthand;
    const char* long_name;
};

constexpr ModuleEntry kModules[] = {
    {0x02, "status",     "soStatusModuleImpl"},
    {0x03, "model",      "soModelModuleImpl"},
    {0x04, "motion",     "soMotionModuleImpl"},
    {0x05, "posture",    "soPostureModuleImpl"},
    {0x06, "collision",  "soCollision*ModuleImpl"},   // unified across 5 sub-modules
    {0x07, "controller", "ftControllerModuleImpl"},
    {0x08, "ground",     "soGroundModuleImpl"},
    {0x09, "situation",  "soSituationMoudleImpl"},    // sic — matches BC symbol
    {0x0A, "sound",      "soSoundModuleImpl"},
    {0x0B, "visibility", "soVisibilityModuleImpl"},
    {0x0C, "character",  "ft<charname>"},
    {0x0D, "animcmd",    "soAnimCmdModuleImpl"},
    {0x0E, "kinetic",    "soKineticModuleGeneric"},
    {0x0F, "link",       "soLinkModuleImpl"},
    {0x10, "article",    "soGenerateArticleManageModuleImpl"},
    {0x11, "effect",     "soEffectModuleImpl"},
    {0x12, "work",       "soWorkManageModuleImpl"},
    {0x13, "combo",      "soComboModuleImpl"},
    {0x14, "area",       "ftAreaModuleImpl"},
    {0x15, "territory",  "soTerritoryModuleImpl"},
    {0x16, "target",     "soTargetSearchModuleImpl"},
    {0x17, "physics",    "soPhysicsModuleImpl"},
    {0x18, "slope",      "soSlopeModuleImpl"},
    {0x19, "shadow",     "soShadowModuleImpl"},
    {0x1A, "camera",     "soCameraModuleImpl"},
    {0x1C, "stop",       "ftStopModuleImpl"},
    {0x1D, "shake",      "soShakeModuleImpl"},
    {0x1E, "damage",     "soDamageModuleImpl"},
    {0x1F, "item",       "soItemManageModuleImpl"},
    {0x20, "turn",       "soTurnModuleImpl"},
    {0x21, "colorBlend", "soColorBlendModuleImpl"},
    {0x22, "team",       "soTeamModuleImpl"},
    {0x23, "slow",       "soSlowModuleImpl"},
    {0x64, "cancel",     "ftCancelModuleImpl"},
    {0xC0, "custom",     "<community-defined>"},
};

const ModuleEntry* find_module(std::uint32_t cmd_id) {
    const std::uint8_t tb = static_cast<std::uint8_t>((cmd_id >> 24) & 0xFF);
    for (const auto& m : kModules) if (m.top_byte == tb) return &m;
    return nullptr;
}
} // namespace

const char* command_format(uint32_t cmd_id) {
    if (const char* o = override_command_format(cmd_id)) return o;
    for (const auto& f : kFormats) {
        if (f.id == cmd_id) return f.fmt;
    }
    return nullptr;
}

const char* command_module_shorthand(std::uint32_t cmd_id) {
    const auto* m = find_module(cmd_id);
    return m ? m->shorthand : nullptr;
}

const char* command_module_long_name(std::uint32_t cmd_id) {
    const auto* m = find_module(cmd_id);
    return m ? m->long_name : nullptr;
}

std::vector<std::uint32_t> all_named_command_ids() {
    std::vector<std::uint32_t> out;
    out.reserve(sizeof(kNames) / sizeof(kNames[0]));
    for (const auto& n : kNames) out.push_back(n.id);
    return out;
}
} // namespace psax
