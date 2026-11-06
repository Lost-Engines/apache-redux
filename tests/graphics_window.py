#!/usr/bin/env python3
"""Check Redux resizing on a private Xvfb display, using existing game data."""
import argparse, os, subprocess, tempfile, time
from pathlib import Path
from PIL import ImageGrab
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',type=Path,required=True)
p.add_argument('--data-dir',type=Path,required=True)
p.add_argument('--settle-seconds',type=float,default=.3,help='Allow extra time for instrumented builds')
a=p.parse_args()
assert os.environ.get('APACHE_PRIVATE_DISPLAY')=='1', 'Run on private Xvfb, never the desktop'
os.environ['XDG_SESSION_TYPE']='x11'
os.environ.pop('WAYLAND_DISPLAY',None)
def xdo(*args): return subprocess.check_output(['xdotool',*map(str,args)],text=True).strip()
for resolution in ['auto','640x480']:
 with tempfile.TemporaryDirectory(prefix='apache-redux-window-') as directory:
  with open(Path(directory)/'game.log','w+') as log:
   game=subprocess.Popen([str(a.binary.resolve()),'--data-dir',str(a.data_dir.resolve()),'--save-dir',directory,'--mission','61','--no-audio','--skip-intro','--scale','1','--resolution',resolution,'--fov','110'],stdout=log,stderr=log)
   try:
    for _ in range(500):
     found=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(game.pid),'--name','Apache Redux'],capture_output=True,text=True)
     if found.returncode==0: wid=found.stdout.splitlines()[0];break
     assert game.poll() is None, 'Game exited during startup'
     time.sleep(.03)
    else: raise AssertionError('No game window')
    xdo('windowfocus',wid)
    def capture():
     geometry=dict(line.split('=',1) for line in xdo('getwindowgeometry','--shell',wid).splitlines())
     x,y,w,h=[int(geometry[k]) for k in ['X','Y','WIDTH','HEIGHT']]
     return ImageGrab.grab(xdisplay=os.environ['DISPLAY']).crop((x,y,x+w,y+h)).convert('RGB')
    for _ in range(100):
     if len(capture().getcolors(65536) or [])>30: break
     assert game.poll() is None;time.sleep(.05)
    else: raise AssertionError('No rendered flight frame')
    # Xvfb repeats these symbols on modifier levels; use unambiguous physical codes.
    key_codes={'F1':67,'F2':68,'F3':69,'F6':72,'Pause':127}
    def key(value): xdo('key','--clearmodifiers',key_codes.get(value,value));time.sleep(a.settle_seconds)
    key('Pause');key('F6')
    before_orbit=capture()
    xdo('keydown','Alt_L','Right');time.sleep(max(.5,a.settle_seconds));xdo('keyup','Right','Alt_L')
    time.sleep(a.settle_seconds)
    assert capture().tobytes()!=before_orbit.tobytes(), 'Alt + Right did not move the paused external camera'
    xdo('windowsize',wid,1000,700)
    for _ in range(100):
     image=capture();bounds=image.getbbox()
     if image.size==(1000,700) and bounds:
      if resolution=='auto' and bounds==(0,0,1000,700):break
      if resolution!='auto' and 32<=bounds[0]<=34 and 966<=bounds[2]<=968 and bounds[1]==0 and bounds[3]==700:break
     assert game.poll() is None;time.sleep(.1)
    else:raise AssertionError(('Frame did not resize',bounds))
    key('F3');key('F2');pilot=capture()
    key('F6');external=capture()
    assert external.tobytes()!=pilot.tobytes(), 'F6 did not select the external view'
    xdo('keydown',key_codes['F1']);time.sleep(max(1,a.settle_seconds));help_frame=capture()
    xdo('keyup',key_codes['F1']);time.sleep(a.settle_seconds)
    assert help_frame.tobytes()!=external.tobytes() and capture().tobytes()==help_frame.tobytes(), 'Held F1 did not leave help open'
    key('F1');assert capture().tobytes()==pilot.tobytes(), 'Second F1 did not return to the paused pilot cockpit'
    key('F1');key('F2');assert capture().tobytes()==pilot.tobytes(), 'F2 did not leave help for the pilot cockpit'
    key('F6');external=capture();key('F1');key('Escape')
    assert capture().tobytes()==external.tobytes(), 'Escape did not return from help to the previous view'
    key('Escape');assert capture().tobytes()!=external.tobytes(), 'Escape did not open confirmation'
    key('Escape');assert capture().tobytes()==external.tobytes(), 'Escape did not cancel confirmation'
    key('Escape')
    xdo('windowsize',wid,1100,750);time.sleep(max(.4,a.settle_seconds))
    assert game.poll() is None, 'Resizing the exit prompt terminated the game'
    key('n');key('F1');key('Escape');key('Escape');key('y')
    assert game.poll() is None, 'Could not return to the menu'
    xdo('mousemove','--window',wid,5,5);time.sleep(max(.2,a.settle_seconds));main_menu=capture()
    def click_menu(x,y):
     w,h=capture().size;scale=min(w/640,h/480)
     xdo('mousemove','--window',wid,round((w-640*scale)/2+x*scale),round((h-480*scale)/2+y*scale));xdo('click',1)
     time.sleep(max(.2,a.settle_seconds));xdo('mousemove','--window',wid,5,5);time.sleep(max(.2,a.settle_seconds))
    click_menu(620,472);assert capture().tobytes()!=main_menu.tobytes(), 'Scaled menu click did not open exit confirmation'
    click_menu(550,445);assert capture().tobytes()==main_menu.tobytes(), 'Scaled No button did not return to the same menu'
    print(resolution, 'resize, aspect, paused orbit, views, F1/F2 help return, exit prompt and scaled menu clicks passed',flush=True)
   finally:
    if game.poll() is None: game.terminate()
    game.wait(timeout=5)
    log.seek(0);errors=log.read()
    assert 'runtime error:' not in errors and 'AddressSanitizer' not in errors, errors
