#include <rack.hpp>

// TNN1T1S Ghost — a Rack-native drum system inspired by classic 909 behavior.
// Separate voices, shared accent, and a central control bus (GHOST CTRL).

// Forward declarations -- one per module source file
extern rack::Model* modelKck;
extern rack::Model* modelKckLab;
extern rack::Model* modelChhOhh;
extern rack::Model* modelChhOhhLab;
extern rack::Model* modelRimClap;
extern rack::Model* modelRimClapLab;
extern rack::Model* modelSnr;
extern rack::Model* modelSnrLab;
extern rack::Model* modelToms;
extern rack::Model* modelTomLab;
extern rack::Model* modelCrashRide;
extern rack::Model* modelCrashRideLab;
extern rack::Model* modelTr909Ctrl;

rack::Plugin* pluginInstance;

void init(rack::Plugin* p) {
    pluginInstance = p;

    // Control bus
    p->addModel(modelTr909Ctrl);

    // Voices (each ships a standard voice + an expanded LAB variant)
    p->addModel(modelKck);
    p->addModel(modelKckLab);
    p->addModel(modelSnr);
    p->addModel(modelSnrLab);
    p->addModel(modelChhOhh);
    p->addModel(modelChhOhhLab);
    p->addModel(modelRimClap);
    p->addModel(modelRimClapLab);
    p->addModel(modelToms);
    p->addModel(modelTomLab);
    p->addModel(modelCrashRide);
    p->addModel(modelCrashRideLab);
}
