// QDBus precisa de um QCoreApplication; o resto é GoogleTest.
#include <QCoreApplication>

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    const QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
