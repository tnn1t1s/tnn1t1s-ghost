# src/ghost/signal/

Audio signal utilities shared by the voice engines (`Audio.hpp`): normalization,
conversion to Rack volts, denormal floors, and related helpers. Header-only, no
Rack GUI dependency (so the stress harness can drive it without libRack).
