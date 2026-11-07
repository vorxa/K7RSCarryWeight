#pragma once

// The settings menu (specification, section 13): a section in the window of SKSE Menu Framework.
namespace k7::rscw::ui
{
    // At kDataLoaded, after the localization tables are read: registers the section if the framework is loaded;
    // otherwise only the line of k7::skse::menu::Check() is in the log.
    void RegisterMenu();
}
