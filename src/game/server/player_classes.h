/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#pragma once

enum class Class {
    None,
    Hunter, //note:Offensive TODO cool down bar so when you are using disguise the bar goes |<---|, but after you used it and you are waiting [20 sec] for cool down it goes |--->|
    Medic,//note:Defensive TODO implement:can place if custom places pickupables, 2 options: onk cheap place 1 use pickup, or twok expensive place respawnable (permanent) pickupable
    Commando, //note:Offensive TODO make him more like assassin or scout slt. make him weak but still let him rocket boost
    Tank, //note:Offensive TODO Implement: make him heavy and slower, can hold players inf. make him stronk dmg/2 or dmg/1.5, spawn with grenade launcher and reload gun (machine gun) faster
    Spider, //note:Offensive/Defensive TODO Make shotgun pusher/puller idk | make laser make slowing webs
    Engineer, //note:Defensive TODO Make 3 (while standing in place) hammer hits to create a turret
    Armorer,//note:Defensive TODO make better name, implement: can (like medic place custom pickupables same 2 options, but for weapons), maybe can create aura of battle or slt. or give him ability to make turrets
    Necromancer//note:Offensive/Defensive|HARD to implement! TODO can summon offensive zombies and defensive ones, he is weak, after killing anyone he automatically summons free zombie private guard ! HARD TO IMPLEMENT !
};