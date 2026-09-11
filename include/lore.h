#pragma once
#include "game.h"
namespace pond {
// Authored, original wasteland fiction. Labels supplement v1 identities; never change saves.
struct LorePage { const char* title; const char* lines[6]; };
LorePage dossier(const Catch& c, unsigned page, uint32_t knownObjects = 0);
unsigned fishEvidence(unsigned species);
const LorePage* fishAnnotation(const Catch&,uint32_t objects);
LorePage siteDossier(unsigned spot);
LorePage eventDossier(uint32_t code);
const char* eventName(Anomaly event);
unsigned evidenceNext(unsigned objectType);
constexpr unsigned AnnotationCount = 12;
uint32_t annotationBit(const Catch&, uint32_t knownObjects);
uint32_t unlockedAnnotations(uint32_t knownObjects);
const LorePage* annotationFor(const Catch& c, uint32_t knownObjects);
}
