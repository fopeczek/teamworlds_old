/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#pragma once

enum class Class {
    None,
    Hunter, //note:Offensive TODO cool down bar so when you are using disguise the bar goes |<---|, but after you used it and you are waiting [15 sec] for cool down it goes |--->|
    Medic,//note:Defensive TODO implement:can place if custom places pickupables, 2 options: onk cheap place 1 use pickup, or twok expensive place respawnable (permanent) pickupable
    Scout, //note:Offensive TODO make him place mines (if on your team you will se a o [circle] of shields floating or rotating
    Tank, //note:Offensive TODO Implement: make him heavy and slower, can hold players inf. make him stronk dmg/2 or dmg/1.5, spawn with grenade launcher and reload gun (machine gun) faster
    Spider, //note:Offensive/Defensive TODO make like in engineer walls (make that when two laser walls intersect one of them will stop on another (NO: --|--, but YES:   |--))
    Engineer, //note:Defensive TODO make that when two laser walls intersect one of them will stop on another (NO: --|--, but YES:   |--)
    Armorer,//note:Defensive TODO make better name, implement: can (like medic place custom pickupables same 2 options, but for weapons), maybe can create aura of battle or slt. or give him ability to make turrets (Make 3 (while standing in place) hammer hits to create a turret)
    Necromancer//note:Offensive/Defensive|HARD to implement! TODO can summon offensive zombies and defensive ones, he is weak, after killing anyone he automatically summons free zombie private guard ! HARD TO IMPLEMENT !
    // TODO !MAYBE! add another class
};