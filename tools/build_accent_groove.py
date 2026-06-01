#!/usr/bin/env python3
"""Build patches/ghost-accent-groove.vcv -- a boom-bap groove that *plays the
GHOST CTRL accent system*.

KCK is the dynamic engine (its AccentMix is non-neutral: ghost -6dB -> accents
louder). It runs a busy ghost-note kick with TWO accent rails derived from the
Hora sequencer:
  - Accent A (TOTAL) on the medium hits
  - Accent B (LOCAL) on the biggest hits -> A+B = the "both" case (loudest)
So the kick breathes ghost / accent / both, and GHOST CTRL's ACCENT A & ACCENT B
knobs sculpt that range live. SNR backbeat gets a TOTAL accent (snappier
character); hats keep the groove.

Reuses the working clock / sequencer baseline / audio configs from
ghost-buscrush-demo so the sequencer runs and audio is wired.

Run: python tools/build_accent_groove.py
"""
import subprocess, json, tempfile, pathlib

BASE = "patches/ghost-buscrush-demo.vcv"
OUT  = "patches/ghost-accent-groove.vcv"

def read(vcv):
    return json.loads(subprocess.run(['bash','-c',f'zstd -dc "{vcv}" | tar -xO ./patch.json'],
                                     capture_output=True).stdout)

base = read(BASE)
mod  = {m.get('model'): m for m in base['modules']}
CLK, SEQ, AUD = mod['SlimeChild-Substation-Clock'], mod['Drumsequencer'], mod['AudioInterface2']
SAPH, CRUSH    = mod['Saphire'], mod['BusCrush']
KCK, SNR, OHCH, CTRL = mod['Kck'], mod['Snr'], mod['ChhOhh'], mod['Tr909Ctrl']

# --- contiguous CTRL + voices (row 0) so the expander chain reaches them ---
CTRL['pos'], KCK['pos'], SNR['pos'], OHCH['pos'] = [0,0], [8,0], [24,0], [36,0]
CLK['pos'], SEQ['pos'] = [0,1], [8,1]
CRUSH['pos'], SAPH['pos'], AUD['pos'] = [38,1], [50,1], [60,1]
# CTRL accent rails up so the accents really speak; master with headroom
CTRL['params'] = [{'value':0.9,'id':0},{'value':0.8,'id':1},{'value':0.5,'id':2}]

modules = [CLK, SEQ, CTRL, KCK, SNR, OHCH, CRUSH, SAPH, AUD]

# --- Hora pattern: track = output-2 (kick four-on-floor lives on track 2) -----
# 32-step grid. Accent rows must overlap the trigger steps they accent.
PAT = {
  2: [0,3,6,7,10,14,16,19,22,26,30],   # out4  -> KCK TRIG  (busy ghost kicks)
  3: [0,10,16,26],                     # out5  -> KCK TOTAL (Accent A, medium)
  9: [0,16],                           # out11 -> KCK LOCAL (Accent B; A+B = loudest)
  4: [4,12,14,20,28,30],               # out6  -> SNR TRIG  (backbeat + ghosts)
  5: [4,12,20,28],                     # out7  -> SNR TOTAL (accented backbeats)
  6: [2,6,10,14,18,22,26,30],          # out8  -> CHH       (offbeat hats)
  8: [12,28],                          # out10 -> OHH       (open-hat accents)
}
data = SEQ['data']
for t in range(1,13):                    # clear every track, then write ours
    data[f'gates{t}P1'] = [0]*32
for t,steps in PAT.items():
    row=[0]*32
    for i in steps:
        row[i]=1
        data[f'proba_level {i+1}_{t}']=1.0     # step-first keys; ensure it fires
        data[f'ratchet_level {i+1}_{t}']=0
    data[f'gates{t}P1']=row
data['runningSeq']=True; data['gate run']=0; data['Direct Clock']=0; data['auto reset']=0

# --- cables ------------------------------------------------------------------
C = [
  (CLK['id'],1,SEQ['id'],2,'#4f99ff'),                 # clock -> seq
  # triggers (red) + accent rails (yellow)
  (SEQ['id'],4, KCK['id'],0,'#f44336'),                # kick trig
  (SEQ['id'],5, KCK['id'],11,'#ffd400'),               # kick Accent A (TOTAL)
  (SEQ['id'],11,KCK['id'],10,'#ffd400'),               # kick Accent B (LOCAL)
  (SEQ['id'],6, SNR['id'],0,'#f44336'),                # snare trig
  (SEQ['id'],7, SNR['id'],6,'#ffd400'),                # snare Accent (TOTAL)
  (SEQ['id'],8, OHCH['id'],0,'#f44336'),               # closed hat trig
  (SEQ['id'],10,OHCH['id'],1,'#f44336'),               # open hat trig
  # audio (blue) -> the one BusCrush
  (KCK['id'],0, CRUSH['id'],0,'#3695ef'),
  (SNR['id'],0, CRUSH['id'],1,'#3695ef'),
  (OHCH['id'],0,CRUSH['id'],2,'#3695ef'),
  (OHCH['id'],1,CRUSH['id'],3,'#3695ef'),
  (CRUSH['id'],0,SAPH['id'],0,'#3695ef'),
  (CRUSH['id'],1,SAPH['id'],1,'#3695ef'),
  (SAPH['id'],0,AUD['id'],0,'#3695ef'),
  (SAPH['id'],1,AUD['id'],1,'#3695ef'),
]
cables=[{'id':7000000+i,'outputModuleId':om,'outputId':oi,'inputModuleId':im,'inputId':ii,'color':col}
        for i,(om,oi,im,ii,col) in enumerate(C,1)]

patch={'version':base['version'],'unsaved':False,'zoom':0.0,
       'gridOffset':base.get('gridOffset',[0,0]),'modules':modules,'cables':cables}
with tempfile.TemporaryDirectory() as t:
    (pathlib.Path(t)/"patch.json").write_text(json.dumps(patch,indent=2))
    subprocess.run(['bash','-c',f'tar -C "{t}" -cf - . | zstd -q -o "{OUT}" -f'],check=True)
print(f"built {OUT}: {len(modules)} modules, {len(cables)} cables; CTRL ACCENT A=0.9 B=0.8")
