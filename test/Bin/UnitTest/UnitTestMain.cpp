#include <gtest/gtest.h>

#include "Library/StackTrace/StackTraceOnCrash.h"

#include "Utility/NonInteractiveCrt.h"
#include "Utility/UnicodeCrt.h"

GTEST_API_ int main(int argc, char **argv) {
    StackTraceOnCrash st;
    NonInteractiveCrt nonInteractiveCrt; // Tests run unattended, a failed assert shouldn't block on a dialog.
    UnicodeCrt _(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
