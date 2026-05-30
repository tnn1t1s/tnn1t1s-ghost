#pragma once
#include <rack.hpp>

/**
 * AgentModule -- marker base class for Ghost modules.
 *
 * Ghost no longer requires a parallel JSON manifest. Runtime interface
 * truth lives in Rack's own config metadata: configParam(), configInput(),
 * and configOutput().
 *
 * A smaller semantics layer may be useful later, but only once there is a
 * proven need that Rack's native metadata cannot express cleanly.
 */
struct AgentModule : rack::Module {
};
