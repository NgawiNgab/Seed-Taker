#pragma once

// Version-specific offsets and vtable slots for MCPE 1.26.62 (arm64).
//
// Status: UNKNOWN — nothing here is verified against the 1.26.62 binary yet.
// Entries are added only after Zaphkiel binary evidence (investigation
// ST-001, see docs/EVIDENCE_LOG.md).
//
// Do NOT copy offsets from BedrockTools: they target a different game build
// and must be treated as inapplicable until re-verified.

#include <cstddef>

namespace seedtaker::sdk::offsets {

namespace VTable {
// Pending ST-001.
}

}
