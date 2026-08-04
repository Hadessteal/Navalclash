// SPDX-License-Identifier: LGPL-2.1-or-later
#include "carried_body.h"

#include <algorithm>
#include <cmath>

namespace navycraft {
namespace {
Aabb3d translatedToPosition(
    const Aabb3d &box, const Vec3d &old_position, const Vec3d &new_position) noexcept
{
    return box.translated(new_position - old_position);
}
}

CarriedBodyOutput CarriedBodyManager::step(
    const std::vector<const DynamicConstruct *> &constructs,
    const CarriedBodyInput &input) noexcept
{
    return step(constructs, nullptr, input);
}

CarriedBodyOutput CarriedBodyManager::step(
    const std::vector<const DynamicConstruct *> &constructs,
    const ConstructArticulationEngine *articulations,
    const CarriedBodyInput &input) noexcept
{
    CarriedBodyOutput output;
    output.position = input.position;
    output.velocity = input.velocity;
    if (input.id == 0 || !input.world_box.valid())
        return output;

    const double dt = std::isfinite(input.delta_seconds) ?
        std::max(0.0, input.delta_seconds) : 0.0;
    if (std::isfinite(input.current_time) && input.current_time >= 0.0)
        m_time = std::max(m_time, input.current_time);
    else
        m_time += dt;
    Record &record = m_records[input.id];
    record.last_seen_time = m_time;

    RiderMotionInput rider_input;
    rider_input.position = input.position;
    rider_input.velocity = input.velocity;
    rider_input.world_box = input.world_box;
    rider_input.jump_pressed = input.jump_pressed;
    rider_input.detach_requested = input.detach_requested;
    rider_input.inherit_platform_velocity_on_detach = true;
    rider_input.delta_seconds = dt;
    const RiderMotionOutput rider = RiderMotionSolver::step(
        record.rider, constructs, rider_input);

    output.position = rider.position;
    output.velocity = rider.velocity;
    output.displacement = rider.position - input.position;
    output.construct_id = rider.construct_id;
    output.grounded = rider.grounded;
    output.jumped = rider.jumped;

    if (articulations) {
        ArticulatedMotionInput articulated_input;
        articulated_input.position = output.position;
        articulated_input.velocity = output.velocity;
        articulated_input.world_box = translatedToPosition(
            input.world_box, input.position, output.position);
        articulated_input.jump_pressed = input.jump_pressed;
        articulated_input.detach_requested = input.detach_requested;
        articulated_input.delta_seconds = dt;
        const ArticulatedMotionOutput articulated =
            ConstructArticulatedMotion::stepRider(
                record.articulated_rider, *articulations, articulated_input);
        if (articulated.grounded || record.articulated_rider.contact.active) {
            output.position = articulated.position;
            output.velocity = articulated.velocity;
            output.displacement = output.position - input.position;
            output.construct_id = articulated.construct_id;
            output.articulation_id = articulated.articulation_id;
            output.grounded = articulated.grounded;
            output.jumped = output.jumped || articulated.jumped;
            if (articulated.contact_started)
                PlatformContact::clear(record.rider.contact);
        }
    }

    Aabb3d carried_box = translatedToPosition(
        input.world_box, input.position, output.position);

    // Resolve the body's own motion and any hull motion that entered the body.
    // On the first observation there is no reliable previous position, so only
    // depenetration is performed. Later steps use the observed frame delta.
    Vec3d desired_delta{};
    Aabb3d sweep_box = carried_box;
    if (record.have_last_position) {
        desired_delta = output.position - record.last_position;
        sweep_box = carried_box.translated(desired_delta * -1.0);
    }

    HullCollisionInput collision_input;
    collision_input.world_box = sweep_box;
    collision_input.velocity = output.velocity;
    collision_input.desired_delta = desired_delta;
    collision_input.delta_seconds = dt;
    collision_input.maximum_sweep_step =
        input.kind == CarriedBodyKind::DroppedItem ? 0.10 : 0.20;
    const HullCollisionOutput collision = MovingHullCollisionSolver::move(
        constructs, collision_input);

    HullCollisionOutput combined_collision = collision;
    if (articulations) {
        ArticulatedMotionInput articulated_collision_input;
        articulated_collision_input.position = record.have_last_position ?
            record.last_position : output.position;
        articulated_collision_input.velocity = collision.velocity;
        articulated_collision_input.world_box = record.have_last_position ?
            sweep_box.translated(collision.allowed_delta) :
            carried_box.translated(collision.position_correction);
        articulated_collision_input.desired_delta = {};
        articulated_collision_input.delta_seconds = dt;
        const ArticulatedMotionOutput articulated_collision =
            ConstructArticulatedMotion::move(*articulations,
                articulated_collision_input);
        combined_collision.position_correction += articulated_collision.allowed_delta;
        combined_collision.allowed_delta += articulated_collision.allowed_delta;
        combined_collision.velocity = articulated_collision.velocity;
        combined_collision.collided = combined_collision.collided ||
            articulated_collision.collided;
        combined_collision.hit_wall = combined_collision.hit_wall ||
            articulated_collision.hit_wall;
        combined_collision.hit_ceiling = combined_collision.hit_ceiling ||
            articulated_collision.hit_ceiling;
        combined_collision.touching_ground = combined_collision.touching_ground ||
            articulated_collision.grounded;
        combined_collision.contacts.insert(combined_collision.contacts.end(),
            articulated_collision.contacts.begin(), articulated_collision.contacts.end());
    }

    if (record.have_last_position) {
        output.position = record.last_position + combined_collision.allowed_delta;
        output.displacement = output.position - input.position;
    } else {
        output.position += combined_collision.position_correction;
        output.displacement += combined_collision.position_correction;
    }
    output.velocity = combined_collision.velocity;
    output.collided = combined_collision.collided;
    output.hit_wall = combined_collision.hit_wall;
    output.hit_ceiling = combined_collision.hit_ceiling;
    output.grounded = output.grounded || combined_collision.touching_ground;
    output.contacts = combined_collision.contacts;

    record.last_position = output.position;
    record.have_last_position = true;
    return output;
}

void CarriedBodyManager::remove(CarriedBodyId id) noexcept
{
    m_records.erase(id);
}

void CarriedBodyManager::clear() noexcept
{
    m_records.clear();
    m_time = 0.0;
}

void CarriedBodyManager::prune(double current_time, double maximum_age) noexcept
{
    maximum_age = std::max(0.0, maximum_age);
    for (auto iterator = m_records.begin(); iterator != m_records.end();) {
        if (current_time - iterator->second.last_seen_time > maximum_age)
            iterator = m_records.erase(iterator);
        else
            ++iterator;
    }
}

std::size_t CarriedBodyManager::size() const noexcept
{
    return m_records.size();
}

} // namespace navycraft
