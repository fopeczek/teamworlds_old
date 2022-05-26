/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#pragma once

enum class Class {
    None,
    Hunter, //note:Offensive
    Medic,//note:Support TODO implement:can place in custom places pickupables, 2 options: onk cheap place 1 use pickup, or twok expensive place respawnable (permanent) pickupable
    Scout, //note:Offensive TODO BUG: when shooting someone infinite knock back (probably only when shooting underneath)  LOW PRIORITY: make him place mines (if on your team you will se a o [circle] of shields floating or rotating
    Tank, //note:Offensive
    Spider, //note:Offensive/Defensive TODO when some one is caught in web show a direction where he is | make bullets just slow down instead of collide with web
    Engineer, //note:Defensive TODO make that when two laser walls intersect one of them will stop on another (NO: --|--, but YES:   |--)
    Armorer,//note:Support TODO make better name, implement: can (like medic place custom pickupables same 2 options, but for weapons), maybe can create aura of battle or slt
    Necromancer//note:Offensive/Defensive|HARD to implement! TODO can summon offensive zombies and defensive ones, he is weak, after killing anyone he automatically summons free zombie private guard ! HARD TO IMPLEMENT !
    // TODO Bug can't change to blue
};