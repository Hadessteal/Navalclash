# Milestone 5G — onboarding, HUD and player presentation

M5G is the final planned gameplay-presentation milestone before the full patched Luanti executable build and live integration pass.

## Guided onboarding

The persistent basic-training sequence advances through:

1. career enlistment;
2. faction selection;
3. opening the station console;
4. accepting a port contract;
5. boarding or commanding a vessel;
6. operating an assigned station.

The tutorial can be inspected, dismissed, resumed or reset without deleting career progress.

## Role-aware HUD

The HUD combines career, mission and vessel data and changes its station readout for:

- captain and executive officer;
- helm;
- engineering;
- gunnery;
- sensors;
- deck crew and passengers.

It reports vessel speed, heading, throttle, hull integrity, flooding, maintenance condition, mission progress and current alerts. Full, minimal and disabled modes are persistent per player.

## Integrated station console

`/station` opens a tabbed formspec with command, helm, engineering, weapons, sensors, missions and settings pages. Controls operate the same server-authoritative vessel state used by chat commands and enforce crew-station permissions for helm inputs.

New world nodes provide the same interface:

- integrated bridge console;
- naval training console;
- portable service handbook.

## Accessibility

Persistent player settings include:

- high-contrast presentation;
- larger HUD text;
- colour-vision palettes for deuteranopia, protanopia and tritanopia;
- subtitles;
- reduced-motion preference;
- alert filtering;
- tutorial enable/disable.

## Alerts

The server owns a deduplicated alert queue with info, success, warning and critical severity. Alerts appear on the HUD and, when subtitles are enabled, in chat. The last forty messages are retained in a per-session notification log.

## Authority and protocol

M5G changes no native ABI. The moving-construct protocol remains at M4N capability 11. Station controls, tutorial state, presentation preferences and alerts are server-side Lua gameplay systems.

## Remaining release gate

The codebase still requires a complete patched Luanti 5.16.1 executable build, live client/server integration, multiplayer stress testing, soak testing and performance optimisation before commercial release packaging.
