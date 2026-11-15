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
    (2, 3, 4, 6, 8, 10, 12),                 # Brood: shared pool (level 0: a new colony has two recruits)
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


@dataclass(frozen=True)
class Stage:
    height_multiplier: float
    leafy_height: float
    richness_multiplier: float
    gust_multiplier: float
    sparse_spacing: float
    sparse_blend_m: float


# FDayTuning::Stages defaults; tree geometry/density are represented by the supply below.
STAGES = (Stage(1.0, 20.0, 1.0, 1.0, 12.0, 55.0),
          Stage(1.4, 26.0, 1.3, 1.2, 8.0, 45.0),
          Stage(1.15, 32.0, 1.6, 1.4, 8.0, 45.0))
GUST_PER_DAY_ON_TREE = 0.06
BAND_HEIGHT_M = 10.0
# Observed first leaves; subsequent bands use their absolute lower edge.
FIRST_LEAF_HEIGHT_M = (32.4, 36.0, 33.0)
# Highest leaf per tree (LeafSet log lines), for the linear richness curve.
CROWN_TOP_M = (296.0, 337.0, 405.0)
# --richness height: richness from the cut height like FLeafSet::Build, 1 + 2 * H * (0.4 + 0.6 * Out) with the mean
# Out (0.5); H linear over the crown (RICHNESS_HEIGHT_SCALE_M 0) or 1 - exp(-dz / scale). Default "day": the old guess.
RICHNESS_MODE = "day"
RICHNESS_HEIGHT_SCALE_M = 0.0
OUT_MEAN_FACTOR = 0.4 + 0.6 * 0.5


def height_richness(stage, height):
    first = FIRST_LEAF_HEIGHT_M[stage]
    dz = max(0.0, height - first)
    if RICHNESS_HEIGHT_SCALE_M > 0:
        h = 1.0 - math.exp(-dz / RICHNESS_HEIGHT_SCALE_M)
    else:
        h = min(1.0, dz / max(1.0, CROWN_TOP_M[stage] - first))
    return 1.0 + 2.0 * h * OUT_MEAN_FACTOR
SPARSE_PATCH_LEAVES = (1, 2, 3)
EXPOSURE_SHARE_PER_METRE = 0.006
MIN_SHELTERED_SHARE = 0.25
MOVE_CARD_S = 4.0
ONE_TREE_FIRST_MOVE_DAY = 4


def load_tree_supply():
    """Read the measured template-0.5 bands, including packed lines and empty bands."""
    report = Path(__file__).with_name("tree_bands_measured.txt").read_text()
    supplies, origins = [], []
    for tree in range(1, len(STAGES) + 1):
        bands = {}
        whole = None
        for line in report.splitlines():
            if not line.startswith(f"tree {tree} day 1:"):
                continue
            for lo, hi, pieces in re.findall(r"(\d+)-(\d+) m: \d+/\d+ live, (\d+) pieces", line):
                lo, hi, pieces = int(lo), int(hi), int(pieces)
                assert lo % BAND_HEIGHT_M == 0 and hi - lo == BAND_HEIGHT_M
                assert lo not in bands, f"Duplicate band on tree {tree}: {lo} m"
                bands[lo] = pieces
            match = re.search(r"whole tree \d+/\d+ live, (\d+) pieces", line)
            if match:
                whole = int(match[1])
        assert bands and whole is not None, f"Missing fresh-tree report for tree {tree}"
        origin = min(bands)
        assert FIRST_LEAF_HEIGHT_M[tree - 1] >= origin
        assert FIRST_LEAF_HEIGHT_M[tree - 1] < origin + BAND_HEIGHT_M
        heights = range(origin, max(bands) + int(BAND_HEIGHT_M), int(BAND_HEIGHT_M))
        assert all(h in bands for h in heights), f"Missing measured band on tree {tree}"
        supply = tuple(bands[h] for h in heights)
        assert sum(supply) == whole, f"Band sum differs from whole-tree report for tree {tree}"
        supplies.append(supply)
        origins.append(origin)
    return tuple(supplies), tuple(origins)


TREE_SUPPLY, TREE_BAND_ORIGINS_M = load_tree_supply()


def band_height(stage, band):
    stage = min(stage, len(STAGES) - 1)
    return max(FIRST_LEAF_HEIGHT_M[stage], TREE_BAND_ORIGINS_M[stage] + BAND_HEIGHT_M * band)


@dataclass(frozen=True)
class TreeState:
    stage: int
    day_on_tree: int
    remaining: tuple

    @classmethod
    def fresh(cls, stage=0):
        return cls(stage, 1, TREE_SUPPLY[min(stage, len(TREE_SUPPLY) - 1)])


@dataclass
class Day:
    seconds: float
    haul: float
    recruit_pieces: int
    lost: float
    harvest: float
    remaining: int
    tree_remaining: tuple = ()
    mean_cut_height: float = 0.0
    lowest_live_height: float = 0.0


@dataclass(frozen=True)
class Trip:
    route: float
    capacity: int
    band: int
    value: float
    home: float
    recruit_seconds: float
    recruit_loss: float
    scout_loss: float


def loss_probability(day, size, grip, trip_seconds, shelter_share=SHELTERED_ROUTE_SHARE,
                     *, peak=None):
    """Integrate force > hold over route classes, weighted by encountering a gust."""
    if peak is None:
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


def simulate_day(day, levels, recruits, line_count=SCOUTS, harvest_factor=HARVEST_FACTOR,
                 *, tree=None):
    mandibles, strength, grip, stamina, brood = (EFFECTS[i][v] for i, v in enumerate(levels))
    pool = int(brood) if recruits else 0
    height = HEIGHT_DAY_ONE_M + HEIGHT_PER_DAY_M * (day - 1)
    size = min(SIZE_MAX, SIZE_DAY_ONE + SIZE_LEARNING_PER_DAY * (day - 1))
    richness = min(RICHNESS_MAX, RICHNESS_DAY_ONE + RICHNESS_PER_DAY * (day - 1))
    mass = MASS_MIN + (MASS_MAX - MASS_MIN) * size
    cut = size / (CUT_RATE * mandibles)
    laden_speed = WALK_MPS * max(MIN_CARRY_FACTOR, 1 - CARRY_LOSS_PER_MASS * strength * mass)
    sprint_duty = STAMINA_REGEN / (STAMINA_REGEN + STAMINA_DRAIN * stamina)
    scout_speed = WALK_MPS * (1 + (SPRINT_MULTIPLIER - 1) * sprint_duty * SPRINT_USE_FRACTION)
    per_leaf = LARGE_PIECES_PER_LEAF if size > LARGE_PIECE_SIZE else SMALL_PIECES_PER_LEAF
    # Preserve the old three-small/two-large approximation in template-0.5 units.
    piece_use = SMALL_PIECES_PER_LEAF / per_leaf
    available = list(tree.remaining) if tree else []
    consumed = [0] * len(available)
    plans = []
    for scout in range(SCOUTS):
        leaves = min(PATCH_MAX_LEAVES, PATCH_LEAVES[scout % len(PATCH_LEAVES)])
        capacity = min(PATCH_MAX_PIECES, leaves * per_leaf)
        band = -1
        shelter, wind_day, peak = SHELTERED_ROUTE_SHARE, day, None
        if tree:
            stage = STAGES[min(tree.stage, len(STAGES) - 1)]
            # Distinct scout patches reserve their full capacity for this day only.
            band = next((i for i, left in enumerate(available) if left + EPSILON >= piece_use), -1)
            assert band >= 0, "Tree exhausted: no scout piece fits; extend the measured supply table."
            height = band_height(tree.stage, band)
            if height < stage.leafy_height + stage.sparse_blend_m:
                leaves = SPARSE_PATCH_LEAVES[scout % len(SPARSE_PATCH_LEAVES)]
                capacity = min(PATCH_MAX_PIECES, leaves * per_leaf)
            capacity = min(capacity, math.floor((available[band] + EPSILON) / piece_use))
            available[band] -= capacity * piece_use
            shelter = max(MIN_SHELTERED_SHARE, SHELTERED_ROUTE_SHARE -
                          EXPOSURE_SHARE_PER_METRE * (height - HEIGHT_DAY_ONE_M))
            wind_day = tree.day_on_tree
            peak = PEAK_DAY_ONE * stage.gust_multiplier * (1 + GUST_PER_DAY_ON_TREE * (wind_day - 1))
        route = (height + LATERAL_ROUTE_M) * ROUTE_DETOUR + GROUND_ROUTE_M
        rich = height_richness(tree.stage, height) if (tree and RICHNESS_MODE == "height") else richness
        value = mass * rich * (stage.richness_multiplier if tree else 1.0)
        home = route / laden_speed
        recruit_trip = route / RECRUIT_OUT_MPS + cut + home / RECRUIT_HOME_MULTIPLIER
        recruit_loss = loss_probability(wind_day, size, grip, recruit_trip, shelter, peak=peak)
        recruit_trip += FALL_RECOVERY_EXTRA_S * recruit_loss
        scout_loss = SCOUT_LOSS_FACTOR * loss_probability(wind_day, size, grip, home, shelter, peak=peak)
        plans.append(Trip(route, capacity, band, value, home,
                          recruit_trip, recruit_loss, scout_loss))

    events = []
    serial = 0

    def event(time, kind, index=-1):
        nonlocal serial
        serial += 1
        heapq.heappush(events, (time, serial, kind, index))

    release = 0.0
    for scout, plan in enumerate(plans):
        outbound = plan.route / scout_speed
        if scout or (tree and tree.day_on_tree > 1):
            outbound = plan.route * (SHARED_ROUTE_FRACTION / RIDE_MPS +
                                     (1 - SHARED_ROUTE_FRACTION) / scout_speed)
        release += outbound + SEARCH_PER_SCOUT_S + cut
        event(release, "line", scout)
        event(release + plan.home, "scout_home", scout)
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
            event(departure + plans[chosen].recruit_seconds, "recruit_home", worker)
            next_departure = departure + RECRUIT_STAGGER_S
            worker_patch[worker] = chosen

    worker_patch = [-1] * pool
    while events:
        now, _, kind, index = heapq.heappop(events)
        finish = max(finish, now)
        if kind == "line":
            if tree:
                consumed[plans[index].band] += 1
            if index < line_count:
                patches.append(plans[index].capacity - 1)
                assigned.append(0)
        elif kind == "scout_home":
            plan = plans[index]
            haul += plan.value * (1 - plan.scout_loss)
            lost += plan.scout_loss
        elif kind == "recruit_home":
            last_recruit_home = now
            pieces += 1
            plan = plans[worker_patch[index]]
            haul += plan.value * (1 - plan.recruit_loss)
            lost += plan.recruit_loss
            if tree:
                consumed[plan.band] += 1
            assigned[worker_patch[index]] -= 1
            busy[index] = False
        dispatch(now)

    # The director waits for the last handoff. Recruits must still have work at that point.
    harvest_start = last_send + HANDOFF_S
    harvest = max(0.0, finish - harvest_start) if last_recruit_home > harvest_start else 0.0
    seconds = finish - harvest + harvest / harvest_factor + NEST_AND_RECAP_S
    assert pieces <= pool * TRIPS_PER_RECRUIT
    assert all(left >= 0 for left in patches)
    remaining, mean_height, lowest = (), 0.0, 0.0
    if tree:
        remaining = tuple(left - used * piece_use for left, used in zip(tree.remaining, consumed))
        assert all(left >= -EPSILON for left in remaining)
        assert sum(consumed) == SCOUTS + pieces
        assert math.isclose(sum(tree.remaining) - sum(remaining), (SCOUTS + pieces) * piece_use)
        mean_height = sum(band_height(tree.stage, i) * used
                          for i, used in enumerate(consumed)) / sum(consumed)
        lowest = next((band_height(tree.stage, i) for i, left in enumerate(remaining)
                       if left + EPSILON >= piece_use), math.inf)
    return Day(seconds, haul, pieces, lost, harvest, sum(patches), remaining, mean_height, lowest)


def buy_upgrades(day, levels, balance, recruits, costs, *, tree=None):
    """Buy greatest positive next-day haul/min gain per mass; reconsider after each buy."""
    purchases = []
    while True:
        baseline = simulate_day(day + 1, levels, recruits, tree=tree)
        baseline_rate = baseline.haul / baseline.seconds
        options = []
        for index, level in enumerate(levels):
            if level == MAX_LEVEL or costs[level] > balance or (index == 4 and not recruits):
                continue
            candidate = levels.copy()
            candidate[index] += 1
            result = simulate_day(day + 1, candidate, recruits, tree=tree)
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


@dataclass
class TreeDay:
    day: int
    tree: TreeState
    result: Day
    seconds: float
    total: float
    purchases: list
    levels: list


def next_tree(tree, result, total, milestones):
    """Apply at most one pending move, at the next dawn, including after the end card."""
    due = sum(total >= threshold for threshold in milestones[1:])
    if tree.stage < due:
        return TreeState.fresh(tree.stage + 1)
    return TreeState(tree.stage, tree.day_on_tree + 1, result.tree_remaining)


def campaign_one_tree(recruits, costs, milestones, *, max_days=MAX_DAYS,
                      harvest_factor=HARVEST_FACTOR, stop_at_end=True):
    levels = [0] * len(NAMES)
    balance = total = seconds = 0.0
    tree = TreeState.fresh()
    rows = []
    for day in range(1, max_days + 1):
        result = simulate_day(day, levels, recruits, harvest_factor=harvest_factor, tree=tree)
        if tree.stage > 0 and tree.day_on_tree == 1:
            result.seconds += MOVE_CARD_S
        total += result.haul
        balance += result.haul
        seconds += result.seconds
        before = levels.copy()
        tomorrow = next_tree(tree, result, total, milestones)
        ended = total >= milestones[-1]
        purchases = []
        if not (ended and stop_at_end):
            balance, purchases = buy_upgrades(day, levels, balance, recruits, costs, tree=tomorrow)
        rows.append(TreeDay(day, tree, result, seconds, total, purchases, before))
        if ended and stop_at_end:
            break
        tree = tomorrow
    return rows


def recommend_one_tree(recruits, costs):
    """Retain costs; choose the second milestone for day four, then the end near 17.5 min."""
    no_moves = (MILESTONE_ROUND, math.inf, math.inf)
    warmup = campaign_one_tree(recruits, costs, no_moves, max_days=ONE_TREE_FIRST_MOVE_DAY)
    middle = math.floor(warmup[-1].total / MILESTONE_ROUND) * MILESTONE_ROUND
    assert warmup[-2].total < middle <= warmup[-1].total
    trial = campaign_one_tree(recruits, costs, (MILESTONE_ROUND, middle, math.inf))
    options = [row for row in trial if row.tree.stage == 1 and
               TARGET_WINDOW[0] <= row.seconds / 60 <= TARGET_WINDOW[1]]
    assert options, "No one-tree end day in the pacing window; retune stage assumptions."
    end = min(options, key=lambda row: abs(row.seconds / 60 - TARGET_MINUTES))
    final = math.floor(end.total / MILESTONE_ROUND) * MILESTONE_ROUND
    first = max(MILESTONE_ROUND, math.floor(final * MILESTONE_FRACTIONS[0] /
                                           MILESTONE_ROUND) * MILESTONE_ROUND)
    milestones = (first, middle, final)
    assert first < middle < final
    rows = campaign_one_tree(recruits, costs, milestones)
    assert rows[-1].day == end.day and rows[-2].total < final <= rows[-1].total
    assert sum(row.tree.stage == 0 for row in rows) == ONE_TREE_FIRST_MOVE_DAY
    assert TARGET_WINDOW[0] <= rows[-1].seconds / 60 <= TARGET_WINDOW[1]
    return milestones, rows


def report_one_tree(recruits, costs):
    milestones, rows = recommend_one_tree(recruits, costs)
    print(f"\nOne tree ON, recruits {'ON' if recruits else 'OFF'}; milestones {milestones}")
    print("day  tree/day   min   total min    haul  lifetime  cut m  lowest left m  low bands left  purchases")
    for row in rows:
        r = row.result
        print(f"{row.day:3}  {row.tree.stage + 1:3}/{row.tree.day_on_tree:<3}  {r.seconds / 60:5.2f}  "
              f"{row.seconds / 60:9.2f}  {r.haul:6.1f}  {row.total:8.1f}  {r.mean_cut_height:5.1f}  "
              f"{r.lowest_live_height:11.1f}  {str(r.tree_remaining[:2]):>14}  "
              f"{', '.join(row.purchases) or ('end card' if row is rows[-1] else 'save balance')}")
    days = [sum(row.tree.stage == stage for row in rows) for stage in range(3)]
    print(f"Days per tree to end card: {days}; end day {rows[-1].day}, {rows[-1].seconds / 60:.2f} min. "
          f"Tree 3 starts day {rows[-1].day + 1} on continue.")
    skipped = campaign_one_tree(recruits, costs, milestones, harvest_factor=SKIP_HARVEST_FACTOR)
    # Upgrade forecasts use watched timing in both cases, matching the same player choices.
    assert len(skipped) == len(rows)
    for watched, fast in zip(rows, skipped):
        assert (watched.tree, watched.result.haul, watched.result.recruit_pieces,
                watched.result.lost, watched.result.tree_remaining, watched.levels) == (
                    fast.tree, fast.result.haul, fast.result.recruit_pieces,
                    fast.result.lost, fast.result.tree_remaining, fast.levels)
    print(f"Skipped harvest end card: {skipped[-1].seconds / 60:.2f} min; identical cuts, losses, supply and haul.")

    # Exercise the pending third move and continued play, not just threshold selection.
    continuation = campaign_one_tree(recruits, costs, milestones, max_days=rows[-1].day + 2,
                                     stop_at_end=False)
    assert continuation[rows[-1].day].tree == TreeState.fresh(2)
    assert continuation[-1].tree.stage == 2 and continuation[-1].tree.day_on_tree == 2
    # Crossing both move thresholds on one recap must still spend one day on tree 2.
    fresh = TreeState.fresh()
    first = simulate_day(1, [0] * len(NAMES), recruits, tree=fresh)
    second = next_tree(fresh, first, milestones[-1] + 1, milestones)
    assert second == TreeState.fresh(1)
    second_result = simulate_day(2, [0] * len(NAMES), recruits, tree=second)
    assert next_tree(second, second_result, milestones[-1] + 1, milestones) == TreeState.fresh(2)

    print("Three-line check on sparse low and higher bands (Brood 2..6, other upgrades 1):")
    for band in (0, 2):
        supply = tuple(0 if i < band else left for i, left in enumerate(TREE_SUPPLY[0]))
        ratios = []
        for brood in range(2, MAX_LEVEL + 1):
            levels = [COMPARISON_OTHER_LEVEL] * (len(NAMES) - 1) + [brood]
            state = TreeState(0, COMPARISON_DAY, supply)
            one, three = (simulate_day(COMPARISON_DAY, levels, True, lines, tree=state)
                          for lines in (1, SCOUTS))
            ratios.append(three.haul / one.haul)
            assert ratios[-1] >= MIN_LINE_ADVANTAGE
        print(f"  lowest band {band} ({band_height(0, band):g} m): "
              + ", ".join(f"{ratio:.2f}x" for ratio in ratios))
    return milestones, rows


def cpp_float(value):
    return f"{value:g}f" if value != int(value) else f"{int(value)}.f"


def check_cpp(costs, on, off, tree_on, tree_off):
    """Optional read-only check that the checked-in table matches this recommendation."""
    cpp = (Path(__file__).resolve().parents[1] / "Source/AntTreeGame/Game/Upgrades.cpp").read_text()
    expected = {"Costs": costs, "MilestonesWithRecruits": thresholds(end_row(on)),
                "MilestonesByHand": thresholds(end_row(off)),
                "MilestonesOneTreeWithRecruits": tree_on,
                "MilestonesOneTreeByHand": tree_off}
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
    tuning = (Path(__file__).resolve().parents[1] / "Source/AntTreeGame/Colony/ColonyTypes.h").read_text()
    stages = re.search(r"\bStages\s*=\s*\{(.*?)\};", tuning, re.S)
    assert stages, "Missing C++ stage defaults"
    actual = tuple(tuple(float(v.strip().removesuffix("f")) for v in values.split(","))
                   for values in re.findall(r"FStageTuning\(([^)]+)\)", stages[1]))
    expected = tuple((s.height_multiplier, s.leafy_height * 100, s.richness_multiplier,
                      s.gust_multiplier, s.sparse_spacing, s.sparse_blend_m * 100) for s in STAGES)
    assert actual == expected, f"C++ stage defaults {actual} differ from simulation {expected}"
    gust = re.search(r"\bGustPerDayOnTree\s*=\s*([\d.]+)f", tuning)
    assert gust and float(gust[1]) == GUST_PER_DAY_ON_TREE, "C++ within-tree gust rise differs"
    print("C++ costs, all four milestone sets, effect ladders, stage defaults and gust rise match.")


def report_one_tree_fixed(milestones, costs):
    """One tree, recruits on, the given milestones: the day table, end day and minutes."""
    rows = campaign_one_tree(True, costs, milestones)
    print(f"\nOne tree ON, recruits ON; milestones {milestones} (given); richness {RICHNESS_MODE}"
          + (f", height scale {RICHNESS_HEIGHT_SCALE_M:g} m" if RICHNESS_MODE == "height" else ""))
    print("day  tree/day   min   total min    haul  lifetime  cut m  purchases")
    for row in rows:
        r = row.result
        print(f"{row.day:3}  {row.tree.stage + 1:3}/{row.tree.day_on_tree:<3}  {r.seconds / 60:5.2f}  "
              f"{row.seconds / 60:9.2f}  {r.haul:6.1f}  {row.total:8.1f}  {r.mean_cut_height:5.1f}  "
              f"{', '.join(row.purchases) or ('end card' if row is rows[-1] else 'save balance')}")
    days = [sum(row.tree.stage == stage for row in rows) for stage in range(3)]
    ended = rows[-1].total >= milestones[-1]
    print(f"Days per tree to end card: {days}; end day {rows[-1].day}{'' if ended else ' (not reached)'}, "
          f"{rows[-1].seconds / 60:.2f} min.")
    return rows


def main():
    global RICHNESS_MODE, RICHNESS_HEIGHT_SCALE_M
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check-cpp", action="store_true", help="also verify Upgrades.cpp matches the recommendation")
    parser.add_argument("--one-tree", action="store_true",
                        help="only the default game (one tree, recruits on) with today's costs: recommendation, "
                             "and the --milestones given")
    parser.add_argument("--milestones", help="one-tree milestones to evaluate, e.g. 25,155,270")
    parser.add_argument("--richness", choices=("day", "height"), default="day",
                        help="richness per day (the old guess) or from the cut height like the game")
    parser.add_argument("--height-scale", type=float, default=0.0,
                        help="with --richness height: FDayTuning::RichnessHeightScale in metres (0: linear)")
    args = parser.parse_args()
    RICHNESS_MODE, RICHNESS_HEIGHT_SCALE_M = args.richness, args.height_scale
    if args.one_tree:
        costs = (14, 32, 68, 144, 304, 640)
        print(f"Brood ladder {EFFECTS[4]}; costs {costs}; richness {RICHNESS_MODE}"
              + (f", height scale {RICHNESS_HEIGHT_SCALE_M:g} m" if RICHNESS_MODE == "height" else ""))
        try:
            report_one_tree(True, costs)
        except AssertionError as error:
            print(f"(no automatic recommendation: {error})")
        if args.milestones:
            report_one_tree_fixed(tuple(float(v) for v in args.milestones.split(",")), costs)
        return
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
    if not candidates:
        # Recruits from day 1 (Brood level 0 = 2, 15 Nov): the one-tree-off campaign no longer meets its windows with any
        # cost candidate. One tree is the default game; report its campaigns with today's costs.
        costs = (14, 32, 68, 144, 304, 640)
        print("One tree OFF: no cost candidate meets both pacing windows and the haul-growth window with this Brood "
              f"ladder {EFFECTS[4]}; skipped. One tree ON with today's costs {costs}:")
        report_one_tree(True, costs)
        report_one_tree(False, costs)
        return
    _, costs, on, off = min(candidates, key=lambda candidate: candidate[0])
    for label, rows in (("Recruits ON", on), ("By hand (recruits OFF)", off)):
        end = end_row(rows)
        milestones = thresholds(end)
        print(f"\nOne tree OFF, {label}; milestones {milestones}")
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
    print("\nMeasured supply: Tools/tree_bands_measured.txt (template 0.5, absolute 10 m bands)")
    for stage, supply in enumerate(TREE_SUPPLY):
        print(f"Tree {stage + 1}: {len(supply)} bands from {TREE_BAND_ORIGINS_M[stage]} m, "
              f"{sum(supply)} pieces; first leaves {FIRST_LEAF_HEIGHT_M[stage]:g} m")
    tree_on, _ = report_one_tree(True, costs)
    tree_off, _ = report_one_tree(False, costs)
    print("\nOne-tree Upgrades.cpp numbers:")
    for name, values in (("MilestonesOneTreeWithRecruits", tree_on),
                         ("MilestonesOneTreeByHand", tree_off)):
        print(f"const float {name}[] = {{ " + ", ".join(cpp_float(v) for v in values) + " };")
    if args.check_cpp:
        check_cpp(costs, on, off, tree_on, tree_off)


if __name__ == "__main__":
    main()
