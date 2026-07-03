#include "NonInteractiveCrt.h"

#ifdef _WINDOWS
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#   include <crtdbg.h>

#   include <cstdio>
#   include <cstdlib>

static int __cdecl reportToStderrHook(int reportType, char *message, int *returnValue) {
    if (message)
        std::fputs(message, stderr);

    // Under a debugger, ask the caller to `_CrtDbgBreak`, which breaks right at the failed assertion.
    if (returnValue)
        *returnValue = IsDebuggerPresent() ? 1 : 0;

    // Unattended, a failed assertion would fast-fail after this hook returns, and fast-fails can't be intercepted.
    // Calling `abort` instead gives crash handlers (e.g. `StackTraceOnCrash`) a chance to print a stack trace.
    // The report `abort` itself emits is of type `_CRT_ERROR`, so this doesn't recurse.
    if (reportType == _CRT_ASSERT && !IsDebuggerPresent())
        std::abort();

    return TRUE; // Report was handled here, don't pop a dialog.
}
#endif

NonInteractiveCrt::NonInteractiveCrt() {
#ifdef _WINDOWS
    // Route debug-CRT assertion / error / warning reports to stderr instead of a modal dialog. No-op in release
    // builds, where the debug CRT report machinery doesn't exist.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportHook(reportToStderrHook);

    // `abort` should write its message to stderr, not show the Windows Error Reporting dialog.
    _set_abort_behavior(0, _CALL_REPORTFAULT);

    // And hard crashes (access violations etc.) shouldn't show the "has stopped working" dialog either.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
}
