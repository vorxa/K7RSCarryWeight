# SKSE Menu Framework API header

- File: `SKSEMenuFramework.h`, version 3.15 (SKSE Menu Framework mod, Nexus id 120352,
  file "SKSEMenuFramework Header", uploaded 2026-09-17). Identical to `include/SKSEMenuFramework.h`
  in https://github.com/QTR-Modding/SKSE-Menu-Framework-3-Example (commit aa8effa).
- Author: QTR-Modding. License: LGPL-2.1, text in `LICENSE`, taken from
  https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API (that repository's header is older
  than 3.15, the license is assumed to cover the API as a whole).
- Include it after CommonLibSSE and `<filesystem>`/`<atomic>`: the header uses `std::filesystem`,
  `std::atomic` and `RE::InputEvent` without including them.
