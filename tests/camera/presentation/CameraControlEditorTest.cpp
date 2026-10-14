#include "modules/camera/presentation/CameraControlEditor.hpp"
#include "fakes/FakeCameraControlAccess.hpp"
#include "fakes/FakeClock.hpp"

#include <QSignalSpy>
#include <QTest>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

using ghub::camera::CameraControlEditor;
using ghub::camera::ControlKind;
using ghub::camera::SetCameraControl;
using ghub::device_access::AccessRegistry;
using ghub::device_access::PhysicalInterfaceId;
using ghub::shared_kernel::DomainError;
using ghub::shared_kernel::ErrorCode;

namespace {

constexpr std::uint32_t brightnessId = 1;
constexpr std::uint32_t switchId = 2;
constexpr std::uint32_t menuId = 3;
constexpr std::uint32_t inactiveId = 4;

// Porta que bloqueia a thread de trabalho até o teste liberar (sem sleep).
class Gate {
public:
    void wait()
    {
        entered_.store(true);
        std::unique_lock lock{mutex_};
        condition_.wait(lock, [this] { return open_; });
    }

    void open()
    {
        {
            const std::scoped_lock lock{mutex_};
            open_ = true;
        }
        condition_.notify_all();
    }

    [[nodiscard]] bool entered() const { return entered_.load(); }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    bool open_ = false;
    std::atomic<bool> entered_{false};
};

class CameraControlEditorTest : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        access.controls = {
            {
                .id = brightnessId,
                .name = "Brightness",
                .kind = ControlKind::Integer,
                .minimum = 0,
                .maximum = 255,
                .step = 1,
                .defaultValue = 128,
                .currentValue = 100,
                .inactive = false,
                .readOnly = false,
                .options = {},
            },
            {
                .id = switchId,
                .name = "Auto White Balance",
                .kind = ControlKind::Boolean,
                .minimum = 0,
                .maximum = 1,
                .step = 1,
                .defaultValue = 1,
                .currentValue = 1,
                .inactive = false,
                .readOnly = false,
                .options = {},
            },
            {
                .id = menuId,
                .name = "Power Line Frequency",
                .kind = ControlKind::Menu,
                .minimum = 0,
                .maximum = 2,
                .step = 1,
                .defaultValue = 2,
                .currentValue = 2,
                .inactive = false,
                .readOnly = false,
                .options =
                    {
                        {.value = 0, .label = "Disabled"},
                        {.value = 1, .label = "50 Hz"},
                        {.value = 2, .label = "60 Hz"},
                    },
            },
            {
                .id = inactiveId,
                .name = "Exposure Time",
                .kind = ControlKind::Integer,
                .minimum = 5,
                .maximum = 2500,
                .step = 1,
                .defaultValue = 156,
                .currentValue = 156,
                .inactive = true,
                .readOnly = false,
                .options = {},
            },
        };
        access.writes.clear();
        access.writeError.reset();
        access.onWrite = nullptr;
    }

    void doesNotWrite_OnConstruction()
    {
        const CameraControlEditor editor{setCameraControl};

        QCOMPARE(access.opens, std::size_t{0});
        QVERIFY(access.writes.empty());
        QVERIFY(!editor.busy());
        QVERIFY(editor.status().isEmpty());
    }

    void setValue_WritesThroughUseCase_AndReportsReadBackValue()
    {
        CameraControlEditor editor{setCameraControl};
        QSignalSpy written(&editor, &CameraControlEditor::controlWritten);

        editor.setValue(brightnessId, 150);
        QTRY_COMPARE(written.count(), 1);

        QCOMPARE(access.writes.size(), std::size_t{1});
        QCOMPARE(access.writes.front().id, brightnessId);
        QCOMPARE(access.writes.front().value, 150);
        QCOMPARE(editor.status(), QStringLiteral("Brightness: 150"));
        QVERIFY(!editor.busy());
    }

    void status_UsesOnOffForBoolean_AndLabelForMenu()
    {
        CameraControlEditor editor{setCameraControl};
        QSignalSpy written(&editor, &CameraControlEditor::controlWritten);

        editor.setValue(switchId, 0);
        QTRY_COMPARE(written.count(), 1);
        QCOMPARE(editor.status(), QStringLiteral("Auto White Balance: desligado"));

        editor.setValue(menuId, 1);
        QTRY_COMPARE(written.count(), 2);
        QCOMPARE(editor.status(), QStringLiteral("Power Line Frequency: 50 Hz"));
    }

    void restoreDefault_WritesTheDefaultValue()
    {
        CameraControlEditor editor{setCameraControl};
        QSignalSpy written(&editor, &CameraControlEditor::controlWritten);

        editor.restoreDefault(brightnessId);
        QTRY_COMPARE(written.count(), 1);

        QCOMPARE(access.writes.size(), std::size_t{1});
        QCOMPARE(access.writes.front().value, 128);
        QCOMPARE(editor.status(), QStringLiteral("Brightness: 128"));
    }

    void rejectedValue_ReportsFailure_WithoutWriting()
    {
        CameraControlEditor editor{setCameraControl};
        QSignalSpy written(&editor, &CameraControlEditor::controlWritten);

        editor.setValue(brightnessId, 999);
        QTRY_COMPARE(written.count(), 1);

        QVERIFY(access.writes.empty());
        QCOMPARE(editor.status(), QStringLiteral("valor recusado"));
        QVERIFY(!editor.busy());
    }

    void inactiveControl_IsRejected_WithoutWriting()
    {
        CameraControlEditor editor{setCameraControl};
        QSignalSpy written(&editor, &CameraControlEditor::controlWritten);

        editor.setValue(inactiveId, 200);
        QTRY_COMPARE(written.count(), 1);

        QVERIFY(access.writes.empty());
        QCOMPARE(editor.status(), QStringLiteral("valor recusado"));
    }

    void mapsErrorCodesToShortTexts()
    {
        struct Case {
            ErrorCode code;
            const char* text;
        };
        const std::vector<Case> cases{
            {.code = ErrorCode::DeviceBusy, .text = "câmera ocupada por outra escrita"},
            {.code = ErrorCode::InvalidArgument, .text = "valor recusado"},
            {.code = ErrorCode::LeaseExpired, .text = "escrita cancelada"},
            {.code = ErrorCode::LeaseRevoked, .text = "escrita cancelada"},
            {.code = ErrorCode::BackendUnavailable, .text = "câmera indisponível"},
        };
        for (const auto& item : cases) {
            CameraControlEditor editor{setCameraControl};
            QSignalSpy written(&editor, &CameraControlEditor::controlWritten);
            access.writeError = DomainError{.code = item.code, .detail = "detalhe interno"};

            editor.setValue(brightnessId, 150);
            QTRY_COMPARE(written.count(), 1);

            QCOMPARE(editor.status(), QString::fromUtf8(item.text));
            QVERIFY(!editor.status().contains(QStringLiteral("detalhe")));
        }
    }

    void doesNotRetry_AfterFailure()
    {
        CameraControlEditor editor{setCameraControl};
        QSignalSpy written(&editor, &CameraControlEditor::controlWritten);
        access.writeError = DomainError{.code = ErrorCode::DeviceBusy, .detail = "ocupado"};

        editor.setValue(brightnessId, 150);
        QTRY_COMPARE(written.count(), 1);
        QCoreApplication::processEvents();

        QCOMPARE(written.count(), 1);
        QVERIFY(access.writes.empty());
    }

    void requestsDuringOperation_AreCoalescedIntoTheLastOne()
    {
        Gate gate;
        access.onWrite = [&gate] { gate.wait(); };
        CameraControlEditor editor{setCameraControl};
        QSignalSpy written(&editor, &CameraControlEditor::controlWritten);
        QSignalSpy changed(&editor, &CameraControlEditor::editorChanged);

        editor.setValue(brightnessId, 10);
        QVERIFY(editor.busy());
        QTRY_VERIFY(gate.entered());
        editor.setValue(brightnessId, 20);
        editor.setValue(brightnessId, 30);
        QVERIFY(editor.busy());
        QVERIFY(changed.count() >= 1);
        gate.open();
        QTRY_COMPARE(written.count(), 2);
        QTRY_VERIFY(!editor.busy());

        QCOMPARE(access.writes.size(), std::size_t{2});
        QCOMPARE(access.writes.at(0).value, 10);
        QCOMPARE(access.writes.at(1).value, 30);
        QCOMPARE(editor.status(), QStringLiteral("Brightness: 30"));
    }

    void destroyingDuringOperation_WaitsAndDropsPendingRequest()
    {
        Gate gate;
        access.onWrite = [&gate] { gate.wait(); };
        auto editor = std::make_unique<CameraControlEditor>(setCameraControl);

        editor->setValue(brightnessId, 10);
        QTRY_VERIFY(gate.entered());
        editor->setValue(brightnessId, 20);
        gate.open();
        editor.reset(); // espera a escrita em andamento; o pedido pendente é descartado

        QCOMPARE(access.writes.size(), std::size_t{1});
        QCOMPARE(access.writes.front().value, 10);
        QCOMPARE(access.liveSessions, 0);
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    ghub::fakes::FakeClock clock;
    AccessRegistry registry{clock};
    ghub::fakes::FakeCameraControlAccess access;
    SetCameraControl setCameraControl{access, registry, PhysicalInterfaceId::create("v4l2:046d:094c").value()};
};

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): o construtor do teste usa Result::value() em dado constante válido.
QTEST_GUILESS_MAIN(CameraControlEditorTest)
#include "CameraControlEditorTest.moc"
