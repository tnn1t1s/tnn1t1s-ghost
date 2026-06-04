#!/usr/bin/env python3
"""Build patches/demos/ghost-supersonic.vcv -- a faithful recreation of the classic
electro / "Supersonic"-school 808 groove (Planet Rock -> J.J. Fad lineage),
played on the GHOST kit. ~132 BPM feel.

Exact step data for the original isn't published anywhere (and drum rhythms
aren't copyrightable), so this reconstructs the well-documented electro
conventions: snare on the backbeat ONLY (no ghost notes), a syncopated electro
kick woven around it, busy 8th-note hats with offbeat open-hats, an electro clap
doubling the backbeat, and a descending tom fill in bar 2.

Reuses the working clock / sequencer baseline / audio from ghost-buscrush-demo.
Run: python tools/build_supersonic.py
"""
import subprocess, json, tempfile, pathlib

BASE = "patches/demos/ghost-buscrush-demo.vcv"
OUT  = "patches/demos/ghost-supersonic.vcv"

base = json.loads(subprocess.run(['bash','-c',f'zstd -dc "{BASE}" | tar -xO ./patch.json'],
                                 capture_output=True).stdout)
m = {x.get('model'): x for x in base['modules']}
CLK, SEQ, AUD = m['SlimeChild-Substation-Clock'], m['Drumsequencer'], m['AudioInterface2']
SAPH, CRUSH   = m['Saphire'], m['BusCrush']
KCK, SNR, OHCH, RC, TOMS, CTRL = m['Kck'], m['Snr'], m['ChhOhh'], m['RimClap'], m['Toms'], m['GhostCtrl']

# CTRL + voices contiguous (row 0) so the expander chain reaches every voice.
CTRL['pos'],KCK['pos'],SNR['pos'],OHCH['pos'],RC['pos'],TOMS['pos'] = [0,0],[8,0],[24,0],[36,0],[50,0],[62,0]
CLK['pos'],SEQ['pos'] = [0,1],[8,1]
CRUSH['pos'],SAPH['pos'],AUD['pos'] = [40,1],[52,1],[62,1]
# Electro is punchy + uniform: ACCENT A full so every (accented) kick hits hard.
CTRL['params'] = [{'value':1.0,'id':0},{'value':0.5,'id':1},{'value':0.5,'id':2}]
for v in (KCK,SNR,OHCH,RC,TOMS): v.pop('params',None)   # fresh voice defaults

modules = [CLK, SEQ, CTRL, KCK, SNR, OHCH, RC, TOMS, CRUSH, SAPH, AUD]

# --- electro pattern (32 steps; track = output-2) ---------------------------
PAT = {
  2:  [0,6,10,16,22,26],                 # out4  KCK trig  (syncopated electro kick)
  3:  [0,6,10,16,22,26],                 # out5  KCK TOTAL (accent every kick -> full punch)
  4:  [4,12,20,28],                      # out6  SNR trig  (backbeat only, no ghosts)
  5:  [4,12,20,28],                      # out7  SNR TOTAL (snappier accent character)
  6:  [0,2,4,8,10,12,16,18,20,24,26,28], # out8  CHH       (8th hats, holes where OHH plays)
  8:  [6,14,22,30],                      # out10 OHH       (offbeat open-hat "tss")
  9:  [4,12,20,28],                      # out11 CLAP      (doubles the backbeat)
  10: [28,30],                           # out12 TOM LOW   } descending
  11: [26],                              # out13 TOM MID   } electro
  12: [24],                              # out14 TOM HIGH  } fill, bar 2
}
data = SEQ['data']
for t in range(1,13): data[f'gates{t}P1'] = [0]*32
for t,steps in PAT.items():
    row=[0]*32
    for i in steps:
        row[i]=1
        data[f'proba_level {i+1}_{t}']=1.0
        data[f'ratchet_level {i+1}_{t}']=0
    data[f'gates{t}P1']=row
data['runningSeq']=True; data['gate run']=0; data['Direct Clock']=0; data['auto reset']=0
# nudge the displayed tempo toward the ~132 BPM electro feel
for p in SEQ['params']:
    if p['id']==2: p['value']=132.0

# --- cables ------------------------------------------------------------------
C = [
  (CLK['id'],1,SEQ['id'],2,'#4f99ff'),
  (SEQ['id'],4, KCK['id'],0,'#f44336'), (SEQ['id'],5, KCK['id'],11,'#ffd400'),
  (SEQ['id'],6, SNR['id'],0,'#f44336'), (SEQ['id'],7, SNR['id'],6,'#ffd400'),
  (SEQ['id'],8, OHCH['id'],0,'#f44336'),(SEQ['id'],10,OHCH['id'],1,'#f44336'),
  (SEQ['id'],11,RC['id'],0,'#f44336'),
  (SEQ['id'],12,TOMS['id'],0,'#f44336'),(SEQ['id'],13,TOMS['id'],1,'#f44336'),(SEQ['id'],14,TOMS['id'],2,'#f44336'),
  # audio -> single BusCrush (8 channels)
  (KCK['id'],0,CRUSH['id'],0,'#3695ef'), (SNR['id'],0,CRUSH['id'],1,'#3695ef'),
  (OHCH['id'],0,CRUSH['id'],2,'#3695ef'),(OHCH['id'],1,CRUSH['id'],3,'#3695ef'),
  (RC['id'],0,CRUSH['id'],4,'#3695ef'),
  (TOMS['id'],0,CRUSH['id'],5,'#3695ef'),(TOMS['id'],1,CRUSH['id'],6,'#3695ef'),(TOMS['id'],2,CRUSH['id'],7,'#3695ef'),
  (CRUSH['id'],0,SAPH['id'],0,'#3695ef'),(CRUSH['id'],1,SAPH['id'],1,'#3695ef'),
  (SAPH['id'],0,AUD['id'],0,'#3695ef'), (SAPH['id'],1,AUD['id'],1,'#3695ef'),
]
cables=[{'id':7100000+i,'outputModuleId':om,'outputId':oi,'inputModuleId':im,'inputId':ii,'color':col}
        for i,(om,oi,im,ii,col) in enumerate(C,1)]

patch={'version':base['version'],'unsaved':False,'zoom':0.0,
       'gridOffset':base.get('gridOffset',[0,0]),'modules':modules,'cables':cables}
with tempfile.TemporaryDirectory() as t:
    (pathlib.Path(t)/"patch.json").write_text(json.dumps(patch,indent=2))
    subprocess.run(['bash','-c',f'tar -C "{t}" -cf - . | zstd -q -o "{OUT}" -f'],check=True)
print(f"built {OUT}: {len(modules)} modules, {len(cables)} cables, electro 808 groove @ ~132")
