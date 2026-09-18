# Geese: a language and API for agent-authored Rack patches

## Status

Design specification for the TNN1T1S Ghost Agent Network. Geese is deliberately small for Version 1: it describes real Rack modules, real ports, ordinary mono cables, named parameters, and bounded learning ownership. It does not simulate Rack, scrape a GUI, or create a second hidden patch format.

> A flock of small, opinionated operations makes a patch no human would patiently make.

## 1. Purpose

Geese is a declarative, agent-safe programming interface for constructing and maintaining VCV Rack patches. Its first use is the Agent Network: a visible analog-computer-style neural controller whose physical inference graph is far larger than a person would normally wire or tune.

It covers two needs:

1. Patch authoring: a person or agent can construct an exact patch, including the hundreds of explicit mono connections that make the learned graph physically legible.
2. Live maintenance: a numerical learner can observe declared signals and propose sparse, bounded updates to declared learnable parameters without touching Rack's audio callback or guessing at GUI state.

A .geese program is source. A Rack patch remains the runtime artifact. Compilation resolves module model IDs, instantiates modules, applies initial parameters, connects ports, and attaches Geese metadata to the resulting Rack state.

## 2. Non-goals

- No audio-rate ML training in an arbitrary Rack graph.
- No promise of differentiating through third-party DSP, cables, saturation, or effects.
- No hidden neural-weight matrix disguised as a module.
- No polyphonic cable used to conceal a dense learned layer in Version 1.
- No autonomous unbounded knob turning.
- No replacing the Rack patch file or its module metadata as runtime truth.
- No LLM in a high-frequency optimisation loop. An LLM may design experiments and explain results; a numerical optimiser performs repeated candidate evaluation.

## 3. Design rules

### Rack-native truth

Every module and port is identified by a stable model slug plus a stable local identifier. Every mutable scalar is a normal Rack parameter, declared by the module with configParam(), configInput(), configOutput(), and configLight().

Geese stores only enough source intent to recreate or validate the topology. It must resolve real Rack parameter IDs at load time. If a module version cannot supply a declared parameter or port, compilation fails clearly. It never maps by screen position or label text.

### Visible means physical

A learned edge in the Version 1 reference patch is a normal source output, a normal green/red mono cable, an input on AGENT NEURON or AGENT CTRL, and a front-panel W1 through W8 weight control at that input.

Bias, activation, drive, output span, and offset are ordinary front-panel values. Geese may label, lock, snapshot, and update them. It may not make them invisible.

### Slow learner, fast instrument

| Timescale | Owner | What happens |
|---|---|---|
| Audio sample | Rack modules | Synthesis, DSP, neuron inference, cable voltages |
| 20–100 Hz | AGENT SENSE | Feature extraction, target and model descriptors |
| Bar / 250 ms minimum | learner + Console | Propose, settle, score, commit or roll back |
| Human choice | musician | Patch, lock parameters, set target, decide character |

The learner never performs allocation, filesystem I/O, network access, or an optimiser step in an audio callback.

### Explicit ownership

A parameter is one of:

- manual: human-owned; learner cannot write it;
- learn: learner may propose bounded changes;
- frozen: temporarily locked by HOLD, an active gesture, or a safety state;
- base: calibrated value; recallable but not altered during live adaptation;
- adapt: part of the small live adaptation set.

A physical human gesture immediately changes ownership to manual and cancels any pending proposal that includes that parameter. Returning it to learn is explicit, never a timeout.

## 4. Files and execution

Geese source uses UTF-8 text with a .geese extension.

~~~
target-follower.geese        # topology and initial state
target-follower.patch.json   # normal Rack patch produced/loaded by the host
session.geese-log            # append-only learning/audit events
~~~

Suggested lifecycle:

~~~
geese check target-follower.geese
geese compile target-follower.geese --to target-follower.patch.json
geese attach target-follower.patch.json --agent localhost:7721
geese observe target-follower.geese
geese replay session.geese-log --at revision:42
~~~

The transport is not normative. A Rack plugin can host the compiler/bridge locally; an external process can use a loopback RPC API. The semantics below are normative.

## 5. Core language

### Lexical conventions

- Identifiers: letters, digits, _, -; begin with a letter.
- Names are case-sensitive.
- Values carry units where ambiguity is dangerous: V, ms, Hz, dB, bars.
- Comments begin with #.
- Statements are newline-delimited; indentation has no semantic meaning.
- Strings use double quotes.

### Top-level forms

~~~
patch <name> {
  rack <version-range>
  require <plugin-slug> <version-range>
  module ...
  cable ...
  signal ...
  learn ...
  objective ...
  policy ...
}
~~~

A valid program must contain a module. A cable may only connect an existing module output to an existing module input. A parameter reference may only target a declared normal Rack parameter.

### Module declaration

~~~
module <instance> : <plugin>/<model> {
  at <x-hp>, <y-row>       # optional deterministic placement hint
  param <local-param> = <value>
  label <local-field> = "text"
  tag <name>[, <name>...]
}
~~~

Example:

~~~
module hidden-01 : tnn1t1s-ghost/AgentNeuron {
  at 26hp, 0row
  param weight-1 = 0.12
  param bias = -0.04
  param activation = "tanh"
  tag hidden, learnable
}
~~~

At is a layout hint only. It never selects controls or determines connectivity.

### Cables

~~~
cable <source>.<output> -> <destination>.<input> [as <name>]
~~~

Example:

~~~
cable sense.level -> feature-bus.level
cable feature-bus.level -> hidden-01.in-1 as level-to-hidden-01
~~~

Cables are singular ordinary connections in Version 1. The compiler rejects a Geese cable which would require a polyphonic carrier, implicit mixer, or implicit multiplier. Fan-out is explicit through normal output fan-out or an explicit FEATURE BUS.

### Signals

A signal gives an agent-readable meaning to an output, input, or host-derived metric. Signals are observations, not control permissions.

~~~
signal <name> = <endpoint> {
  rate <Hz>
  unit <unit>
  range <min> .. <max>
  role target | model | diagnostic
  summary mean | rms | last | peak
}
~~~

Example:

~~~
signal target.bright = sense.target-bright {
  rate 50Hz
  unit norm
  range 0.0 .. 1.0
  role target
  summary mean
}
~~~

Version 1 standard feature names are level, onset, bright, spread, noise, harm, flux, and motion. They are normalized descriptors, not a claim of perceptual completeness.

### Learnable parameters

~~~
learn <name> = <module>.<param> {
  owner base | adapt | learn
  bounds <min> .. <max>
  step <= <amount>
  slew <duration>
  group <name>
  loss-weight <number>
  unlock <manual | console>
}
~~~

Example:

~~~
learn h01-w1 = hidden-01.weight-1 {
  owner base
  bounds -1.0 .. 1.0
  step <= 0.10
  slew 250ms
  group hidden-weights
  unlock console
}
~~~

Bounds are stricter than, or equal to, the Rack parameter's intrinsic range. Step is maximum absolute movement in one accepted transaction. Slew is enforced while applying an accepted change.

### Objectives and policies

~~~
objective <name> {
  compare <target-signal> to <model-signal> using l1 | l2 | huber
  weight <number>
  window <duration>
}

policy <name> {
  cadence 1bar
  settle >= 250ms
  proposal max <n> params
  budget <n> evaluations
  accept if improvement >= <number>
  rollback after <n> failures
  scope <group>[, <group>...]
}
~~~

A policy selects a bounded numerical strategy, such as coordinate search, CMA-ES on a small adapter set, SPSA, or contextual bandit. It does not embed that strategy in Geese source. The strategy is reported in audit events with its seed and configuration.

## 6. Reference patch: target follower

The following is representative Geese. The compiler expands the loops into all 172 ordinary cables.

~~~geese
patch target-follower {
  rack "2.x"
  require tnn1t1s-ghost ">=0.1"

  module sense : tnn1t1s-ghost/AgentSense {
    at 0hp, 0row
    param feature-rate = 50Hz
    param window-ms = 80ms
  }

  module feature-bus : tnn1t1s-ghost/FeatureBus { at 18hp, 0row }

  for f in [level, onset, bright, spread, noise, harm, flux, motion] {
    cable sense.target-{f} -> feature-bus.{f}
  }

  for j in 1..8 {
    module hidden-{j:02} : tnn1t1s-ghost/AgentNeuron {
      at 26hp + (j - 1) * 18hp, 0row
      param bias = 0.0
      param activation = "tanh"
    }

    for i, f in enumerate([level, onset, bright, spread, noise, harm, flux, motion]) {
      cable feature-bus.{f} -> hidden-{j:02}.in-{i + 1}
      learn hidden-{j:02}-w{i + 1} = hidden-{j:02}.weight-{i + 1} {
        owner base
        bounds -1.0 .. 1.0
        step <= 0.10
        slew 250ms
        group hidden-base
        unlock console
      }
    }
    cable hidden-{j:02}.out -> ctrl-bus.hidden-{j:02}
  }

  for k, name in enumerate([pitch, wave, fm, cutoff, res, env, amp, drive, delay, reverb, mix, space]) {
    module ctrl-{name} : tnn1t1s-ghost/AgentCtrl {
      at 26hp + (k - 1) * 18hp, 1row
      label name = "{name}"
      param span = 5V
      param offset = 0V
      param slew = 250ms
    }

    for j in 1..8 {
      cable ctrl-bus.hidden-{j:02} -> ctrl-{name}.in-{j}
      learn ctrl-{name}-w{j} = ctrl-{name}.weight-{j} {
        owner adapt
        bounds -1.0 .. 1.0
        step <= 0.08
        slew 250ms
        group live-adapter
        unlock console
      }
    }
  }

  policy live-adapt {
    cadence 1bar
    settle >= 250ms
    proposal max 12 params
    budget 8 evaluations
    accept if improvement >= 0.01
    rollback after 3 failures
    scope live-adapter
  }
}
~~~

The base layer is calibrated from a controlled voice/model pairing before performance. The live adapter is deliberately small: it gives responsiveness without claiming that a 204-parameter arbitrary synth patch can be safely retrained from scratch at musical time.

## 7. Programmatic API

The API may be local JSON-RPC, in-process C++, or a bridge over a loopback connection. Its calls and return values must preserve these semantics.

### Read-only discovery

~~~ts
type PatchInfo = {
  patchId: string
  revision: number
  rackVersion: string
  modules: ModuleInfo[]
  cables: CableInfo[]
  parameters: ParameterInfo[]
  signals: SignalInfo[]
}

getPatch(): PatchInfo
getParameter(ref: string): ParameterState
sampleSignals(names: string[], windowMs: number): SignalFrame[]
getAudit(afterRevision?: number): AuditEvent[]
~~~

ParameterInfo includes module instance, Rack model, stable Rack parameter ID, Geese reference, physical range, declared Geese bounds, ownership, and current value. getPatch() exposes resolved Rack IDs and must flag a source/runtime mismatch.

### Patch construction

~~~ts
compile(source: string, options?: { validateOnly?: boolean }): {
  diagnostics: Diagnostic[]
  compiled?: CompiledPatch
}

instantiate(compiled: CompiledPatch, options?: {
  replacePatch?: boolean
  attachConsole?: boolean
}): { patchId: string; revision: number }
~~~

Construction is explicit. replacePatch defaults to false and requires host confirmation. Instantiation does not start learning.

### Learning session

~~~ts
startSession(config: {
  policy: string
  objective: string
  seed: number
  mode: "observe" | "propose"
}): { sessionId: string; revision: number }

stopSession(sessionId: string): void
hold(reason?: string): { revision: number }
releaseHold(): { revision: number }
~~~

Observe collects aligned feature and score history but changes nothing. Propose may create candidates under the selected policy. Console HOLD overrides all modes.

### Atomic parameter transactions

No API accepts a naked persistent setParam() for learner writes. Learners use transaction proposals.

~~~ts
type Change = {
  ref: string
  expectedRevision: number
  from: number
  to: number
  reason: string
}

propose(sessionId: string, changes: Change[]): Proposal
apply(proposalId: string): {
  transactionId: string
  settlingUntil: string
  revision: number
}

evaluate(transactionId: string, windowMs: number): Evaluation
commit(transactionId: string): { revision: number }
rollback(transactionId: string, reason: string): { revision: number }
~~~

The host rejects a proposal if the session is not in propose mode; HOLD is engaged; it exceeds policy size; a parameter is undeclared, manually owned, frozen, or out of scope; the expected revision is stale; the current value differs; a bound or step is violated; a manual gesture arrived; or a transaction is still settling.

Accepted changes apply as one revision with declared slew. Evaluation cannot begin before the settle period. Commit preserves values; rollback restores exact prior state with safe slew.

### Human control API

~~~ts
setOwner(ref: string, owner: "manual" | "learn" | "frozen" | "base" | "adapt"): void
lock(ref: string): void
unlock(ref: string): void
snapshot(name: string): Snapshot
restore(snapshotId: string): { revision: number }
~~~

Restore is a transaction and is visible in audit history. Touching a knob in Rack is semantically equivalent to setOwner(ref, "manual").

## 8. Agent contract

An agent is granted a precise contract:

1. Read resolved topology, features, current state, ownership, recent history, and audit events.
2. Select a declared policy and propose a small candidate set.
3. Wait for Rack-side settling.
4. Request evaluation.
5. Commit only an improvement that clears the threshold; otherwise roll back.
6. Append a compact explanation: intent, changed refs, score before/after, and decision.

For V1 live adaptation, coordinate/bandit/SPSA-style search over 12–40 selected output-layer values is preferred. Full-network training belongs to calibration or offline pass. An agent may choose a new live subset only through a human-visible ownership change.

## 9. Audit and reproducibility

Every mutable event is append-only:

~~~json
{
  "revision": 44,
  "kind": "commit",
  "session": "warm-plumage",
  "policy": "live-adapt",
  "seed": 921734,
  "changes": [{"ref": "ctrl-cutoff-w3", "from": 0.17, "to": 0.21}],
  "scoreBefore": 0.362,
  "scoreAfter": 0.335,
  "loss": {"bright": 0.19, "motion": 0.11},
  "note": "Reduced brightness mismatch without worsening motion."
}
~~~

An audit log includes rejected proposals, failed evaluations, and manual interventions. Those failures are valuable musical and engineering evidence.

## 10. Safety and failure semantics

- Audio continues if the agent, bridge, or external process disappears.
- A disconnect freezes pending work and leaves the last committed physical state intact.
- Module deletion, cable removal, model-version mismatch, or source/runtime divergence pauses the session with a diagnostic.
- NaN score, an unclamped parameter, or out-of-range voltage rolls back the active transaction and enters HOLD.
- AGENT CTRL defaults to bounded output and slews parameter changes. It never emits an unbounded correction because a learner requested it.
- No target audio leaves the local process unless an external service is explicitly configured.

## 11. Validation

An implementation must test source/schema diagnostics; resolution against live Rack metadata; deterministic expansion to 172 reference cables; invalid endpoint direction and missing model rejection; transaction atomicity; stale revision rejection; HOLD behavior; manual-gesture cancellation; settle delay; exact rollback; patch reload plus audit replay; and audio-thread safety.

## 12. Naming

Geese is practical rather than mystical: a flock has formation, noisy local interactions, clear visible members, and a collective shape beyond what one actor could draw by hand. Its vocabulary is patch, cable, signal, learn, policy, proposal, settle, commit, rollback, and flock, not magic, AI mood, or hidden intelligence.

The visual patch remains the star: a ridiculous, beautiful physical control graph that can be listened to, repatched, and argued with.
