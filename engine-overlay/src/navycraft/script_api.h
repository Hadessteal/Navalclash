// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "script/lua_api/l_base.h"

class ModApiNavyCraft final : public ModApiBase {
private:
    static int l_create_dynamic_construct(lua_State *L);
    static int l_get_dynamic_construct(lua_State *L);
    static int l_set_dynamic_construct_transform(lua_State *L);
    static int l_set_dynamic_construct_velocity(lua_State *L);
    static int l_remove_dynamic_construct(lua_State *L);
    static int l_list_dynamic_constructs(lua_State *L);
    static int l_raycast_dynamic_constructs(lua_State *L);
    static int l_get_dynamic_construct_surface_velocity(lua_State *L);
    static int l_set_dynamic_construct_node(lua_State *L);
    static int l_get_dynamic_construct_sections(lua_State *L);
    static int l_get_dynamic_construct_protocol_version(lua_State *L);
    static int l_get_dynamic_construct_node_state(lua_State *L);
    static int l_set_dynamic_construct_metadata(lua_State *L);
    static int l_set_dynamic_construct_inventory(lua_State *L);
    static int l_start_dynamic_construct_timer(lua_State *L);
    static int l_stop_dynamic_construct_timer(lua_State *L);
    static int l_poll_dynamic_construct_events(lua_State *L);
    static int l_resolve_dynamic_construct_timer(lua_State *L);
    static int l_resolve_dynamic_construct_mutation(lua_State *L);
    static int l_initialise_dynamic_construct_persistence(lua_State *L);
    static int l_sync_dynamic_construct_persistence(lua_State *L);
    static int l_get_dynamic_construct_actions(lua_State *L);
    static int l_rollback_dynamic_construct_action(lua_State *L);
    static int l_dynamic_construct_local_to_world(lua_State *L);
    static int l_emit_dynamic_construct_effect(lua_State *L);
    static int l_spawn_dynamic_construct_projectile(lua_State *L);
    static int l_step_dynamic_construct_projectiles(lua_State *L);
    static int l_get_dynamic_construct_projectiles(lua_State *L);
    static int l_impact_dynamic_construct_projectile(lua_State *L);
    static int l_observe_dynamic_construct_target(lua_State *L);
    static int l_solve_dynamic_construct_fire_control(lua_State *L);
    static int l_configure_dynamic_construct_battery(lua_State *L);
    static int l_remove_dynamic_construct_battery(lua_State *L);
    static int l_step_dynamic_construct_fire_control(lua_State *L);
    static int l_get_dynamic_construct_fire_control_tracks(lua_State *L);
    static int l_get_dynamic_construct_fire_control_batteries(lua_State *L);
    static int l_configure_dynamic_construct_navigation(lua_State *L);
    static int l_set_dynamic_construct_route(lua_State *L);
    static int l_observe_dynamic_construct_obstacle(lua_State *L);
    static int l_step_dynamic_construct_navigation(lua_State *L);
    static int l_get_dynamic_construct_navigation(lua_State *L);
    static int l_set_dynamic_construct_navigation_enabled(lua_State *L);
    static int l_clear_dynamic_construct_navigation(lua_State *L);
    static int l_step_dynamic_construct_structure(lua_State *L);
    static int l_get_dynamic_construct_structure(lua_State *L);
    static int l_force_dynamic_construct_split(lua_State *L);
    static int l_set_dynamic_construct_flooding(lua_State *L);
    static int l_configure_dynamic_construct_joint(lua_State *L);
    static int l_set_dynamic_construct_joint(lua_State *L);
    static int l_remove_dynamic_construct_joint(lua_State *L);
    static int l_configure_dynamic_construct_turret(lua_State *L);
    static int l_aim_dynamic_construct_turret(lua_State *L);
    static int l_clear_dynamic_construct_turret_target(lua_State *L);
    static int l_get_dynamic_construct_articulations(lua_State *L);
    static int l_step_dynamic_construct_articulations(lua_State *L);
    static int l_dynamic_construct_line_of_fire(lua_State *L);
    static int l_configure_dynamic_construct_liquid_compartment(lua_State *L);
    static int l_configure_dynamic_construct_liquid_port(lua_State *L);
    static int l_set_dynamic_construct_liquid(lua_State *L);
    static int l_step_dynamic_construct_liquids(lua_State *L);
    static int l_get_dynamic_construct_liquids(lua_State *L);
    static int l_register_dynamic_construct_special_node(lua_State *L);

public:
    static void Initialize(lua_State *L, int top);
};
