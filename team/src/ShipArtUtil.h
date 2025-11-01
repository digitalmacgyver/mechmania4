/* ShipArtUtil.h
 * Utilities for discovering and selecting ship art sets.
 */

#ifndef _SHIP_ART_UTIL_H_
#define _SHIP_ART_UTIL_H_

#include <set>
#include <string>
#include <vector>

namespace shipart {

// Discover available ship art packs. Each entry is in "<faction>:<ship>" form.
// The optional assetsRootOverride allows callers to provide the parsed
// --assets-root value so both clients and server share the same search roots.
std::vector<std::string> DiscoverShipArtOptions(
    const std::string& assetsRootOverride = std::string());

// Canonicalize a user provided ship-art request. Returns an empty string when
// the request does not match any known art packs. Supports legacy aliases
// such as "mm4orange" and "mm4blue".
std::string CanonicalizeShipArtRequest(
    const std::string& request,
    const std::vector<std::string>& availableOptions);

// Choose a random art pack from the provided list, excluding any entries whose
// lowercase representation appears in excludeLower. When every option is
// excluded the first available entry is returned to guarantee progress.
std::string ChooseRandomShipArt(
    const std::vector<std::string>& availableOptions,
    const std::set<std::string>& excludeLower = {});

// Helpers exposed for callers that need to manage exclusion sets.
std::string ToLower(const std::string& value);
bool EqualsIgnoreCase(const std::string& lhs, const std::string& rhs);

}  // namespace shipart

#endif  // _SHIP_ART_UTIL_H_
