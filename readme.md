Teamwolrds
==========
Teamworlds is a game based on  [teeworlds](https://github.com/teeworlds/teeworlds)

![Notice that server type is named TeamUp](datasrc/github/server.png "Notice that server type is named TeamUp")
Notice that server type is named TeamUp.
---------
![This is lobby](/datasrc/github/Lobby.png "This is lobby")
Upon joining into my server you will spawn in lobby.
------------
![This is lobby](/datasrc/github/Lobby&weapons.png "This is lobby")
Every weapon represents a different class available in my server.

Here are some info about each class: [ 
[Engineer](https://github.com/fopeczek/teamworlds/edit/main/readme.md#Engineer) | 
[Spider](https://github.com/fopeczek/teamworlds/edit/main/readme.md#Spider) | 
[Scout](https://github.com/fopeczek/teamworlds/edit/main/readme.md#Scout) | 
[Tank](https://github.com/fopeczek/teamworlds/edit/main/readme.md#Tank) | 
[Hunter](https://github.com/fopeczek/teamworlds/edit/main/readme.md#Hunter) | 
[Medic](https://github.com/fopeczek/teamworlds/edit/main/readme.md#Medic) | 
[Armorer](https://github.com/fopeczek/teamworlds/edit/main/readme.md#Armorer)
 ]

----------

# Engineer
Engineer is a defensive class capable of building and maintaining defensive walls (force fields). It spawns with laser gun that can place those walls. 

How to place a wall:

https://user-images.githubusercontent.com/46483193/160856467-b97a966d-c65a-4f0d-ba28-de5414473040.mp4

Each player can have utmost 6 simultaneously active walls. 

Every wall has certain amount of hit points, just like a player. Amount of hp is represented by floating (unpickupable) hearts. Each wall can have up to 10 health. 

Newly placed wall can consume up to 5 units of laser gun ammo. Each consumed unit of ammo translates into a single hp of newly-built wall. Since the ammo capacity of laser gun equals 10 units, a player can place exactly 2 walls, each charged with 5 hp. Sfter that they will have no more laser gun ammo, and unless they pick up a refill or deconstruct the wall, they will not be able to place another one. 

Here is an example of placing a wall while having only 2 units of laser ammo:

https://user-images.githubusercontent.com/46483193/160859914-556015ea-b583-494b-805c-cb71195c371d.mp4

Player can charge the wall up to maximum number of 10 hit points. To do that, player has to hit one of the ends of the wall with hammer. Each strike will transfer a single armor point (a shield) into a hp of wall. When no more armor points are available, the strike will transfer a player's hitpoint (a heart) to the wall. Player cannot transfer their last hitpoint this way. 

https://user-images.githubusercontent.com/46483193/160863490-34caec9a-383b-4349-b3a2-ca219249ba85.mp4

As you can see you aren't able to die just by healing walls.

Player can deconstruct the wall and reclaim all the ammo and hitpoints back. To do that, player has to stand very close to one end of his wall, and shoot anywhare with the pistol. All the wall's hp above 5 will be transfered as health (preferably) or armor (if the health bar is already full). After that transfer, the remaining maximum 5 hitpoints will be transfered back as laser gun ammo. 

https://user-images.githubusercontent.com/46483193/160885179-366861b5-1f1c-4fc6-850f-98b0dd6f25d9.mp4

The algorithm allows to permanently loose wall's hp during deconstrcution, if there is no available place to transfer them back to player. In such situations the first shot will only aattempt to transfer the upper 5 hitpoints back as health and armor, if player has capacity to accept them, and the wall will continue to exist with the reduced hp. To remove the wall and lose part or all of its remaining hp, the player hsa to shoot the second time.

There are exctly 2 cases when you would have to confirm removing a wall:
1. To prevent loosing health or armor points, if the total available capacity of health and armor is less then wall's hp minus 5

https://user-images.githubusercontent.com/46483193/160885798-7d6c77b6-e952-4c2a-bb4e-5dc06c4d9e4b.mp4


2. To prevent loosing laser ammo, if your capacity to accept the laser ammo is less than wall's hp or 5, whichever is smaller. 

https://user-images.githubusercontent.com/46483193/160885837-396ca786-ab69-402d-984d-a84134a8666f.mp4



Walls block bullets and kill enemies on contact. Wall loses 1 hp when it is hit with a normal bullet, wall will lose aditional 1 hp if the bullet is explosive. 

Walls can't block lasers. 

When wall is hit with hammer it loses 3 hp. 

https://user-images.githubusercontent.com/46483193/160891224-809b44a6-df3a-4af5-8146-47d45a5edb75.mp4

Wall will get 2 times more damage if you shoot directly in one of its ends. 

https://user-images.githubusercontent.com/46483193/160893735-a77abd65-21a4-4547-959d-3e20c40f9470.mp4

When player rams a wall with full health bar the wall will kill him, and lose 1 hp. If player does so but with armor too wall will lose 2 hp. 

https://user-images.githubusercontent.com/46483193/160925533-9e447ebb-0c0c-41bc-9545-5516bc8c5079.mp4

When someone destroys your wall you will get a private message that informs you about who destroyed it. 
Every wall after being destroyed creates explosions at its ends. 

https://user-images.githubusercontent.com/46483193/160912584-e4137de1-b9c3-47a4-81b5-15d0d267c184.mp4

For each destroyed wall you (that who destroyed them) get 2 score points. 

https://user-images.githubusercontent.com/46483193/160926453-06ca53c7-e72c-4637-b49a-dd4157e19571.mp4

If you die, all your walls die too. 

https://user-images.githubusercontent.com/46483193/160896628-b513e3ae-ba4e-4670-a814-8afb6d6ea1f0.mp4


---------

# Spider
Spider special abitlity is placing webs using shotgun and hooking to metal walls. 

----------

# Scout
Scout is offensive class, his special abitlity is rocket boosting and jumping using granade launcher. 
He also spawns with granade launcher. 


He does only 1 hp of self damage. Tip: To rocket jump you have to fire underneath yourself and jump at the same time. 
Here is video of rocket boosting and jumping. 

https://user-images.githubusercontent.com/46483193/161442033-0d9c7057-7f9d-4bc5-9d24-fe7462b17448.mp4


Also his granade launcher makes more knock back to other players than other classes. And he does only 3 hp of damage to others. 

https://user-images.githubusercontent.com/46483193/161442031-0e1cfeeb-4e91-4d91-9066-a7496e6adf68.mp4


---------

# Tank
Tank offensive class that is slow and resistant. 

----------

# Hunter
Hunter special abitlity is turning invisable using ninja. 

----------

# Medic
Medic implementation is WIP(work in progress) by now it works as vanilla. 

----------

# Armorer
Armorer implementation is WIP(work in progress) by now it works as vanilla. 

----------
