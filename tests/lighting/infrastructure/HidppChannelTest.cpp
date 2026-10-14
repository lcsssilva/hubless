#include "HidppFixture.hpp"

using namespace ghub::lighting;          // NOLINT(google-build-using-namespace)
using namespace ghub::lighting::testing; // NOLINT(google-build-using-namespace)
using infrastructure::HidppChannel;
using infrastructure::HidppError;
using infrastructure::HidppErrorKind;

class HidppChannelTest : public HidppFixture {};

TEST_F(HidppChannelTest, DeniedPairsFailWithoutAnyWrite)
{
    struct Pair {
        std::uint16_t feature;
        std::uint8_t function;
    };
    for (const Pair pair : {Pair{0x4522, 0}, Pair{0x4522, 1}, Pair{0x4522, 2}, Pair{0x4522, 3}, Pair{0x8071, 3},
                            Pair{0x8071, 7}, Pair{0x8070, 8}, Pair{0x1234, 0}}) {
        const auto result = channel->call(device, pair.feature, pair.function);
        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error().kind, HidppErrorKind::PolicyDenied);
    }
    EXPECT_TRUE(io->written().empty()) << "nem a consulta GetFeature pode ser enviada";
}

TEST_F(HidppChannelTest, ResolvesFeatureIndexOnceAndCachesIt)
{
    ASSERT_TRUE(channel->call(device, 0x8071, 0).has_value());
    ASSERT_EQ(io->written().size(), 2U);
    EXPECT_EQ(io->written()[0], longReport(device, 0x00, 0x0B, {0x80, 0x71}));

    ASSERT_TRUE(channel->call(device, 0x8071, 1).has_value());
    ASSERT_EQ(io->written().size(), 3U) << "o índice deve vir do cache; sem nova consulta Root";
    EXPECT_EQ(io->written()[1][2], indexOf(0x8071));
    EXPECT_EQ(io->written()[2][2], indexOf(0x8071));
}

TEST_F(HidppChannelTest, AbsentFeatureFailsAfterOneRootQueryAndIsCached)
{
    features = {0x1004, 0x0001};

    const auto first = channel->call(device, 0x8081, 6);
    ASSERT_FALSE(first.has_value());
    EXPECT_EQ(first.error().kind, HidppErrorKind::FeatureNotPresent);
    EXPECT_EQ(io->written().size(), 1U);

    const auto second = channel->call(device, 0x8081, 6);
    ASSERT_FALSE(second.has_value());
    EXPECT_EQ(second.error().kind, HidppErrorKind::FeatureNotPresent);
    EXPECT_EQ(io->written().size(), 1U);
}

TEST_F(HidppChannelTest, BuildsExactLongReport)
{
    const std::vector<std::uint8_t> params{0x01, 0x03, 0x05};

    const auto result = channel->call(device, 0x8071, 5, params);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(io->written().size(), 2U);
    Frame expected(20, 0);
    expected[0] = 0x11;
    expected[1] = device;
    expected[2] = indexOf(0x8071);
    expected[3] = 0x5B;
    expected[4] = 0x01;
    expected[5] = 0x03;
    expected[6] = 0x05;
    EXPECT_EQ(io->written()[1], expected);
}

TEST_F(HidppChannelTest, ReturnsTheSixteenResponseBytes)
{
    const std::vector<std::uint8_t> params{9, 8, 7};

    const auto result = channel->call(device, 0x8070, 1, params);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)[0], 9);
    EXPECT_EQ((*result)[1], 8);
    EXPECT_EQ((*result)[2], 7);
    EXPECT_EQ(result->size(), 16U);
}

TEST_F(HidppChannelTest, RejectsMoreThanSixteenParameterBytesWithoutWriting)
{
    const std::vector<std::uint8_t> params(17, 0);

    const auto result = channel->call(device, 0x8070, 1, params);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, HidppErrorKind::InvalidArgument);
    EXPECT_TRUE(io->written().empty());
}

TEST_F(HidppChannelTest, IgnoresFramesThatDoNotMatchBeforeTheRightResponse)
{
    // Quadros anteriores: outro software ID (kernel 0x01, OpenRGB 0x0E), notificação, outro dispositivo,
    // outro índice, relatório curto e erro endereçado a outro software ID.
    const auto idx = indexOf(0x8070);
    io->setResponder([&](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == 0) {
            return {longReport(device, 0, 0x0B, {idx})};
        }
        return {longReport(device, idx, 0x11, {0xAA}),
                longReport(device, idx, 0x1E, {0xAA}),
                longReport(device, idx, 0x00, {0xAA}),
                longReport(0x02, idx, 0x1B, {0xAA}),
                longReport(device, static_cast<std::uint8_t>(idx + 1), 0x1B, {0xAA}),
                Frame{0x10, device, idx, 0x1B, 0, 0, 0},
                longReport(device, 0xFF, 0x1E, {}),
                longReport(device, idx, 0x1B, {0x42})};
    });

    const auto result = channel->call(device, 0x8070, 1);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)[0], 0x42);
}

TEST_F(HidppChannelTest, NotificationDuringCallReachesTheHandlerAndTheResponseStillMatches)
{
    const auto idx = indexOf(0x8070);
    const Frame receiverNotice{0x10, device, 0x41, 0x11, 0x71, 0x7c, 0x40};
    const auto otherDevice = longReport(0x02, idx, 0x1B, {0xAA});
    io->setResponder([&](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == 0) {
            return {longReport(device, 0, 0x0B, {idx})};
        }
        return {receiverNotice, otherDevice, longReport(device, idx, 0x1B, {0x42})};
    });
    std::vector<Frame> seen;
    channel->setNotificationHandler(
        [&](std::span<const std::uint8_t> report) { seen.emplace_back(report.begin(), report.end()); });

    const auto result = channel->call(device, 0x8070, 1);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)[0], 0x42);
    EXPECT_EQ(seen, (std::vector<Frame>{receiverNotice, otherDevice}));
}

TEST_F(HidppChannelTest, PollNotificationsDeliversPendingReportsAndWritesNothing)
{
    const Frame first{0x10, device, 0x41, 0x11, 0x71, 0x7c, 0x40};
    const auto second = longReport(device, 4, 0x00, {0x01, 0x01, 0x01});
    io->queueRead(first);
    io->queueRead(second);
    std::vector<Frame> seen;
    channel->setNotificationHandler(
        [&](std::span<const std::uint8_t> report) { seen.emplace_back(report.begin(), report.end()); });

    const auto polled = channel->pollNotifications();

    ASSERT_TRUE(polled.has_value());
    EXPECT_EQ(seen, (std::vector<Frame>{first, second}));
    EXPECT_TRUE(io->written().empty());
    ASSERT_FALSE(io->readTimeouts().empty());
    for (const auto timeout : io->readTimeouts()) {
        EXPECT_EQ(timeout, std::chrono::milliseconds{0});
    }
}

TEST_F(HidppChannelTest, PollNotificationsWithNothingPendingSucceedsWithoutCallingTheHandler)
{
    int calls = 0;
    channel->setNotificationHandler([&](std::span<const std::uint8_t>) { ++calls; });

    EXPECT_TRUE(channel->pollNotifications().has_value());
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(io->written().empty());
}

TEST_F(HidppChannelTest, PollNotificationsReportsIoFailure)
{
    class FailingIo final : public infrastructure::HidReportIo {
    public:
        std::expected<std::size_t, infrastructure::HidIoError> write(std::span<const std::uint8_t>) override
        {
            return std::unexpected(infrastructure::HidIoError{.failure = infrastructure::HidIoFailure::Io});
        }
        std::expected<std::size_t, infrastructure::HidIoError> read(std::span<std::uint8_t>,
                                                                    std::chrono::milliseconds) override
        {
            return std::unexpected(
                infrastructure::HidIoError{.failure = infrastructure::HidIoFailure::Io, .systemError = 5});
        }
    };
    HidppChannel failing{std::make_unique<FailingIo>(), clock};

    const auto polled = failing.pollNotifications();

    ASSERT_FALSE(polled.has_value());
    EXPECT_EQ(polled.error().kind, HidppErrorKind::Io);
}

TEST_F(HidppChannelTest, FeatureIndexResolvesThroughRootAndReportsAbsence)
{
    const auto present = channel->featureIndex(device, 0x8071);
    ASSERT_TRUE(present.has_value());
    EXPECT_EQ(*present, indexOf(0x8071));

    const auto absent = channel->featureIndex(device, 0x1D4B);
    ASSERT_FALSE(absent.has_value());
    EXPECT_EQ(absent.error().kind, HidppErrorKind::FeatureNotPresent);
}

TEST_F(HidppChannelTest, TypesHidpp20DeviceError)
{
    const auto idx = indexOf(0x8070);
    io->setResponder([&](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == 0) {
            return {longReport(device, 0, 0x0B, {idx})};
        }
        return {longReport(device, 0xFF, idx, {0x3B, 0x05})};
    });

    const auto result = channel->call(device, 0x8070, 3);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, HidppErrorKind::DeviceErrorHidpp20);
    EXPECT_EQ(result.error().code, 0x05);
}

TEST_F(HidppChannelTest, TypesHidpp10DeviceError)
{
    const auto idx = indexOf(0x8070);
    io->setResponder([&](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == 0) {
            return {longReport(device, 0, 0x0B, {idx})};
        }
        return {Frame{0x10, device, 0x8F, idx, 0x3B, 0x02, 0x00}};
    });

    const auto result = channel->call(device, 0x8070, 3);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, HidppErrorKind::DeviceErrorHidpp10);
    EXPECT_EQ(result.error().code, 0x02);
}

TEST_F(HidppChannelTest, ErrorsForOtherSoftwareIdsAreIgnored)
{
    const auto idx = indexOf(0x8070);
    io->setResponder([&](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == 0) {
            return {longReport(device, 0, 0x0B, {idx})};
        }
        return {longReport(device, 0xFF, idx, {0x3E, 0x05}), longReport(device, idx, 0x3B, {0x42})};
    });

    const auto result = channel->call(device, 0x8070, 3);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)[0], 0x42);
}

TEST_F(HidppChannelTest, ReportsTimeoutWhenTotalDeadlineIsExhaustedByUnmatchedFrames)
{
    io->setReadCost(std::chrono::milliseconds{100});
    io->setResponder([&](const Frame& frame) -> std::vector<Frame> {
        // Responde sempre com um quadro de outro software ID: nunca casa.
        return {longReport(frame[1], frame[2], 0x01), longReport(frame[1], frame[2], 0x01),
                longReport(frame[1], frame[2], 0x01), longReport(frame[1], frame[2], 0x01)};
    });

    const auto result = channel->call(device, 0x8070, 1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, HidppErrorKind::Timeout);
    EXPECT_EQ(io->written().size(), 1U) << "sem reenvio (R11)";
    for (const auto timeout : io->readTimeouts()) {
        EXPECT_LE(timeout, HidppChannel::defaultTimeout);
        EXPECT_GT(timeout.count(), 0);
    }
}

TEST_F(HidppChannelTest, ReportsTimeoutWhenTheDeviceStaysSilent)
{
    io->setResponder(nullptr);

    const auto result = channel->call(device, 0x8070, 1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, HidppErrorKind::Timeout);
    ASSERT_EQ(io->readTimeouts().size(), 1U);
    EXPECT_EQ(io->readTimeouts().front(), HidppChannel::defaultTimeout);
    EXPECT_EQ(io->written().size(), 1U);
}

TEST_F(HidppChannelTest, WriteFailureIsReportedAndNotRetried)
{
    io->failWrites({.failure = infrastructure::HidIoFailure::Io, .systemError = 5});

    const auto result = channel->call(device, 0x8070, 1);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, HidppErrorKind::Io);
    EXPECT_EQ(io->written().size(), 1U);
    EXPECT_TRUE(io->readTimeouts().empty());
}

// --- Rastreio (INC-008 R12): um traço por chamada, sem bytes de parâmetros ---

class HidppChannelTraceTest : public HidppFixture {
protected:
    // Recria o canal com o receptor de rastreio; `io` passa a apontar para o novo fake.
    void installSink()
    {
        auto fake = std::make_unique<ghub::fakes::FakeHidReportIo>(&clock);
        io = fake.get();
        io->setResponder([this](const Frame& frame) { return respond(frame); });
        channel.emplace(std::move(fake), clock, HidppChannel::defaultTimeout,
                        [this](const infrastructure::HidppCommandTrace& trace) { traces.push_back(trace); });
    }

    std::vector<infrastructure::HidppCommandTrace> traces;
};

TEST_F(HidppChannelTraceTest, SuccessEmitsExactlyOneTraceEvenWithRootQuery)
{
    installSink();
    const std::vector<std::uint8_t> params{0x01, 0x02, 0x03};

    ASSERT_TRUE(channel->call(device, 0x8071, 5, params).has_value());

    ASSERT_EQ(io->written().size(), 2U) << "consulta Root + comando";
    ASSERT_EQ(traces.size(), 1U);
    EXPECT_EQ(traces[0].deviceIndex, device);
    EXPECT_EQ(traces[0].featureId, 0x8071);
    EXPECT_EQ(traces[0].function, 5);
    EXPECT_EQ(traces[0].paramCount, 3U);
    EXPECT_FALSE(traces[0].failure.has_value());
    EXPECT_EQ(traces[0].errorCode, 0);
}

TEST_F(HidppChannelTraceTest, PolicyDeniedEmitsTraceAndWritesNothing)
{
    installSink();

    ASSERT_FALSE(channel->call(device, 0x4522, 0).has_value());

    EXPECT_TRUE(io->written().empty());
    ASSERT_EQ(traces.size(), 1U);
    EXPECT_EQ(traces[0].featureId, 0x4522);
    EXPECT_EQ(traces[0].failure, HidppErrorKind::PolicyDenied);
}

TEST_F(HidppChannelTraceTest, InvalidArgumentAndAbsentFeatureEmitOneTraceEach)
{
    installSink();
    features = {0x1004, 0x0001};

    ASSERT_FALSE(channel->call(device, 0x8070, 1, std::vector<std::uint8_t>(17, 0)).has_value());
    ASSERT_FALSE(channel->call(device, 0x8081, 6).has_value());

    ASSERT_EQ(traces.size(), 2U);
    EXPECT_EQ(traces[0].failure, HidppErrorKind::InvalidArgument);
    EXPECT_EQ(traces[0].paramCount, 17U);
    EXPECT_EQ(traces[1].failure, HidppErrorKind::FeatureNotPresent);
}

TEST_F(HidppChannelTraceTest, Hidpp20DeviceErrorCarriesKindAndCode)
{
    installSink();
    const auto idx = indexOf(0x8070);
    io->setResponder([&](const Frame& frame) -> std::vector<Frame> {
        if (frame[2] == 0) {
            return {longReport(device, 0, 0x0B, {idx})};
        }
        return {longReport(device, 0xFF, idx, {0x3B, 0x05})};
    });

    ASSERT_FALSE(channel->call(device, 0x8070, 3).has_value());

    ASSERT_EQ(traces.size(), 1U);
    EXPECT_EQ(traces[0].failure, HidppErrorKind::DeviceErrorHidpp20);
    EXPECT_EQ(traces[0].errorCode, 0x05);
}

TEST_F(HidppChannelTraceTest, TimeoutEmitsTrace)
{
    installSink();
    io->setResponder(nullptr);

    ASSERT_FALSE(channel->call(device, 0x8070, 1).has_value());

    ASSERT_EQ(traces.size(), 1U);
    EXPECT_EQ(traces[0].failure, HidppErrorKind::Timeout);
}

TEST_F(HidppChannelTraceTest, EmptySinkDoesNotBreakTheCall)
{
    ASSERT_TRUE(channel->call(device, 0x8070, 1).has_value());
    ASSERT_FALSE(channel->call(device, 0x4522, 0).has_value());
}
