#include <rack.hpp>

// TNN1T1S Ghost — a Rack-native drum system inspired by classic 909 behavior.
// Separate voices, shared accent, and a central control bus (GHOST CTRL).

// Forward declarations -- the GHOST core system (one per shipped module).
// Lab variants and the Attenuate reference module exist in the source tree but
// are intentionally not registered, so the browser shows only the core kit.
// EXCEPTION: KckLab is registered as a tuning bench while the kick is being
// dialed in to 909 fidelity; unregister before release.
extern rack::Model* modelKck;
extern rack::Model* modelKckLab;
extern rack::Model* modelSnr;
extern rack::Model* modelChhOhh;
extern rack::Model* modelRimClap;
extern rack::Model* modelToms;
extern rack::Model* modelCrashRide;
extern rack::Model* modelTr909Ctrl;
extern rack::Model* modelGhostMix;

rack::Plugin* pluginInstance;

void init(rack::Plugin* p) {
    pluginInstance = p;

    // GHOST core: six voices + the control bus.
    p->addModel(modelKck);
    p->addModel(modelKckLab);   // tuning bench (temporary; unregister before release)
    p->addModel(modelSnr);
    p->addModel(modelChhOhh);
    p->addModel(modelRimClap);
    p->addModel(modelToms);
    p->addModel(modelCrashRide);
    p->addModel(modelTr909Ctrl);
    p->addModel(modelGhostMix);   // dedicated 909-kit summing mixer
}
