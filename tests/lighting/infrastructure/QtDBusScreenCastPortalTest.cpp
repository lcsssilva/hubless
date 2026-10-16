// Partes puras do adaptador QtDBus do portal ScreenCast (caminhos dos objetos e leitura da resposta do Start). A
// conversa real com o portal não é testada aqui: nenhum teste chama o portal nem captura a tela.
#include "modules/lighting/infrastructure/QtDBusScreenCastPortal.hpp"

#include <gtest/gtest.h>

#include <QVariantMap>

namespace {

using ghub::lighting::infrastructure::QtDBusScreenCastPortal;
using ghub::shared_kernel::ErrorCode;

TEST(QtDBusScreenCastPortalPathsTest, SenderTokenDropsTheColonAndReplacesDotsWithUnderscores)
{
    EXPECT_EQ(QtDBusScreenCastPortal::senderToken(QStringLiteral(":1.42")), QStringLiteral("1_42"));
    EXPECT_EQ(QtDBusScreenCastPortal::senderToken(QStringLiteral(":1.2.3")), QStringLiteral("1_2_3"));
}

TEST(QtDBusScreenCastPortalPathsTest, RequestPathFollowsThePortalDocumentation)
{
    EXPECT_EQ(QtDBusScreenCastPortal::requestPath(QStringLiteral(":1.42"), QStringLiteral("ghub_ab12")),
              QStringLiteral("/org/freedesktop/portal/desktop/request/1_42/ghub_ab12"));
}

TEST(QtDBusScreenCastPortalPathsTest, TokensAreUniqueAndUseOnlyPathSafeCharacters)
{
    const auto first = QtDBusScreenCastPortal::newToken();
    const auto second = QtDBusScreenCastPortal::newToken();

    EXPECT_NE(first, second);
    EXPECT_TRUE(first.startsWith(QStringLiteral("ghub_")));
    for (const auto character : first) {
        EXPECT_TRUE(character.isLetterOrNumber() || character == QLatin1Char('_')) << qPrintable(first);
    }
}

TEST(QtDBusScreenCastPortalStartResultTest, MissingStreamsIsBackendUnavailable)
{
    const auto node = QtDBusScreenCastPortal::firstStreamNode(QVariantMap{});

    ASSERT_FALSE(node.has_value());
    EXPECT_EQ(node.error().code, ErrorCode::BackendUnavailable);
}

TEST(QtDBusScreenCastPortalStartResultTest, ResultsNeverCarryOrKeepARestoreToken)
{
    // O adaptador não lê `restore_token`: só o nó. Um resultado só com o token não dá stream.
    QVariantMap results;
    results.insert(QStringLiteral("restore_token"), QStringLiteral("segredo"));

    EXPECT_FALSE(QtDBusScreenCastPortal::firstStreamNode(results).has_value());
}

} // namespace
