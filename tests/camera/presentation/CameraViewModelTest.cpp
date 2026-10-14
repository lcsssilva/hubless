#include "modules/camera/presentation/CameraViewModel.hpp"
#include "fakes/FakeCameraBackend.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

#include <chrono>
#include <optional>
#include <string_view>
#include <vector>

using namespace std::chrono_literals;
using ghub::camera::CameraControl;
using ghub::camera::CameraControlSpec;
using ghub::camera::CameraDescription;
using ghub::camera::CameraViewModel;
using ghub::camera::ControlKind;
using ghub::camera::FrameRate;
using ghub::camera::GetCameraStatus;
using ghub::camera::PixelFormat;
using ghub::camera::VideoMode;

namespace {

FrameRate fps(std::uint32_t frames, std::uint32_t seconds = 1)
{
    return FrameRate::create(frames, seconds).value();
}

VideoMode mode(std::string_view fourcc, std::uint32_t width, std::uint32_t height, std::vector<FrameRate> rates)
{
    return VideoMode::create(PixelFormat::create(fourcc).value(), width, height, std::move(rates)).value();
}

std::vector<FrameRate> allRates()
{
    return {fps(30), fps(24), fps(20), fps(15), fps(10), fps(15, 2), fps(5)};
}

CameraControl control(CameraControlSpec spec)
{
    return CameraControl::create(std::move(spec)).value();
}

// Subconjunto de modos e controles da Brio 100 lidos do hardware em 2026-10-13 (YUYV antes de MJPG, como no driver).
CameraDescription brio100()
{
    return {
        .name = "Brio 100",
        .modes =
            {
                mode("YUYV", 640, 480, allRates()),
                mode("YUYV", 160, 120, allRates()),
                mode("YUYV", 1280, 720, {fps(5)}),
                mode("YUYV", 1920, 1080, {fps(5)}),
                mode("MJPG", 640, 480, allRates()),
                mode("MJPG", 160, 120, allRates()),
                mode("MJPG", 1280, 720, allRates()),
                mode("MJPG", 1920, 1080, allRates()),
            },
        .controls =
            {
                control({
                    .id = 1,
                    .name = "Brightness",
                    .kind = ControlKind::Integer,
                    .minimum = 0,
                    .maximum = 255,
                    .step = 1,
                    .defaultValue = 128,
                    .currentValue = 128,
                    .inactive = false,
                    .readOnly = false,
                    .options = {},
                }),
                control({
                    .id = 2,
                    .name = "White Balance, Automatic",
                    .kind = ControlKind::Boolean,
                    .minimum = 0,
                    .maximum = 1,
                    .step = 1,
                    .defaultValue = 1,
                    .currentValue = 1,
                    .inactive = false,
                    .readOnly = false,
                    .options = {},
                }),
                control({
                    .id = 3,
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
                }),
                control({
                    .id = 4,
                    .name = "Auto Exposure",
                    .kind = ControlKind::Menu,
                    .minimum = 0,
                    .maximum = 3,
                    .step = 1,
                    .defaultValue = 3,
                    .currentValue = 3,
                    .inactive = false,
                    .readOnly = false,
                    .options = {{.value = 1, .label = "Manual Mode"}, {.value = 3, .label = "Aperture Priority Mode"}},
                }),
                control({
                    .id = 5,
                    .name = "White Balance Temperature",
                    .kind = ControlKind::Integer,
                    .minimum = 2800,
                    .maximum = 7500,
                    .step = 1,
                    .defaultValue = 4000,
                    .currentValue = 4000,
                    .inactive = true,
                    .readOnly = false,
                    .options = {},
                }),
            },
    };
}

CameraControlSpec integerSpec()
{
    return {
        .id = 1,
        .name = "Brightness",
        .kind = ControlKind::Integer,
        .minimum = 0,
        .maximum = 255,
        .step = 1,
        .defaultValue = 128,
        .currentValue = 128,
        .inactive = false,
        .readOnly = false,
        .options = {},
    };
}

CameraControlSpec menuSpec()
{
    return {
        .id = 4,
        .name = "Auto Exposure",
        .kind = ControlKind::Menu,
        .minimum = 0,
        .maximum = 3,
        .step = 1,
        .defaultValue = 3,
        .currentValue = 3,
        .inactive = false,
        .readOnly = false,
        .options = {{.value = 1, .label = "Manual Mode"}, {.value = 3, .label = "Aperture Priority Mode"}},
    };
}

CameraDescription withControls(std::vector<CameraControl> controls)
{
    return {.name = "Brio 100", .modes = {}, .controls = std::move(controls)};
}

CameraDescription withModes(std::vector<VideoMode> modes)
{
    return {.name = "Brio 100", .modes = std::move(modes), .controls = {}};
}

QString modeText(const CameraViewModel& viewModel, qsizetype index)
{
    return viewModel.modes().at(index).toMap().value("text").toString();
}

QString controlSummary(const CameraViewModel& viewModel, qsizetype index)
{
    return viewModel.controls().at(index).toMap().value("summary").toString();
}

class CameraViewModelTest : public QObject {
    Q_OBJECT

private slots:
    void reportsNotDetected()
    {
        backend.setResult(std::optional<CameraDescription>{});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(viewModel.status(), QStringLiteral("não detectada"));
        QVERIFY(viewModel.modes().isEmpty());
        QVERIFY(viewModel.controls().isEmpty());
    }

    void reportsUnavailable()
    {
        backend.setResult(ghub::shared_kernel::fail(ghub::shared_kernel::ErrorCode::BackendUnavailable, "v4l2"));

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(viewModel.status(), QStringLiteral("leitura indisponível"));
        QVERIFY(viewModel.modes().isEmpty());
        QVERIFY(viewModel.controls().isEmpty());
    }

    void showsCameraName_WhenReady()
    {
        backend.setResult(std::optional{brio100()});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(viewModel.status(), QStringLiteral("Brio 100"));
    }

    void formatsOneModeLinePerPixelFormat()
    {
        backend.setResult(std::optional{brio100()});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(viewModel.modes().size(), 2);
        QCOMPARE(modeText(viewModel, 0), QStringLiteral("YUYV: até 1920×1080 a 5 fps"));
        QCOMPARE(modeText(viewModel, 1), QStringLiteral("MJPG: até 1920×1080 a 30 fps"));
    }

    void formatsFractionalRate()
    {
        backend.setResult(std::optional{withModes({mode("MJPG", 640, 480, {fps(15, 2)})})});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(modeText(viewModel, 0), QStringLiteral("MJPG: até 640×480 a 7,5 fps"));
    }

    void formatsNtscRateWithTwoDecimals()
    {
        backend.setResult(std::optional{withModes({mode("MJPG", 640, 480, {fps(30000, 1001)})})});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(modeText(viewModel, 0), QStringLiteral("MJPG: até 640×480 a 29,97 fps"));
    }

    void listsControlsInDescriptionOrder()
    {
        backend.setResult(std::optional{brio100()});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(viewModel.controls().size(), 5);
        QCOMPARE(viewModel.controls().at(0).toMap().value("name").toString(), QStringLiteral("Brightness"));
        QCOMPARE(viewModel.controls().at(4).toMap().value("name").toString(),
                 QStringLiteral("White Balance Temperature"));
    }

    void summarizesIntegerControl()
    {
        backend.setResult(std::optional{brio100()});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(controlSummary(viewModel, 0), QStringLiteral("128 (0–255, padrão 128)"));
    }

    void summarizesBooleanControl()
    {
        auto off = integerSpec();
        off.name = "Auto";
        off.kind = ControlKind::Boolean;
        off.maximum = 1;
        off.defaultValue = 1;
        off.currentValue = 0;
        backend.setResult(std::optional{brio100()});
        const CameraViewModel on{getCameraStatus, 1h};
        backend.setResult(std::optional{withControls({control(off)})});
        const CameraViewModel disabled{getCameraStatus, 1h};

        QCOMPARE(controlSummary(on, 1), QStringLiteral("ligado"));
        QCOMPARE(controlSummary(disabled, 0), QStringLiteral("desligado"));
    }

    void summarizesMenuControl_WithCurrentOptionLabel()
    {
        backend.setResult(std::optional{brio100()});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(controlSummary(viewModel, 2), QStringLiteral("60 Hz"));
        QCOMPARE(controlSummary(viewModel, 3), QStringLiteral("Aperture Priority Mode"));
    }

    void summarizesMenuControl_WithNumber_WhenNoOptionMatches()
    {
        auto spec = menuSpec();
        spec.currentValue = 2;
        backend.setResult(std::optional{withControls({control(spec)})});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(controlSummary(viewModel, 0), QStringLiteral("2"));
    }

    void marksInactiveControl()
    {
        backend.setResult(std::optional{brio100()});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(controlSummary(viewModel, 4), QStringLiteral("4000 (2800–7500, padrão 4000) · inativo"));
    }

    void marksReadOnlyControl()
    {
        auto spec = integerSpec();
        spec.readOnly = true;
        backend.setResult(std::optional{withControls({control(spec)})});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(controlSummary(viewModel, 0), QStringLiteral("128 (0–255, padrão 128) · somente leitura"));
    }

    void marksInactiveBeforeReadOnly()
    {
        auto spec = integerSpec();
        spec.inactive = true;
        spec.readOnly = true;
        backend.setResult(std::optional{withControls({control(spec)})});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(controlSummary(viewModel, 0), QStringLiteral("128 (0–255, padrão 128) · inativo · somente leitura"));
    }

    void reportsUnavailableValue()
    {
        auto integer = integerSpec();
        integer.currentValue = std::nullopt;
        auto menu = menuSpec();
        menu.currentValue = std::nullopt;
        backend.setResult(std::optional{withControls({control(integer), control(menu)})});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(controlSummary(viewModel, 0), QStringLiteral("valor indisponível (0–255, padrão 128)"));
        QCOMPARE(controlSummary(viewModel, 1), QStringLiteral("valor indisponível"));
    }

    void exposesIntegerControlFieldsForEditing()
    {
        backend.setResult(std::optional{brio100()});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        const auto map = viewModel.controls().at(0).toMap();
        QCOMPARE(map.value("id").toUInt(), 1U);
        QCOMPARE(map.value("kind").toString(), QStringLiteral("integer"));
        QCOMPARE(map.value("minimum").toLongLong(), 0);
        QCOMPARE(map.value("maximum").toLongLong(), 255);
        QCOMPARE(map.value("step").toLongLong(), 1);
        QCOMPARE(map.value("defaultValue").toLongLong(), 128);
        QCOMPARE(map.value("value").toLongLong(), 128);
        QVERIFY(map.value("options").toList().isEmpty());
        QCOMPARE(map.value("editable").toBool(), true);
    }

    void exposesBooleanAndMenuKinds_AndOptions()
    {
        backend.setResult(std::optional{brio100()});

        const CameraViewModel viewModel{getCameraStatus, 1h};

        QCOMPARE(viewModel.controls().at(1).toMap().value("kind").toString(), QStringLiteral("boolean"));
        const auto menu = viewModel.controls().at(2).toMap();
        QCOMPARE(menu.value("kind").toString(), QStringLiteral("menu"));
        const auto options = menu.value("options").toList();
        QCOMPARE(options.size(), 3);
        QCOMPARE(options.at(1).toMap().value("value").toLongLong(), 1);
        QCOMPARE(options.at(1).toMap().value("label").toString(), QStringLiteral("50 Hz"));
        QCOMPARE(menu.value("value").toLongLong(), 2);
    }

    void marksControlNotEditable_WhenInactiveReadOnlyOrWithoutValue()
    {
        auto readOnly = integerSpec();
        readOnly.readOnly = true;
        auto noValue = menuSpec();
        noValue.currentValue = std::nullopt;
        backend.setResult(std::optional{brio100()});
        const CameraViewModel inactive{getCameraStatus, 1h};
        backend.setResult(std::optional{withControls({control(readOnly), control(noValue)})});
        const CameraViewModel others{getCameraStatus, 1h};

        QCOMPARE(inactive.controls().at(4).toMap().value("editable").toBool(), false);
        QCOMPARE(others.controls().at(0).toMap().value("editable").toBool(), false);
        const auto withoutValue = others.controls().at(1).toMap();
        QCOMPARE(withoutValue.value("editable").toBool(), false);
        QVERIFY(!withoutValue.value("value").isValid());
    }

    void emitsChanged_OnlyWhenSomethingChanges()
    {
        backend.setResult(std::optional{brio100()});
        CameraViewModel viewModel{getCameraStatus, 1h};
        QSignalSpy changed(&viewModel, &CameraViewModel::cameraChanged);

        viewModel.refresh();
        QCOMPARE(changed.count(), 0);

        auto other = brio100();
        other.name = "Brio 100 (2)";
        backend.setResult(std::optional{other});
        viewModel.refresh();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(viewModel.status(), QStringLiteral("Brio 100 (2)"));
    }

    void emitsChanged_WhenCameraDisappears()
    {
        backend.setResult(std::optional{brio100()});
        CameraViewModel viewModel{getCameraStatus, 1h};
        QSignalSpy changed(&viewModel, &CameraViewModel::cameraChanged);
        backend.setResult(std::optional<CameraDescription>{});

        viewModel.refresh();

        QCOMPARE(changed.count(), 1);
        QCOMPARE(viewModel.status(), QStringLiteral("não detectada"));
        QVERIFY(viewModel.controls().isEmpty());
    }

    void readsOnConstruction_AndOnEachRefresh()
    {
        const int before = backend.calls();
        CameraViewModel viewModel{getCameraStatus, 1h};
        QCOMPARE(backend.calls(), before + 1);

        viewModel.refresh();

        QCOMPARE(backend.calls(), before + 2);
    }

    void refreshesPeriodically()
    {
        CameraViewModel viewModel{getCameraStatus, 10ms};
        QSignalSpy changed(&viewModel, &CameraViewModel::cameraChanged);
        backend.setResult(std::optional{brio100()});

        QVERIFY(changed.wait(5000));
        QCOMPARE(viewModel.status(), QStringLiteral("Brio 100"));
    }

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): `private slots:` acima é `private:` para o clang.
private:
    ghub::fakes::FakeCameraBackend backend;
    GetCameraStatus getCameraStatus{backend};
};

} // namespace

QTEST_GUILESS_MAIN(CameraViewModelTest)
#include "CameraViewModelTest.moc"
