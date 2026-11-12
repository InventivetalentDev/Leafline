#!/usr/bin/env python3
"""Deterministic expected-value economy model. Stdlib only; never reads/writes saves."""

import argparse
import heapq
import math
from pathlib import Path
import re
from dataclasses import dataclass

# Live inputs: DayDirector / ColonyTypes / AntMovementComponent / WindSystem.
SCOUTS = 3
HEIGHT_DAY_ONE_M = 20.0
HEIGHT_PER_DAY_M = 7.0
WALK_MPS = 2.0
SPRINT_MULTIPLIER = 2.0
RIDE_MPS = 6.0
CUT_RATE = 0.28
MASS_MIN, MASS_MAX = 1.0, 5.0
DIAMETER_MIN_CM, DIAMETER_MAX_CM = 30.0, 100.0
CARRY_LOSS_PER_MASS, MIN_CARRY_FACTOR = 0.08, 0.45
STAMINA_DRAIN, STAMINA_REGEN = 0.17, 0.22
PEAK_DAY_ONE, PEAK_PER_DAY = 0.22, 0.15
BODY_AREA, AREA_DIAMETER_CM, BASE_GRIP = 1.0, 40.0, 1.0
SHELTER_FACTOR = 0.25
CALM_MEAN, CALM_SHRINK, CALM_FLOOR = 12.0, 1.2, 4.0
WARNING_S, RISING_S, HOLD_MEAN_S, HOLD_GROW_S, FALLING_S = 3.0, 1.2, 3.25, 0.4, 2.0
EFFECTS = (
    (1, 1.25, 1.5, 1.8, 2.15, 2.55, 3),       # Mandibles
    (1, .75, .55, .4, .3, .22, .16),           # Strength
    (1, 1.25, 1.5, 1.8, 2.15, 2.55, 3),       # Grip
    (1, .8, .65, .5, .4, .32, .25),           # Stamina
    (0, 2, 3, 4, 6, 8, 10),                  # Brood: shared pool
)
NAMES = ("Mandibles", "Strength", "Grip", "Stamina", "Brood")
MAX_LEVEL = 6

# Implemented line-core inputs supplied by the main session.
TRIPS_PER_RECRUIT = 3
HARVEST_FACTOR = 5.0
RECRUIT_STAGGER_S = 1.5
RECRUIT_OUT_MPS = 3.4
RECRUIT_HOME_MULTIPLIER = 1.5
SKIP_HARVEST_FACTOR = 400.0
PATCH_MAX_LEAVES, PATCH_MAX_PIECES = 4, 10
LARGE_PIECE_SIZE = 0.6
SMALL_PIECES_PER_LEAF, LARGE_PIECES_PER_LEAF = 3, 2

# Guesses to replace with Leaf.Patch and route/playtest measurements.
LATERAL_ROUTE_M, GROUND_ROUTE_M, ROUTE_DETOUR = 25.0, 10.0, 1.2
SHARED_ROUTE_FRACTION = 0.55  # later scouts ride the shared trunk, then scout a new patch
SEARCH_PER_SCOUT_S, NEST_AND_RECAP_S, HANDOFF_S = 20.0, 22.0, 1.4
SPRINT_USE_FRACTION = 0.35   # fraction of sustainable sprint duty actually used by player
SIZE_DAY_ONE, SIZE_LEARNING_PER_DAY, SIZE_MAX = 0.40, 0.09, 0.90
RICHNESS_DAY_ONE, RICHNESS_PER_DAY, RICHNESS_MAX = 1.2, 0.25, 3.0
PATCH_LEAVES = (2, 3, 4)  # before the scout's cut; day-one capacities are 6 / 9 / 10
SHELTERED_ROUTE_SHARE = 0.60
EXPOSED_RANGE = (0.35, 1.0)  # uniform maximum exposure over the route; not a per-frame roll
SCOUT_LOSS_FACTOR = 0.75    # player avoids gusts; return still unattended
FALL_RECOVERY_EXTRA_S = 12.0  # expected extra time on a lost trip
TARGET_MINUTES, TARGET_WINDOW = 17.5, (15.0, 20.0)
HAUL_GROWTH_TARGET, HAUL_GROWTH_WINDOW = 10.0, (7.0, 14.0)
COST_CANDIDATES = (
    (4, 10, 24, 56, 128, 288),
    (6, 14, 30, 64, 136, 288),
    (8, 18, 40, 88, 192, 416),
    (10, 24, 52, 112, 240, 512),
    (12, 28, 60, 128, 272, 576),
    (14, 32, 68, 144, 304, 640),
)
MILESTONE_FRACTIONS, MILESTONE_ROUND = (0.1, 0.4, 1.0), 5
MIN_LINE_ADVANTAGE = 1.4
COMPARISON_DAY, COMPARISON_OTHER_LEVEL = 4, 1
WIND_COMPARISON_SIZES, WIND_COMPARISON_TRIP_S = (0.3, 0.9), 60.0
MAX_DAYS = 16
EPSILON = 1e-9


@dataclass
class Day:
    seconds: float
    haul: float
    recruit_pieces: int
    lost: float
    harvest: float
    remaining: int


def loss_probability(day, size, grip, trip_seconds, shelter_share=SHELTERED_ROUTE_SHARE):
    """Integrate force > hold over route classes, weighted by encountering a gust."""
    peak = PEAK_DAY_ONE * (1 + PEAK_PER_DAY * (day - 1))
    diameter = DIAMETER_MIN_CM + (DIAMETER_MAX_CM - DIAMETER_MIN_CM) * size
    force = peak * (BODY_AREA + (diameter / AREA_DIAMETER_CM) ** 2)
    threshold = BASE_GRIP * grip / force
    low, high = EXPOSED_RANGE
    exposed_loss = min(1.0, max(0.0, (high - threshold) / (high - low)))
    sheltered_loss = float(force * SHELTER_FACTOR > BASE_GRIP * grip)
    calm = max(CALM_FLOOR, CALM_MEAN - CALM_SHRINK * (day - 1))
    hold = HOLD_MEAN_S + HOLD_GROW_S * (day - 1)
    cycle = calm + WARNING_S + RISING_S + hold + FALLING_S
    encounter = min(1.0, (trip_seconds + RISING_S + hold) / cycle)
    return encounter * (shelter_share * sheltered_loss + (1 - shelter_share) * exposed_loss)


def simulate_day(day, levels, recruits, line_count=SCOUTS, harvest_factor=HARVEST_FACTOR):
    mandibles, strength, grip, stamina, brood = (EFFECTS[i][v] for i, v in enumerate(levels))
    pool = int(brood) if recruits else 0
    height = HEIGHT_DAY_ONE_M + HEIGHT_PER_DAY_M * (day - 1)
    route = (height + LATERAL_ROUTE_M) * ROUTE_DETOUR + GROUND_ROUTE_M
    size = min(SIZE_MAX, SIZE_DAY_ONE + SIZE_LEARNING_PER_DAY * (day - 1))
    richness = min(RICHNESS_MAX, RICHNESS_DAY_ONE + RICHNESS_PER_DAY * (day - 1))
    mass = MASS_MIN + (MASS_MAX - MASS_MIN) * size
    value = mass * richness  # richness increases value; only size affects burden/sail
    cut = size / (CUT_RATE * mandibles)
    laden_speed = WALK_MPS * max(MIN_CARRY_FACTOR, 1 - CARRY_LOSS_PER_MASS * strength * mass)
    home = route / laden_speed
    recruit_out = route / RECRUIT_OUT_MPS
    recruit_trip = recruit_out + cut + home / RECRUIT_HOME_MULTIPLIER
    recruit_loss = loss_probability(day, size, grip, recruit_trip)
    recruit_trip += FALL_RECOVERY_EXTRA_S * recruit_loss
    scout_loss = SCOUT_LOSS_FACTOR * loss_probability(day, size, grip, home)
    sprint_duty = STAMINA_REGEN / (STAMINA_REGEN + STAMINA_DRAIN * stamina)
    scout_speed = WALK_MPS * (1 + (SPRINT_MULTIPLIER - 1) * sprint_duty * SPRINT_USE_FRACTION)

    events = []
    serial = 0

    def event(time, kind, index=-1):
        nonlocal serial
        serial += 1
        heapq.heappush(events, (time, serial, kind, index))

    release = 0.0
    for scout in range(SCOUTS):
        outbound = route / scout_speed
        if scout:
            outbound = route * (SHARED_ROUTE_FRACTION / RIDE_MPS +
                                (1 - SHARED_ROUTE_FRACTION) / scout_speed)
        release += outbound + SEARCH_PER_SCOUT_S + cut
        event(release, "line", scout)
        event(release + home, "scout_home")
        release += HANDOFF_S
    last_send = release - HANDOFF_S
    patches = []
    assigned = []
    trips = [TRIPS_PER_RECRUIT] * pool
    busy = [False] * pool
    haul = lost = 0.0
    pieces = 0
    finish = 0.0
    last_recruit_home = 0.0
    next_departure = 0.0

    def dispatch(now):
        nonlocal next_departure
        for worker in range(pool):
            live = [p for p, left in enumerate(patches) if left > 0]
            if busy[worker] or trips[worker] == 0 or not live:
                continue
            chosen = max(live, key=lambda p: patches[p] / (1 + assigned[p]))
            # Reserve one piece, so concurrent workers cannot overdraw a finite patch.
            patches[chosen] -= 1
            assigned[chosen] += 1
            trips[worker] -= 1
            busy[worker] = True
            departure = max(now, next_departure)
            event(departure + recruit_trip, "recruit_home", worker)
            next_departure = departure + RECRUIT_STAGGER_S
            worker_patch[worker] = chosen

    worker_patch = [-1] * pool
    while events:
        now, _, kind, index = heapq.heappop(events)
        finish = max(finish, now)
        if kind == "line":
            if index < line_count:
                leaves = min(PATCH_MAX_LEAVES, PATCH_LEAVES[index % len(PATCH_LEAVES)])
                per_leaf = LARGE_PIECES_PER_LEAF if size > LARGE_PIECE_SIZE else SMALL_PIECES_PER_LEAF
                patches.append(min(PATCH_MAX_PIECES, leaves * per_leaf) - 1)
                assigned.append(0)
        elif kind == "scout_home":
            haul += value * (1 - scout_loss)
            lost += scout_loss
        elif kind == "recruit_home":
            last_recruit_home = now
            pieces += 1
            haul += value * (1 - recruit_loss)
            lost += recruit_loss
            assigned[worker_patch[index]] -= 1
            busy[index] = False
        dispatch(now)

    # The director waits for the last handoff. Recruits must still have work at that point.
    harvest_start = last_send + HANDOFF_S
    harvest = max(0.0, finish - harvest_start) if last_recruit_home > harvest_start else 0.0
    seconds = finish - harvest + harvest / harvest_factor + NEST_AND_RECAP_S
    assert pieces <= pool * TRIPS_PER_RECRUIT
    assert all(left >= 0 for left in patches)
    return Day(seconds, haul, pieces, lost, harvest, sum(patches))


def buy_upgrades(day, levels, balance, recruits, costs):
    """Buy greatest positive next-day haul/min gain per mass; reconsider after each buy."""
    purchases = []
    while True:
        baseline = simulate_day(day + 1, levels, recruits)
        baseline_rate = baseline.haul / baseline.seconds
        options = []
        for index, level in enumerate(levels):
            if level == MAX_LEVEL or costs[level] > balance or (index == 4 and not recruits):
                continue
            candidate = levels.copy()
            candidate[index] += 1
            result = simulate_day(day + 1, candidate, recruits)
            gain = (result.haul / result.seconds - baseline_rate) / costs[level]
            if gain > EPSILON:
                options.append((gain, -index, costs[level]))
        if not options:
            break
        _, negative_index, cost = max(options)
        index = -negative_index
        levels[index] += 1
        balance -= cost
        purchases.append(f"{NAMES[index]} {levels[index]} ({cost})")
    return balance, purchases


def campaign(recruits, costs):
    levels = [0] * len(NAMES)
    balance = total = seconds = 0.0
    rows = []
    for day in range(1, MAX_DAYS + 1):
        result = simulate_day(day, levels, recruits)
        total += result.haul
        balance += result.haul
        seconds += result.seconds
        before = levels.copy()
        balance, purchases = buy_upgrades(day, levels, balance, recruits, costs)
        rows.append((day, result, seconds, total, purchases, before))
    return rows


def end_row(rows):
    return min(rows, key=lambda row: abs(row[2] / 60 - TARGET_MINUTES))


def thresholds(row):
    # Round DOWN: the recommended threshold must be reached on this day.
    final = math.floor(row[3] / MILESTONE_ROUND) * MILESTONE_ROUND
    return tuple(max(MILESTONE_ROUND, math.floor(final * f / MILESTONE_ROUND) * MILESTONE_ROUND)
                 for f in MILESTONE_FRACTIONS)


def cpp_float(value):
    return f"{value:g}f" if value != int(value) else f"{int(value)}.f"


def check_cpp(costs, on, off):
    """Optional read-only check that the checked-in table matches this recommendation."""
    cpp = (Path(__file__).resolve().parents[1] / "Source/AntTreeGame/Game/Upgrades.cpp").read_text()
    expected = {"Costs": costs, "MilestonesWithRecruits": thresholds(end_row(on)),
                "MilestonesByHand": thresholds(end_row(off))}
    for name, values in expected.items():
        match = re.search(rf"\b{name}\[\]\s*=\s*\{{([^}}]+)\}}", cpp)
        assert match, f"Missing C++ array: {name}"
        actual = tuple(float(number.strip().removesuffix("f")) for number in match[1].split(","))
        assert actual == values, f"{name}: C++ {actual} differs from recommendation {values}"
    for name, effects in zip(NAMES, EFFECTS):
        match = re.search(rf'TEXT\("{name}"\).*?\{{([^}}]+)\}}', cpp)
        assert match, f"Missing C++ effects: {name}"
        actual = tuple(float(number.strip().removesuffix("f")) for number in match[1].split(","))
        assert actual == effects, f"{name}: C++ effects differ from simulation"
    print("C++ costs, both milestone sets, and all effect ladders match.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-cpp", action="store_true", help="also verify Upgrades.cpp matches the recommendation")
    args = parser.parse_args()
    candidates = []
    for costs in COST_CANDIDATES:
        on, off = campaign(True, costs), campaign(False, costs)
        end_on, end_off = end_row(on), end_row(off)
        growth = end_on[1].haul / on[0][1].haul
        score = abs(math.log(growth / HAUL_GROWTH_TARGET))
        score += abs(end_on[2] / 60 - TARGET_MINUTES) + abs(end_off[2] / 60 - TARGET_MINUTES)
        times_valid = all(TARGET_WINDOW[0] <= end[2] / 60 <= TARGET_WINDOW[1] for end in (end_on, end_off))
        if times_valid and HAUL_GROWTH_WINDOW[0] <= growth <= HAUL_GROWTH_WINDOW[1]:
            candidates.append((score, costs, on, off))
    assert candidates, "No cost candidate meets both pacing windows and the haul-growth window; retune the inputs."
    _, costs, on, off = min(candidates, key=lambda candidate: candidate[0])
    for label, rows in (("Recruits ON", on), ("By hand (recruits OFF)", off)):
        end = end_row(rows)
        milestones = thresholds(end)
        print(f"\n{label}; milestones {milestones}")
        print("day  day min  total min    haul   lifetime  pool  trips  lost  patch left  harvest s  purchases after recap")
        for day, result, seconds, total, purchases, levels in rows[:end[0]]:
            pool = int(EFFECTS[4][levels[4]]) if rows is on else 0
            print(f"{day:3}  {result.seconds / 60:7.2f}  {seconds / 60:9.2f}  {result.haul:6.1f}  "
                  f"{total:9.1f}  {pool:4}  {result.recruit_pieces:5}  {result.lost:4.1f}  "
                  f"{result.remaining:10}  {result.harvest:9.1f}  "
                  f"{(', '.join(purchases) or 'save balance') if day < end[0] else 'end card before buying'}")
        growth = end[1].haul / rows[0][1].haul
        print(f"End card: day {end[0]}, {end[2] / 60:.2f} min; day-haul growth {growth:.2f}x")
        assert TARGET_WINDOW[0] <= end[2] / 60 <= TARGET_WINDOW[1]
        assert rows[end[0] - 2][3] < milestones[-1] <= end[3]
        if rows is on:
            assert HAUL_GROWTH_WINDOW[0] <= growth <= HAUL_GROWTH_WINDOW[1]

    print(f"\nBrood comparison: day {COMPARISON_DAY}, same size/richness, other levels {COMPARISON_OTHER_LEVEL}; one versus three live lines")
    for brood in range(2, MAX_LEVEL + 1):
        levels = [COMPARISON_OTHER_LEVEL] * (len(NAMES) - 1) + [brood]
        one, three = (simulate_day(COMPARISON_DAY, levels, True, lines) for lines in (1, SCOUTS))
        ratio = three.haul / one.haul
        print(f"Brood {brood}: {one.haul:.1f} -> {three.haul:.1f} ({ratio:.2f}x); patch remaining {three.remaining}")
        assert ratio >= MIN_LINE_ADVANTAGE

    print(f"\nWind comparison: day {COMPARISON_DAY}, Grip 0, sheltered versus exposed route classes")
    for size in WIND_COMPARISON_SIZES:
        losses = [loss_probability(COMPARISON_DAY, size, EFFECTS[2][0], WIND_COMPARISON_TRIP_S, share)
                  for share in (1.0, 0.0)]
        print(f"Size {size}: sheltered {losses[0]:.1%}, exposed {losses[1]:.1%}")

    skipped_time = 0.0
    for day, watched, _, _, _, levels in on[:end_row(on)[0]]:
        skipped = simulate_day(day, levels, True, harvest_factor=SKIP_HARVEST_FACTOR)
        assert (skipped.haul, skipped.recruit_pieces, skipped.lost, skipped.remaining) == (
            watched.haul, watched.recruit_pieces, watched.lost, watched.remaining)
        skipped_time += skipped.seconds
    print(f"\nSkipped harvests: identical haul, trips, losses and patch remainder; end card {skipped_time / 60:.2f} min.")

    print("\nRecommended Upgrades.cpp numbers:")
    print("const int32 Costs[] = { " + ", ".join(map(str, costs)) + " };")
    for name, rows in (("MilestonesWithRecruits", on), ("MilestonesByHand", off)):
        print(f"const float {name}[] = {{ " + ", ".join(cpp_float(v) for v in thresholds(end_row(rows))) + " };")
    for name, effects in zip(NAMES, EFFECTS):
        print(f"{name}: {{ " + ", ".join(cpp_float(v) for v in effects) + " }")
    if args.check_cpp:
        check_cpp(costs, on, off)


if __name__ == "__main__":
    main()
