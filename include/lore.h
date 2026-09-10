#pragma once
#include "game.h"
namespace pond {
// Authored, original wasteland fiction. Labels supplement v1 identities; never change saves.
struct LorePage { const char* title; const char* lines[6]; };
LorePage dossier(const Catch& c, unsigned page);
unsigned evidenceNext(unsigned objectType);
}
