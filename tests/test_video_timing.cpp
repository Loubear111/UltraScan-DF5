// VideoTiming: scanline / frame counter (no Bus, no PPU).
#include <gtest/gtest.h>
#include "VideoTiming.h"

namespace {
constexpr uint32_t kLine = VideoTiming::CPU_CYCLES_PER_LINE;

// Run `lines` whole lines in steps of `step` cycles, counting events.
struct Counts { int start = 0, end = 0, hv = 0; };
Counts runLines(VideoTiming& v, uint32_t lines, uint32_t step, uint32_t hvV = 0x1FF, uint32_t hvH = 0, uint32_t extra = 0)
{
    Counts c;
    for (uint64_t done = 0; done < (uint64_t)lines * kLine; done += step)
    {
        const uint32_t ev = v.advance(step, hvV, hvH, extra);
        c.start += (ev & VideoTiming::EV_VBLANK_START) ? 1 : 0;
        c.end   += (ev & VideoTiming::EV_VBLANK_END) ? 1 : 0;
        c.hv    += (ev & VideoTiming::EV_HV_HIT) ? 1 : 0;
    }
    return c;
}
} // namespace

TEST(VideoTiming, ConstantsMatchTheMameNtscFigures)
{
    EXPECT_EQ(VideoTiming::CPU_CYCLES_PER_LINE, 3432u);
    EXPECT_EQ(VideoTiming::LINES_PER_FRAME, 525u);
    EXPECT_EQ(VideoTiming::ACTIVE_LINES, 480u);
}

TEST(VideoTiming, VblankStartsExactlyAtTheActiveLineCount)
{
    VideoTiming v;
    EXPECT_EQ(v.advance(VideoTiming::ACTIVE_LINES * kLine - 1, 0x1FF, 0, 0) & VideoTiming::EV_VBLANK_START, 0u);
    EXPECT_EQ(v.currentLine(), VideoTiming::ACTIVE_LINES - 1);
    EXPECT_NE(v.advance(1, 0x1FF, 0, 0) & VideoTiming::EV_VBLANK_START, 0u);
    EXPECT_EQ(v.currentLine(), VideoTiming::ACTIVE_LINES);
}

TEST(VideoTiming, OneStartAndOneEndPerFrame_AndTheNextFrameRepeats)
{
    VideoTiming v;
    Counts c = runLines(v, VideoTiming::LINES_PER_FRAME, kLine);
    EXPECT_EQ(c.start, 1);
    EXPECT_EQ(c.end, 1);
    EXPECT_EQ(v.currentLine(), 0u);
    c = runLines(v, VideoTiming::LINES_PER_FRAME, kLine);
    EXPECT_EQ(c.start, 1);
    EXPECT_EQ(c.end, 1);
}

TEST(VideoTiming, VblankExtraLinesDelayTheEnd)
{
    VideoTiming v;
    EXPECT_EQ(runLines(v, VideoTiming::LINES_PER_FRAME, kLine, 0x1FF, 0, 10).end, 0);
    EXPECT_EQ(runLines(v, 10, kLine, 0x1FF, 0, 10).end, 1);
}

TEST(VideoTiming, HvHitFiresOncePerFrameAtTheTargetPixel)
{
    VideoTiming v;
    const uint32_t V = 5, H = 100;
    // Just before line 5 + 100 pixels: nothing.
    EXPECT_EQ(v.advance(5 * kLine + 100 * VideoTiming::CPU_CYCLES_PER_PIXEL - 1, V, H, 0) & VideoTiming::EV_HV_HIT, 0u);
    EXPECT_NE(v.advance(1, V, H, 0) & VideoTiming::EV_HV_HIT, 0u);
    // Not again in the same frame.
    Counts rest = runLines(v, VideoTiming::LINES_PER_FRAME - 5, 6, V, H);
    EXPECT_EQ(rest.hv, 0);
    // And once more in the next frame.
    Counts next = runLines(v, VideoTiming::LINES_PER_FRAME, 6, V, H);
    EXPECT_EQ(next.hv, 1);
}

TEST(VideoTiming, SmallAndLargeStepsGiveTheSameEventCounts)
{
    VideoTiming small, large;
    const Counts a = runLines(small, 3 * VideoTiming::LINES_PER_FRAME, 6, 20, 50);
    const Counts b = runLines(large, 3 * VideoTiming::LINES_PER_FRAME, kLine / 2, 20, 50);
    EXPECT_EQ(a.start, 3);
    EXPECT_EQ(a.end, 3);
    EXPECT_EQ(a.hv, 3);
    EXPECT_EQ(b.start, a.start);
    EXPECT_EQ(b.end, a.end);
    EXPECT_EQ(b.hv, a.hv);
}
