#pragma once

/**
 * Utility class that stops the CRT and the OS from popping up interactive dialogs on crashes and failed assertions,
 * routing the reports to stderr instead. This is really only needed on Windows, and this class does nothing on POSIX.
 *
 * Without this, a failed `assert` / `_STL_VERIFY` in a debug build shows the modal "Debug Assertion Failed!"
 * (Abort/Retry/Ignore) dialog, and `abort` shows another one, so unattended runs (tests, CI) stall until someone
 * clicks a button.
 *
 * Use it like this:
 * ```
 * int main(int argc, char **argv) {
 *     NonInteractiveCrt _;
 *     // Asserts & aborts now go to stderr.
 * }
 * ```
 */
class NonInteractiveCrt {
 public:
    NonInteractiveCrt();
};
