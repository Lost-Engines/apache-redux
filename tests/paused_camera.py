#!/usr/bin/env python3
"""Exercise paused cameras and frozen flight using the headless session inspector."""
import argparse
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, required=True)
parser.add_argument('--data-dir', type=Path, required=True)
args = parser.parse_args()


def run(commands, model=0):
    result = subprocess.run(
        [str(args.binary.resolve()), str(args.data_dir.resolve()), 'session', '61', '0', str(model)],
        input='\n'.join(commands) + '\n', text=True, capture_output=True, check=True,
    )
    rows = result.stdout.splitlines()
    states = [tuple(map(int, row.split()[1:])) for row in rows if row.startswith('state ')]
    cameras = [tuple(map(int, row.split()[1:])) for row in rows if row.startswith('camera ')]
    actors = [row for row in rows if row.startswith(('actor ', 'fragment ', 'projectile '))]
    return states, cameras, actors


def frames(count):
    return ['step 10', 'draw -'] * count


start = ['detail 0', 'step 10', 'post 25', 'step 1']  # P pauses after one flight frame.
switch_views = ['post 61', 'step 1', 'draw -', 'snapshot', 'post 60', 'step 1', 'draw -', 'snapshot']
resume = ['post 25', 'step 10', 'draw -', 'snapshot']
for model in (0, 2):
    orbit_start = start + ['post 64', 'step 1', 'draw -', 'snapshot', 'scene-state']
    commands = orbit_start + ['key 56 1', 'key 77 1', 'key 80 1']
    commands += frames(10) + ['snapshot', 'scene-state', 'draw -', 'snapshot']
    commands += ['key 56 0', 'key 77 0', 'key 80 0', 'key 51 1']
    commands += frames(8) + ['snapshot', 'scene-state', 'key 51 0', 'step 10', 'draw -', 'snapshot']
    states, cameras, actors = run(commands + switch_views + resume, model)
    assert len(states) == 8
    assert all(state == states[0] for state in states[1:5]), 'Paused camera changed simulation state'
    assert cameras[0] != cameras[1], 'Alt + arrows did not move the paused camera'
    assert cameras[1] == cameras[2], 'Extra redraw repeated camera movement'
    assert cameras[2][:3] != cameras[3][:3], 'Comma did not zoom the paused camera'
    assert cameras[3] == cameras[4], 'Camera kept moving after releasing controls'
    assert states[5][1] == 5 and states[6][1] == 0, 'Paused F3/F2 view switching failed'
    assert states[5][0] == states[6][0] == states[0][0], 'View switching advanced time'
    count = len(actors) // 3
    assert count > 0 and len(actors) % 3 == 0 and actors[:count] == actors[count:2 * count] == actors[2 * count:], 'Actors or effects advanced while paused'
    baseline, _, _ = run(orbit_start + switch_views + resume, model)
    assert states[-1][0] == states[0][0] + 10, 'Simulation did not resume normally'
    assert states[-1][7:32] == baseline[-1][7:32], 'Paused controls changed flight after resuming'
    print(f'Flight model {model}: orbit, zoom, view switching, frozen world and clean resume passed')

# Helmet-mounted display and spectator views use the same live paused input path.
for key, mode in ((35, 12), (87, 16)):
    commands = start + [f'post {key}', 'step 1', 'draw -', 'snapshot', 'key 56 1', 'key 77 1', 'key 80 1']
    states, cameras, _ = run(commands + frames(8) + ['snapshot'])
    assert states[0][1] == states[1][1] == mode
    assert states[0][:32] == states[1][:32], 'Looking around changed flight state'
    assert cameras[0] != cameras[1], f'Paused view {mode} did not move'
    print(f'View {mode}: paused look controls passed')

# The crash camera has automatic motion and a mission-ending timer, both frozen.
states, cameras, _ = run(start + ['view 19', 'draw -', 'snapshot'] + frames(20) + ['snapshot'])
assert states[0] == states[1] and cameras[0] == cameras[1], 'Crash sequence advanced while paused'
print('Paused crash camera remains frozen')
