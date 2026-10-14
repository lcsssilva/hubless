#include "modules/camera/infrastructure/FrameDecoder.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QTest>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <string_view>
#include <vector>

using ghub::camera::decodeFrame;
using ghub::camera::FrameRate;
using ghub::camera::PixelFormat;
using ghub::camera::PreviewMode;

namespace {

PreviewMode modeOf(std::string_view fourcc, std::uint32_t width, std::uint32_t height)
{
    return {
        .format = PixelFormat::create(fourcc).value(),
        .width = width,
        .height = height,
        .frameRate = FrameRate::create(30, 1).value(),
    };
}

std::vector<std::byte> bytes(std::initializer_list<unsigned> values)
{
    std::vector<std::byte> out;
    for (const unsigned value : values) {
        out.push_back(static_cast<std::byte>(value));
    }
    return out;
}

std::vector<std::byte> jpegOf(int width, int height, const QColor& color)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(color);
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    const bool saved = image.save(&buffer, "JPG", 95);
    Q_ASSERT(saved);
    Q_UNUSED(saved)
    const auto bytes = std::as_bytes(std::span{encoded.constData(), static_cast<std::size_t>(encoded.size())});
    return {bytes.begin(), bytes.end()};
}

// JPEG tem perda: compara canal a canal com tolerância explícita.
bool closeTo(QRgb actual, const QColor& expected, int tolerance)
{
    return std::abs(qRed(actual) - expected.red()) <= tolerance &&
           std::abs(qGreen(actual) - expected.green()) <= tolerance &&
           std::abs(qBlue(actual) - expected.blue()) <= tolerance;
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class FrameDecoderTest : public QObject {
    Q_OBJECT

private slots:
    void yuyv2x2_KnownPixels()
    {
        // Linha 0: preto (Y=16) e branco (Y=235), croma neutro. Linha 1: um par vermelho saturado (Y=81 U=90 V=240).
        const auto data = bytes({16, 128, 235, 128, 81, 90, 81, 240});
        const auto image = decodeFrame(modeOf("YUYV", 2, 2), data);

        QCOMPARE(image.size(), QSize(2, 2));
        QCOMPARE(image.format(), QImage::Format_RGB32);
        QCOMPARE(image.pixel(0, 0), qRgb(0, 0, 0));
        QCOMPARE(image.pixel(1, 0), qRgb(255, 255, 255));
        QCOMPARE(image.pixel(0, 1), qRgb(255, 0, 0));
        QCOMPARE(image.pixel(1, 1), qRgb(255, 0, 0));
    }

    void yuyv4x2_BlueAndRowOrder()
    {
        // Linha 0: azul saturado (Y=41 U=240 V=110) e cinza (Y=126 neutro, ~128). Linha 1: tudo preto.
        const auto data = bytes({41, 240, 41, 110, 126, 128, 126, 128, 16, 128, 16, 128, 16, 128, 16, 128});
        const auto image = decodeFrame(modeOf("YUYV", 4, 2), data);

        QCOMPARE(image.size(), QSize(4, 2));
        QCOMPARE(image.pixel(0, 0), qRgb(0, 0, 255));
        QCOMPARE(image.pixel(1, 0), qRgb(0, 0, 255));
        QCOMPARE(image.pixel(2, 0), qRgb(128, 128, 128));
        QCOMPARE(image.pixel(3, 1), qRgb(0, 0, 0));
    }

    void yuyv_SaturatesOutOfRangeValues()
    {
        // Y=255 passa de 255 em todos os canais; Y=0 fica abaixo de 0.
        const auto data = bytes({255, 128, 0, 128});
        const auto image = decodeFrame(modeOf("YUYV", 2, 1), data);

        QCOMPARE(image.pixel(0, 0), qRgb(255, 255, 255));
        QCOMPARE(image.pixel(1, 0), qRgb(0, 0, 0));
    }

    void yuyv_WrongDataSize_IsNull()
    {
        QVERIFY(decodeFrame(modeOf("YUYV", 2, 2), bytes({16, 128, 235, 128, 81, 90, 81})).isNull());
        QVERIFY(decodeFrame(modeOf("YUYV", 2, 2), bytes({16, 128, 235, 128, 81, 90, 81, 240, 0})).isNull());
    }

    void yuyv_OddWidth_IsNull()
    {
        // 3x2 teria 12 bytes; o tamanho bate, mas a largura ímpar não forma pares Y0 U Y1 V.
        const std::vector<std::byte> data(12, std::byte{16});
        QVERIFY(decodeFrame(modeOf("YUYV", 3, 2), data).isNull());
    }

    void mjpg_ValidFrame_HasModeSizeAndApproxColor()
    {
        const QColor color(200, 50, 50);
        const auto data = jpegOf(16, 8, color);
        const auto image = decodeFrame(modeOf("MJPG", 16, 8), data);

        QCOMPARE(image.size(), QSize(16, 8));
        QCOMPARE(image.format(), QImage::Format_RGB32);
        QVERIFY2(closeTo(image.pixel(8, 4), color, 12), "cor decodificada fora da tolerância de 12");
    }

    void mjpg_OtherResolution_IsNull() { QVERIFY(decodeFrame(modeOf("MJPG", 16, 8), jpegOf(8, 8, Qt::red)).isNull()); }

    void mjpg_SameWidthDifferentHeight_IsRejected()
    {
        QVERIFY(decodeFrame(modeOf("MJPG", 16, 8), jpegOf(16, 16, Qt::red)).isNull());
    }

    void mjpg_SameHeightDifferentWidth_IsRejected()
    {
        QVERIFY(decodeFrame(modeOf("MJPG", 16, 8), jpegOf(32, 8, Qt::red)).isNull());
    }

    void mjpg_TruncatedJpeg_IsNull()
    {
        auto data = jpegOf(64, 64, QColor(10, 200, 30));
        data.resize(data.size() / 3);
        QVERIFY(decodeFrame(modeOf("MJPG", 64, 64), data).isNull());
    }

    void mjpg_RandomBytes_IsNull()
    {
        std::vector<std::byte> data;
        std::uint32_t state = 12345;
        for (int i = 0; i < 512; ++i) {
            state = (state * 1664525U) + 1013904223U;
            data.push_back(static_cast<std::byte>(static_cast<unsigned char>(state >> 24U)));
        }
        QVERIFY(decodeFrame(modeOf("MJPG", 16, 8), data).isNull());
    }

    void emptyData_IsNull()
    {
        QVERIFY(decodeFrame(modeOf("MJPG", 16, 8), {}).isNull());
        QVERIFY(decodeFrame(modeOf("YUYV", 2, 2), {}).isNull());
    }

    void unknownFormat_IsNull()
    {
        const std::vector<std::byte> data(8, std::byte{0});
        QVERIFY(decodeFrame(modeOf("H264", 2, 2), data).isNull());
    }

    void modeAbove4096_IsNull_BeforeAllocating()
    {
        // Os dados nem precisam ter o tamanho certo: o limite é checado antes de qualquer alocação.
        const std::vector<std::byte> data(16, std::byte{16});
        QVERIFY(decodeFrame(modeOf("YUYV", 4098, 2), data).isNull());
        QVERIFY(decodeFrame(modeOf("YUYV", 2, 4097), data).isNull());
        QVERIFY(decodeFrame(modeOf("MJPG", 4097, 4097), data).isNull());
    }
    void yuyv_WiderThan4096_IsRejected_EvenWithCoherentData()
    {
        const std::vector<std::byte> data(std::size_t{4098} * 2 * 2, std::byte{16});
        QVERIFY(decodeFrame(modeOf("YUYV", 4098, 2), data).isNull());
    }

    void yuyv_TallerThan4096_IsRejected_EvenWithCoherentData()
    {
        const std::vector<std::byte> data(std::size_t{2} * 4097 * 2, std::byte{16});
        QVERIFY(decodeFrame(modeOf("YUYV", 2, 4097), data).isNull());
    }

    void yuyv_Exactly4096Wide_IsAccepted()
    {
        const std::vector<std::byte> data(std::size_t{4096} * 2 * 2, std::byte{16});
        const auto image = decodeFrame(modeOf("YUYV", 4096, 2), data);
        QVERIFY(!image.isNull());
        QCOMPARE(image.size(), QSize(4096, 2));
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

} // namespace

QTEST_GUILESS_MAIN(FrameDecoderTest)
#include "FrameDecoderTest.moc"
