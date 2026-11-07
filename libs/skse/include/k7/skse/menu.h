#pragma once

namespace k7::skse::menu
{
    // The in-game menus of the plugins need SKSE Menu Framework, but a plugin must work without it (soft
    // dependency: FR-2.2a of the K7InvVolLimits specification, section 13.1 of the K7RSCarryWeight one).
    // Nothing links against the framework; its exports are looked up by name at run time (see
    // third_party/SKSEMenuFramework) and only when the module is really loaded.
    //
    // Call after kDataLoaded (all SKSE plugins are loaded by then). Writes one line to the log: the version
    // found, or that the menu is unavailable (a warning when `missingIsWarning`, info otherwise).
    void Check(bool missingIsWarning = true);

    // True if the framework module is loaded in the process. Valid after Check().
    bool Available() noexcept;
}
