#include "TerminationSignals.hpp"

#include <QSignalSpy>
#include <QTest>

#include <csignal>

using ghub::app::TerminationSignals;

namespace {

using Handler = void (*)(int);

Handler currentHandler(int signalNumber)
{
    struct sigaction current{};
    return ::sigaction(signalNumber, nullptr, &current) == 0 ? current.sa_handler : SIG_ERR;
}

bool dispositionIs(int signalNumber, Handler handler)
{
    return currentHandler(signalNumber) == handler;
}

} // namespace

class TerminationSignalsTest : public QObject {
    Q_OBJECT

private slots:
    void raisedSignalBecomesTerminationRequested_data()
    {
        QTest::addColumn<int>("signalNumber");
        QTest::newRow("SIGTERM") << SIGTERM;
        QTest::newRow("SIGINT") << SIGINT;
        QTest::newRow("SIGHUP") << SIGHUP;
    }

    void raisedSignalBecomesTerminationRequested()
    {
        QFETCH(int, signalNumber);
        TerminationSignals signals_;
        QVERIFY(signals_.isActive());
        QSignalSpy spy(&signals_, &TerminationSignals::terminationRequested);

        QCOMPARE(::raise(signalNumber), 0);

        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), signalNumber);
    }

    void eachRaiseIsDeliveredOnce()
    {
        TerminationSignals signals_;
        QSignalSpy spy(&signals_, &TerminationSignals::terminationRequested);

        QCOMPARE(::raise(SIGTERM), 0);
        QCOMPARE(::raise(SIGINT), 0);

        QTRY_COMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toInt(), SIGTERM);
        QCOMPARE(spy.at(1).at(0).toInt(), SIGINT);
    }

    void destructorRestoresPreviousDisposition()
    {
        QVERIFY(::signal(SIGHUP, SIG_IGN) != SIG_ERR);
        const Handler sigtermBefore = currentHandler(SIGTERM); // o QTest instala o próprio tratador
        {
            const TerminationSignals signals_;
            QVERIFY(signals_.isActive());
            QVERIFY(!dispositionIs(SIGHUP, SIG_IGN));
        }
        QVERIFY(dispositionIs(SIGHUP, SIG_IGN));
        QVERIFY(dispositionIs(SIGTERM, sigtermBefore));
        QVERIFY(::signal(SIGHUP, SIG_DFL) != SIG_ERR);
    }

    void secondInstanceIsInactiveAndChangesNothing()
    {
        TerminationSignals first;
        QSignalSpy firstSpy(&first, &TerminationSignals::terminationRequested);
        {
            const TerminationSignals second;
            QVERIFY(!second.isActive());
        }
        // O destrutor da segunda não pode desfazer os tratadores da primeira.
        QCOMPARE(::raise(SIGTERM), 0);
        QTRY_COMPARE(firstSpy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TerminationSignalsTest)
#include "TerminationSignalsTest.moc"
