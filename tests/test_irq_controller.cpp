#include <gtest/gtest.h>
#include "IRQController.h"

namespace {
    // Unmask everything so tests can focus on one behaviour at a time.
    void unmaskAll(IRQController &c)
    {
        c.write32(IRQController::P_I_MASKL, 0);
        c.write32(IRQController::P_I_MASKH, 0);
    }
}

TEST(IRQController, ResetIsAllMaskedAndIdle)
{
    IRQController c;
    EXPECT_EQ(c.read32(IRQController::P_I_MASKL), 0xFFFFFFFFu);
    EXPECT_EQ(c.read32(IRQController::P_I_MASKH), 0xFFu);
    c.setSource(10, true);
    EXPECT_FALSE(c.irqActive());
}

TEST(IRQController, PendingReflectsRawLevelEvenWhenMasked)
{
    IRQController c;
    c.setSource(10, true);
    c.setSource(36, true);
    EXPECT_EQ(c.read32(IRQController::P_INTPND), 1u << 10);
    EXPECT_EQ(c.read32(IRQController::P_INTPND_H), 1u << 4);   // source 36
}

TEST(IRQController, UnmaskedSourceActivatesWithVector63MinusSource)
{
    IRQController c;
    unmaskAll(c);
    c.setSource(10, true);
    EXPECT_TRUE(c.irqActive());
    EXPECT_EQ(c.activeVector(), 53u);
    c.setSource(10, false);
    EXPECT_FALSE(c.irqActive());
}

TEST(IRQController, MaskSuppressesSource)
{
    IRQController c;
    unmaskAll(c);
    c.write32(IRQController::P_I_MASKL, 1u << 10);
    c.setSource(10, true);
    EXPECT_FALSE(c.irqActive());
    c.write32(IRQController::P_I_MASKL, 0);
    EXPECT_TRUE(c.irqActive());
}

TEST(IRQController, HighSourcesUseMaskH)
{
    IRQController c;
    unmaskAll(c);
    c.write32(IRQController::P_I_MASKH, 1u << 4);              // source 36
    c.setSource(36, true);
    EXPECT_FALSE(c.irqActive());
    c.write32(IRQController::P_I_MASKH, 0);
    EXPECT_TRUE(c.irqActive());
    EXPECT_EQ(c.activeVector(), 27u);
}

TEST(IRQController, TieBreakPrefersLowestSourceNumber)
{
    IRQController c;
    unmaskAll(c);
    c.setSource(17, true);
    c.setSource(10, true);
    EXPECT_EQ(c.activeSource(), 10);
}

TEST(IRQController, SourcePriorityOverridesTieBreak)
{
    IRQController c;
    unmaskAll(c);
    // Group 2 (sources 16..23): give source 17 priority 000 and source 16 priority 3.
    c.write32(IRQController::P_I_PSLV2, 3u << 0);              // source 16 -> 3rd... lower priority
    c.setSource(16, true);
    c.setSource(17, true);
    EXPECT_EQ(c.activeSource(), 17);
}

TEST(IRQController, GroupPriorityBeatsSourcePriority)
{
    IRQController c;
    unmaskAll(c);
    c.setSource(10, true);                                      // group 1
    c.setSource(17, true);                                      // group 2
    c.write32(IRQController::P_I_PMST, 0x1u << 2 | 0x0u << 4);  // group1 prio 1, group2 prio 0
    EXPECT_EQ(c.activeSource(), 17);
}

TEST(IRQController, SoftwareInterruptSetAndClear)
{
    IRQController c;
    unmaskAll(c);
    c.write32(IRQController::P_I_GPUSWI, 1u);
    EXPECT_TRUE(c.irqActive());
    EXPECT_EQ(c.activeVector(), 53u);
    EXPECT_EQ(c.read32(IRQController::P_INTPND) & (1u << 10), 1u << 10);
    c.write32(IRQController::P_I_GPUSWI, 1u << 8);
    EXPECT_FALSE(c.irqActive());
}

TEST(IRQController, PendingRegistersAreReadOnly)
{
    IRQController c;
    c.write32(IRQController::P_INTPND, 0xFFFFFFFFu);
    EXPECT_EQ(c.read32(IRQController::P_INTPND), 0u);
}

TEST(IRQController, PriorityRegistersRoundTrip)
{
    IRQController c;
    c.write32(IRQController::P_I_PMST, 0xE4u);
    c.write32(IRQController::P_I_PSLV3, 0x123456u);
    EXPECT_EQ(c.read32(IRQController::P_I_PMST), 0xE4u);
    EXPECT_EQ(c.read32(IRQController::P_I_PSLV3), 0x123456u);
}
