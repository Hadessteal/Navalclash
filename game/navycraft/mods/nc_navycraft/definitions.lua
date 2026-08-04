-- Source-derived definitions from NavyCraft 1.1.1-iizDevBuild and
-- NavyCraft-Shipyard 3.1.5. This file contains data only; implementation is
-- original Luanti code.
local D = {}

D.source = {
    navycraft_version = "1.1.1-iizDevBuild",
    shipyard_version = "3.1.5",
    craft_release_delay = 15,
    ship_teleport_cooldown = 600,
    weapon_timeout = 4,
    pump_charge_limit = 20,
    hyperspace_move_multiplier = 16,
    scuttle_delay = 180,
    block_disp_value = 1.0,
    air_disp_value = 15.0,
    minimum_disp_value = 0.33,
    weight_multiplier = 1.0,
}

D.craft_order = {"boat", "ship", "freeship", "halfship", "aircraft", "airship", "submarine", "tank"}
D.craft_types = {
    boat = {drive_command="sail", min_blocks=9, max_blocks=500, max_speed=4, can_navigate=true,
        can_fly=false, can_dive=false, can_dig=false, terrestrial=false, obeys_gravity=false,
        cruise=false, max_engine_speed=4, max_forward_gear=2, max_reverse_gear=-2,
        turn_radius=4, max_surface_speed=4, max_submerged_speed=3},
    ship = {drive_command="sail", min_blocks=9, max_blocks=18000, max_speed=6, can_navigate=true,
        can_fly=false, can_dive=false, can_dig=false, terrestrial=false, obeys_gravity=false,
        cruise=true, max_engine_speed=8, max_forward_gear=2, max_reverse_gear=-2,
        turn_radius=4, max_surface_speed=8, max_submerged_speed=3},
    freeship = {drive_command="sail", min_blocks=9, max_blocks=3000, max_speed=6, can_navigate=true,
        can_fly=false, can_dive=false, can_dig=false, terrestrial=false, obeys_gravity=false,
        cruise=true, max_engine_speed=8, max_forward_gear=3, max_reverse_gear=-2,
        turn_radius=4, max_surface_speed=8, max_submerged_speed=3, discount=100, admin_build=true},
    halfship = {drive_command="sail", min_blocks=9, max_blocks=3000, max_speed=6, can_navigate=true,
        can_fly=false, can_dive=false, can_dig=false, terrestrial=false, obeys_gravity=false,
        cruise=true, max_engine_speed=8, max_forward_gear=2, max_reverse_gear=-2,
        turn_radius=4, max_surface_speed=8, max_submerged_speed=3, discount=50, admin_build=true},
    aircraft = {drive_command="pilot", min_blocks=20, max_blocks=18000, max_speed=20, can_navigate=false,
        can_fly=true, can_dive=false, can_dig=false, terrestrial=false, obeys_gravity=false,
        cruise=true, max_engine_speed=8, max_forward_gear=3, max_reverse_gear=-2,
        turn_radius=4, max_surface_speed=8, max_submerged_speed=3},
    airship = {drive_command="pilot", min_blocks=9, max_blocks=1000, max_speed=6, can_navigate=false,
        can_fly=true, can_dive=false, can_dig=false, terrestrial=false, obeys_gravity=false,
        cruise=false, max_engine_speed=4, max_forward_gear=2, max_reverse_gear=-2,
        turn_radius=4, max_surface_speed=4, max_submerged_speed=3},
    submarine = {drive_command="dive", min_blocks=20, max_blocks=18000, max_speed=3, can_navigate=false,
        can_fly=false, can_dive=true, can_dig=false, terrestrial=false, obeys_gravity=false,
        cruise=true, max_engine_speed=6, max_forward_gear=2, max_reverse_gear=-2,
        turn_radius=4, max_surface_speed=6, max_submerged_speed=3},
    tank = {drive_command="drive", min_blocks=10, max_blocks=2000, max_speed=3, can_navigate=false,
        can_fly=false, can_dive=false, can_dig=false, terrestrial=true, obeys_gravity=true,
        cruise=true, max_engine_speed=4, max_forward_gear=3, max_reverse_gear=-2,
        turn_radius=4, max_surface_speed=4, max_submerged_speed=3},
}

D.engine_order = {
    "diesel_1", "motor_1", "diesel_2", "boiler_1", "diesel_3",
    "gasoline_1", "boiler_2", "boiler_3", "gasoline_2", "nuclear",
    "airplane_1", "airplane_2", "airplane_3", "airplane_4", "airplane_7",
    "airplane_5", "airplane_6", "airplane_8", "tank_1", "tank_2",
}
D.engines = {
    diesel_1={display="Diesel 1",id=0,max_speed=6,power=800,class="sub",cost=100},
    motor_1={display="Motor 1",id=1,max_speed=6,power=500,class="sub",cost=150},
    diesel_2={display="Diesel 2",id=2,max_speed=8,power=1400,class="sub",cost=250},
    boiler_1={display="Boiler 1",id=3,max_speed=8,power=1800,class="ship",cost=250},
    diesel_3={display="Diesel 3",id=4,max_speed=10,power=2500,class="sub",cost=1000},
    gasoline_1={display="Gasoline 1",id=5,max_speed=10,power=500,class="ship",cost=50},
    boiler_2={display="Boiler 2",id=6,max_speed=10,power=2000,class="ship",cost=600},
    boiler_3={display="Boiler 3",id=7,max_speed=10,power=4000,class="ship",cost=1250},
    gasoline_2={display="Gasoline 2",id=8,max_speed=12,power=700,class="ship",cost=100},
    nuclear={display="Nuclear",id=9,max_speed=14,power=6000,class="ship",cost=10000},
    airplane_1={display="Airplane 1",id=10,max_speed=14,power=140,class="aircraft",cost=50},
    airplane_2={display="Airplane 2",id=11,max_speed=16,power=140,class="aircraft",cost=80},
    airplane_3={display="Airplane 3",id=12,max_speed=16,power=160,class="aircraft",cost=120},
    airplane_4={display="Airplane 4",id=13,max_speed=18,power=160,class="aircraft",cost=160},
    airplane_7={display="Airplane 7",id=14,max_speed=18,power=300,class="aircraft",cost=500},
    airplane_5={display="Airplane 5",id=15,max_speed=24,power=180,class="aircraft",cost=400},
    airplane_6={display="Airplane 6",id=16,max_speed=24,power=240,class="aircraft",cost=500},
    airplane_8={display="Airplane 8",id=17,max_speed=28,power=500,class="aircraft",cost=5000},
    tank_1={display="Tank 1",id=18,max_speed=4,power=200,class="tank",cost=50},
    tank_2={display="Tank 2",id=19,max_speed=8,power=400,class="tank",cost=5000},
}
for key, engine in pairs(D.engines) do engine.key=key end

D.components = {
    helm="Helm", nav="Navigation Control", periscope="Periscope", subdrive="Submarine Drive Selector",
    ballast="Ballast Tanks", buoyancy="Buoyancy Indicator", firecontrol="Fire Control",
    tdc="Torpedo Data Computer", radar="Radar", detector="Detector", sonar="Sonar",
    hydrophone="Hydrophone", passive_sonar="Passive Sonar", active_sonar="Active Sonar",
    hf_sonar="High-Frequency Sonar", aa_gun="AA Gun", launcher="Launcher", radio="Radio",
    pump="Pump", hyperdrive="Hyperdrive Control", telegraph="Engine Telegraph", rudder="Rudder Control",
    planes="Dive Plane Control", searchlight="Searchlight", target="Target Selector",
    tube="Torpedo Tube", cannon="Cannon", depth_dropper="Depth Charge Dropper",
    depth_launcher="Depth Charge Launcher Mk II", bomb_dropper="Bomb Dropper",
}

-- cannonType values and labels are copied from OneCannon.java.
D.weapon_order = {0,1,2,3,4,5,6,7,8,9}
D.weapons = {
    [0]={key="single_cannon",display="Single Cannon",kind="shell",count=1,blast=4,speed=34,range=200,ammo="cannon_shell"},
    [1]={key="double_cannon",display="Double Cannon",kind="shell",count=2,blast=4,speed=34,range=200,ammo="cannon_shell"},
    [2]={key="fireball_cannon",display="Fireball Cannon",kind="fireball",count=1,blast=5,speed=25,range=120,ammo="fireball_shell"},
    [3]={key="torpedo_mk2",display="Torpedo Mk II",kind="torpedo",count=1,blast=10,speed=7,range=350,ammo="torpedo_mk2",arming=5,guided=false},
    [4]={key="depth_charge_dropper",display="Depth-Charge Dropper",kind="depth_charge",count=1,blast=10,speed=0,range=80,ammo="depth_charge"},
    [5]={key="depth_charge_launcher_mk2",display="Depth-Charge Launcher Mk II",kind="depth_charge",count=3,blast=10,speed=6,range=120,ammo="depth_charge"},
    [6]={key="triple_cannon",display="Triple Cannon",kind="shell",count=3,blast=4,speed=34,range=200,ammo="cannon_shell"},
    [7]={key="torpedo_mk3",display="Torpedo Mk III",kind="torpedo",count=1,blast=14,speed=9,range=500,ammo="torpedo_mk3",arming=5,guided=true},
    [8]={key="torpedo_mk1",display="Torpedo Mk I",kind="torpedo",count=1,blast=7,speed=6,range=260,ammo="torpedo_mk1",arming=5,guided=false},
    [9]={key="bomb_dropper",display="Bomb Dropper",kind="bomb",count=1,blast=10,speed=0,range=100,ammo="bomb"},
}
D.weapon_by_key = {}
for id, weapon in pairs(D.weapons) do weapon.id=id; D.weapon_by_key[weapon.key]=weapon end

D.lot_order={"SHIP1","SHIP2","SHIP3","SHIP4","SHIP5","HANGAR1","HANGAR2","TANK1","TANK2"}
D.lot_aliases={DD="SHIP1",SUB1="SHIP2",SUB2="SHIP3",CL="SHIP4",CA="SHIP5"}
D.lots={
    SHIP1={display="Destroyer / DD",category="ship"}, SHIP2={display="Submarine 1 / SUB1",category="submarine"},
    SHIP3={display="Submarine 2 / SUB2",category="submarine"}, SHIP4={display="Cruiser / CL",category="ship"},
    SHIP5={display="Carrier / CA",category="ship"}, HANGAR1={display="Hangar 1",category="aircraft"},
    HANGAR2={display="Hangar 2",category="aircraft"}, TANK1={display="Tank 1",category="tank"},
    TANK2={display="Tank 2",category="tank"},
}

D.rank_defaults={
    {name="recruit",exp=0,pay=0}, {name="sailor",exp=100,pay=25},
    {name="petty_officer",exp=300,pay=50}, {name="officer",exp=750,pay=100},
    {name="captain",exp=1500,pay=200}, {name="admiral",exp=3000,pay=400},
}

return D
