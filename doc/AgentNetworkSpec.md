# TNN1T1S Agent Network

## Purpose

Agent Network is a Rack-native, agent-trainable modular control system whose learned computation is exposed as a physically patchable analog computer.

It is not prompt-to-music generation, an audio neural-network black box, or a visual metaphor for machine learning. It is a patch containing ordinary monophonic CV connections, explicit weighted sums, nonlinear neurons, and output controls. An external agent can train the parameters, but the system remains readable, audible, interruptible, and performable in Rack.

> A machine may make and continuously revise a patch that is technically legible but practically beyond human patching patience.

The first instrument is a target-sound follower. It listens to a changing target recording, computes a compact feature vector, maps that vector through two learned layers, and drives a synthesis/effects patch toward a recognizable family resemblance. Fidelity is a useful pressure, not the only goal. The agent may preserve useful artifacts, local minima, and a consistent sonic identity.

## Product principles

1. The network is the patch. Every learned scalar has a front-panel control and a stable Rack parameter identifier. There are no hidden matrix weights.
2. Cables remain ordinary cables. Version 1 uses no polyphonic cable and no expander bus for learned connectivity. Each edge is visible and independently patchable.
3. The agent is a slow maintainer, not an audio-rate brain. Audio-rate inference runs in modules. Feature extraction runs at a bounded control rate. Candidate selection, evaluation, and weight updates run outside Rack's audio callback.
4. Human intervention always wins. Any manual parameter gesture freezes that parameter from agent writes until explicitly released. HOLD freezes all writes. Rollback is first-class.
5. Rack metadata is interface truth. Parameters, inputs, outputs, and lights are declared with Rack config metadata. No parallel manifest is required for Version 1.
6. No fake precision. The system reports its features, loss components, parameter change set, and accepted/rolled-back candidate. It does not claim it has reconstructed a target sound merely because a proxy metric improved.
7. Patchability is part of the sound. A human may repatch an edge, substitute a voice, drive a hidden neuron manually, or use a neuron as a standalone nonlinear CV processor.

## Version 1 reference instrument

### Topology

Version 1 is a dense ordinary-cable network:

~~~
target audio
  -> AGENT SENSE
  -> FEATURE BUS: 8 scalar features
  -> 8 x AGENT NEURON/HIDDEN
  -> 12 x AGENT CTRL
  -> synthesis and effects patch
~~~

For hidden neuron j and final controller k:

~~~
h[j] = act[j](bias[j] + sum_i W1[j,i] * feature[i])   j = 1..8
ctrl[k] = map[k](act[k](bias2[k] + sum_j W2[k,j] * h[j]))   k = 1..12
~~~

This produces 64 first-layer edges, 96 output-layer edges, and 12 final control edges: 172 ordinary cables. It exposes 160 learned weights, 20 biases, and 24 output mappings: 204 visible values.

The scale is intentional. A human can understand each connection and control in isolation, but would not casually construct, tune, and continuously revise the full patch by hand.

### Suite modules

| Module | Quantity in reference patch | Role |
|---|---:|---|
| AGENT SENSE | 1 | Target/model audio observation, feature extraction, comparison taps |
| FEATURE BUS | 1 | Eight labeled monophonic feature outputs, buffered for fan-out |
| AGENT NEURON | 8 | First learned layer, eight weighted inputs to one hidden scalar |
| AGENT CTRL | 12 | Output layer, eight weighted inputs to one scaled and offset control CV |
| AGENT CONSOLE | 1 | Transaction state, training controls, compact audit display |

No module is mandatory outside the reference patch. AGENT NEURON and AGENT CTRL should be useful as ordinary patch components when used alone.

## Signal contract

### CV conventions

All neuron signals use a zero-centered nominal range of -5 V to +5 V.

- Input voltage is clamped only for the internal weighted-sum calculation.
- A weight is bipolar, -2.0 to +2.0.
- Bias is -5.0 to +5.0 in normalized neuron units.
- Pre-activation sum is clamped to -12 to +12.
- Hidden output is -5 V to +5 V.
- CTRL has physical SPAN of 0 to 10 V and OFFSET of -10 V to +10 V.
- CTRL emits OFFSET + SPAN * hidden / 5, then applies optional post-map slew.

The output stage is deliberately explicit. A learned control does not secretly decide whether it represents pitch, cutoff, or a send. The receiving module and visible cable give it meaning.

### Target features

AGENT SENSE Version 1 computes eight stable low-dimensional descriptors from target audio. Values update at 50 Hz using rolling windows, are smoothed, standardized against a rolling 30-second target history, and emitted as signed CV.

1. LEVEL: log RMS energy
2. ONSET: transient strength
3. BRIGHT: spectral-centroid proxy
4. SPREAD: spectral spread
5. NOISE: normalized zero-crossing/noise proxy
6. HARM: harmonicity proxy
7. FLUX: frame-to-frame spectral flux
8. MOTION: envelope movement / modulation rate

Version 1 does not call these a learned perceptual embedding. They are inspectable features, sufficient to prove whether the control network produces compelling dynamic behavior.

The external training agent may use a richer objective: multi-resolution spectral loss, transient alignment, envelope loss, clipped-audio penalty, and human preference. Those values need not all become cables.

### Audio interfaces

AGENT SENSE has TARGET L/R and MODEL L/R audio inputs; RESET trigger input; FEATURE 1 through FEATURE 8 outputs; ERROR output; and a 50 Hz CLOCK output. Lights: TARGET, MODEL, CLIP, ACTIVE.

The target is normally supplied by a Stochastic Telegraph Memory ensemble: Memory plus Brainwash or Embellish, Ruminate, and Depict. Depict remains the visible record of the target loop and moving heads. AGENT SENSE does not duplicate the recorder or claim that the Depict image is a sufficient representation of timbre.

## AGENT NEURON

### Role

AGENT NEURON is an eight-input, one-output weighted-sum and nonlinear CV processor. It is a physical neuron, not a matrix cell.

### Front panel and controls

AGENT NEURON is an 18HP dark Lab module. This follows the Ghost Lab precedent: dense expert surface, shared four-column rhythm, and hard cap below 16 exposed controls.

| Control | Count | Range / purpose |
|---|---:|---|
| W1 through W8 | 8 | bipolar input weights, -2.0 to +2.0 |
| BIAS | 1 | additive normalized bias |
| ACT | 1 | five-position activation selector |
| DRIVE | 1 | activation input gain, 0.25 to 4.0 |
| OUT | 1 | output level, 0 to 5 V |
| LOCK | 1 button/light | freeze agent writes to this module |
| NUDGE | 1 button | request bounded agent proposal for this module only |

Ports: IN 1 through IN 8; BIAS CV; ACT CV; OUT; SUM monitor; ACT OUT monitor.

Lights: SUM is a five-segment bipolar pre-activation meter; ACT is a five-segment output meter; LOCK is amber; WRITE pulses green; TOUCH is white while a human owns a control.

### Activation modes

1. LINEAR
2. TANH
3. SOFT, z divided by one plus absolute z
4. FOLD, bounded triangle fold
5. GATE, smooth threshold response

TANH is the reference-patch default. The others keep the module musically useful beyond the target follower and provide later architectural choices.

### Manual ownership

A changed weight is a musical statement. When the user touches a weight, bias, activation, drive, or output knob:

- the control becomes locally owned;
- agent updates to that parameter are refused;
- ownership persists in patch JSON;
- Release to agent returns one control;
- Release all returns the module.

The agent reads ownership before proposing an update and may learn around locked parameters.

## AGENT CTRL

AGENT CTRL is the same core eight-input neuron, designed as the final learned stage and intended to connect directly to a named target parameter.

It shares eight input weights, BIAS, ACT, DRIVE, LOCK, NUDGE, and meters. Generic OUT is replaced by:

| Control | Purpose |
|---|---|
| SPAN | positive output scaling, 0 to 10 V |
| OFFSET | output origin, -10 to +10 V |
| SLEW | post-map change time, 0 to 250 ms |
| NAME | editable visible destination name, serialized in patch state |

The initial reference patch names the twelve output modules PITCH, WAVE, FM, CUTOFF, RES, ENV, AMP, DRIVE, DELAY, REVERB, MIX, and SPACE. These labels are a reference voice map, not a protocol. A Ghost patch may name them KCK TUNE, KCK DECAY, SNR NOISE, HAT METAL, BUS DRIVE, and sends.

## FEATURE BUS

FEATURE BUS is an 8HP utility module. It receives eight monophonic AGENT SENSE outputs and presents two vertical banks of four labeled buffered outputs.

It has no learned parameter. It makes the fan-out intentional and readable:

- every feature has fixed label and color mark;
- every output is normalled only to its corresponding input;
- no hidden duplication, polyphony, or broadcast;
- eight sources route visibly to eight hidden neurons, yielding 64 cables.

A performer can replace any feature with an LFO, envelope follower, manual voltage, clocked random source, or a different patch signal.

## AGENT CONSOLE

AGENT CONSOLE is an 18HP dark Lab supervisory module. It contains no model weights and does not process audio.

Controls: TRAIN, HOLD, ROLLBACK, COMMIT, BUDGET, and CHARACTER. CHARACTER weights similarity against controlled deviation in the external objective.

The display and lamps show state (IDLE, OBSERVE, CANDIDATE, A/B, COMMITTED, HELD, ROLLED BACK), latest loss and moving average, delta from baseline, candidate number, accepted/rejected counts, manual-ownership count, and clipped-output warning.

Outputs: LOSS CV, IMPROVED trigger, REJECTED trigger, STEP trigger, and discrete STATE CV.

### Transactional update protocol

The agent must never drip arbitrary new weights into an audible patch.

1. Observe target, model, feature vector, patch state, ownership, and baseline snapshot ID.
2. Propose a bounded sparse delta. Version 1 maximum is 12 changed parameters per candidate, absolute change no greater than 0.10 per weight/bias and 0.25 V for final map controls.
3. Queue the candidate on Rack's control/UI path. Update parameters atomically at next safe control boundary.
4. Settle for at least 250 ms plus configured SLEW time.
5. Evaluate a fixed listening window against baseline using the external objective.
6. Commit only when minimum improvement threshold and all safety constraints are met. Otherwise restore exact baseline snapshot.
7. Audit changed stable parameter IDs, comparison results, and reason.

The audio thread performs only deterministic DSP, parameter reads, slew, and meters. It allocates no memory, receives no network request, and runs no optimizer.

## Agent-facing interface

Every learned control is declared with Rack configParam and a stable human-readable ID, for example:

~~~
agent.neuron.03.weight.01
agent.neuron.03.bias
agent.ctrl.07.weight.04
agent.ctrl.07.span
agent.ctrl.07.offset
~~~

Inputs and outputs use configInput and configOutput names matching their labels. This follows Ghost's current AgentModule direction: Rack's native metadata is interface truth.

Rack metadata does not express ownership, snapshots, or write events. The suite adds only these runtime fields, serialized in normal module JSON and exposed by existing inspection paths:

- agentWritable
- manualOwned
- lastWriteRevision
- snapshotId
- candidateId

There is no second patch format and no GUI scraping requirement.

The external agent must enumerate module metadata/current values, read ownership/Console state, render or subscribe to short target/model clips, submit bounded transactions, receive commit/reject results, and store experiment lineage outside the VCV patch.

Version 1 does not allow topology changes during performance. An agent may construct the reference patch before a session and later propose wiring changes only between stopped runs.

## Panel and visual doctrine

Agent Network is a TNN1T1S Lab family, not part of Ghost's cream 909 production shell.

It adopts Ghost dark-Lab standards:

- 18HP dark expert shell;
- shared title/subtitle position;
- common four-column rhythm;
- restrained Inter typography;
- mechanical regularity rather than decorative futurism;
- fewer than 16 meaningful exposed controls per module.

It does not use 909 marks, voice codes, graph paper, or clone language. The reference is late analog computer / test rack, rendered with Ghost's restraint.

Panel geometry stays declarative and generated:

~~~
panelkit/specs/panels/AgentNeuron.panel.yaml
panelkit/specs/panels/AgentCtrl.panel.yaml
panelkit/specs/panels/AgentSense.panel.yaml
panelkit/specs/panels/AgentConsole.panel.yaml
panelkit/specs/panels/FeatureBus.panel.yaml
~~~

Specs declare labels, semantic IDs, param/CV pairs, sections, and I/O. They never contain hand-tuned SVG coordinates. The deterministic panel compiler owns geometry and generated SVG.

Reference cable colors are deliberate: green for target feature and hidden activity, red for comparison/error or antagonistic modulation, white/grey for neutral manual or clock routes. The cable field is not decoration. A user must be able to trace one feature across the first layer and one hidden neuron's effect across final controls.

## Reference patch

patches/agent-network/01-target-follower.vcv contains:

- Stochastic Telegraph target Memory ensemble with Depict;
- AGENT SENSE and FEATURE BUS;
- eight AGENT NEURON modules, numbered left to right;
- twelve AGENT CTRL modules, ordered by reference voice signal path;
- one constrained synthesis voice and effects chain;
- AGENT CONSOLE;
- target, model, and error scopes/meters.

It opens in musically stable static state with TRAIN disabled. It never starts optimizing merely because a patch was opened.

Lesson patches progress from one feature into one neuron into filter cutoff, to two-by-two, then eight-to-four, then full network static, then a recorded inspectable training session.

## Safety and failure behavior

- No parameter update allocates, locks, or blocks audio thread.
- HOLD and per-control ownership are checked before queued write.
- Reject a candidate that clips configured rails, exceeds RMS limit, destabilizes model voice, or worsens protected loss component.
- Retain latest accepted snapshot and one predecessor in memory and serialize them.
- ROLLBACK restores parameters atomically.
- Agent disconnect leaves deterministic inference at last accepted state. TRAIN becomes dim amber. Audio is not muted or reset.
- Missing target input holds last valid features 500 ms then slews them to zero. No NaN or unbounded voltage is emitted.
- Resetting target statistics never changes learned parameters.

## Tests

Unit tests:

- weighted sum against known vectors;
- bipolar weight/bias boundary behavior;
- activation boundedness and continuity;
- CTRL span/offset exactness;
- slew no overshoot;
- ownership blocks writes and round-trips through serialization;
- commit/rollback restores bit-for-bit snapshots;
- audio process performs no allocation.

Rack integration:

- declared IDs unique and aligned with panel semantic IDs;
- all 172 reference cables resolve to expected ports;
- reference patch reopens identically with TRAIN disabled;
- simulated candidates never write locked controls;
- maximum candidate burst creates no click above agreed threshold at 44.1, 48, and 96 kHz;
- target/agent disconnect follows safe-state contract.

Musical acceptance:

- changing target loop yields accepted and rejected candidates;
- user can override one weight and agent adapts around it;
- user can replace one feature cable with LFO and retain stable system;
- trained patch remains useful when target changes rather than collapsing to silence/rails;
- full rack is exciting to inspect at normal Rack zoom. Complexity must feel purposeful, not like cable wallpaper.

## Delivery sequence

### Phase A: passive analog computer

Implement AGENT NEURON, AGENT CTRL, FEATURE BUS, generated panels, and a deterministic full 8 to 8 to 12 patch generator. No agent writes. Validate physical interaction and cable grammar.

### Phase B: perceptual front end

Implement AGENT SENSE with eight explicit features and comparison taps. Connect to recorded target Memory ensemble. Validate bounded control-rate behavior.

### Phase C: transaction loop

Add AGENT CONSOLE, inspection fields, sparse candidate queue, snapshots, rollback, and simple external evolutionary/coordinate updater. TRAIN remains opt-in and default-off.

### Phase D: objective and lineage

Add multi-term loss, CHARACTER preference, experiment recording, and replayable accepted-update history. Agent must explain changed controls and candidate outcome.

### Phase E: expansion

Only after the 8 to 8 to 12 instrument proves musically alive:

- 16-wide variants;
- second hidden layer;
- Venn as human-playable 2D basis front end;
- learned/selectable feature projections;
- stopped-session topology proposals.

## Non-goals

- no end-to-end waveform neural synthesizer;
- no GPU, ONNX Runtime, or general ML framework in VCV audio process;
- no hidden trainable matrices behind a pretty display;
- no automatic cable rewiring during performance;
- no claim that low loss equals musical success;
- no replacement for Ghost's separate behaviorally cohesive instrument design;
- no release commitment until the patch is fun with TRAIN off.

## Open decisions

1. AGENT SENSE native module or initial external sidecar writing standardized feature CV through agent bridge?
2. Which constrained Ghost-adjacent test voice is the right first model?
3. Is FOLD useful in Version 1 or should it wait until basic system is proven?
4. Initial external optimizer: coordinate search, CMA-ES-style bounded proposals, or existing agent-loop evaluator?
5. How much training history belongs in patch versus durable external experiment store?

## Version 1 definition of done

Version 1 is done when the reference patch can be opened, its target loop changed, and an agent can audibly and visibly revise the 172-cable network toward the new target while:

- every modified parameter is physically visible;
- human can seize any control;
- every candidate commits or rolls back transactionally;
- no audio-thread safety/stability regression occurs;
- result is interesting even when it does not perfectly imitate target.

The finished thing should feel less like an automatic preset finder and more like a machine learning, in public, how to inhabit a patch.
