#pragma once
#include "model.h"

namespace openloch::pcb {
// OpenLoch's own example board, 80 × 50 mm: two numbered through-hole pads, an SMD pad, a track on the bottom copper
// to a via, the connection on the top copper on to the SMD pad, an asymmetric silkscreen mark, a component and the
// board outline. Every size is set explicitly here.
Document exampleDocument();
}
