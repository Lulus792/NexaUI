/* Baseline-only core symbol isolation; never part of the installed library. */
#ifdef NEXAUI_SHARED
#undef NEXAUI_SHARED
#endif
#include NEXAUI_CORE_BASELINE_PREFIX
#include NEXAUI_CORE_BASELINE_PATH
