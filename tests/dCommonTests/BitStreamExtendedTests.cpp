// Extended BitStream tests covering primitive round-trips, bit accounting,
// reset/seek semantics, compressed writes, string handling, NiPoint3, and
// graceful over-read behavior.
//
// CBITSTREAM expands to: RakNet::BitStream bitStream;
// (defined in dCommonVars.h)

#include <gtest/gtest.h>

#include "dCommonVars.h"   // for CBITSTREAM
#include "NiPoint3.h"

#include <cstdint>
#include <cstring>
#include <limits>

// ===========================================================================
// Single primitive round-trips
// ===========================================================================

TEST(BitStreamExtended, WriteBool_True_ReadBackTrue) {
    CBITSTREAM;
    bitStream.Write<bool>(true);
    bool val = false;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_TRUE(val);
}

TEST(BitStreamExtended, WriteBool_False_ReadBackFalse) {
    CBITSTREAM;
    bitStream.Write<bool>(false);
    bool val = true;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_FALSE(val);
}

TEST(BitStreamExtended, WriteUint8_ReadBack) {
    CBITSTREAM;
    const uint8_t expected = 0xABu;
    bitStream.Write(expected);
    uint8_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteUint8_Zero_ReadBack) {
    CBITSTREAM;
    const uint8_t expected = 0u;
    bitStream.Write(expected);
    uint8_t val = 0xFF;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, 0u);
}

TEST(BitStreamExtended, WriteUint8_MaxValue_ReadBack) {
    CBITSTREAM;
    const uint8_t expected = std::numeric_limits<uint8_t>::max();
    bitStream.Write(expected);
    uint8_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteUint16_ReadBack) {
    CBITSTREAM;
    const uint16_t expected = 0x1234u;
    bitStream.Write(expected);
    uint16_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteUint16_MaxValue_ReadBack) {
    CBITSTREAM;
    const uint16_t expected = std::numeric_limits<uint16_t>::max();
    bitStream.Write(expected);
    uint16_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteUint32_ReadBack) {
    CBITSTREAM;
    const uint32_t expected = 0xDEADBEEFu;
    bitStream.Write(expected);
    uint32_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteUint32_Zero_ReadBack) {
    CBITSTREAM;
    const uint32_t expected = 0u;
    bitStream.Write(expected);
    uint32_t val = 0xFFFFFFFFu;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, 0u);
}

TEST(BitStreamExtended, WriteUint32_MaxValue_ReadBack) {
    CBITSTREAM;
    const uint32_t expected = std::numeric_limits<uint32_t>::max();
    bitStream.Write(expected);
    uint32_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteUint64_ReadBack) {
    CBITSTREAM;
    const uint64_t expected = 0x0123456789ABCDEFull;
    bitStream.Write(expected);
    uint64_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteUint64_MaxValue_ReadBack) {
    CBITSTREAM;
    const uint64_t expected = std::numeric_limits<uint64_t>::max();
    bitStream.Write(expected);
    uint64_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteInt32_Negative_ReadBack) {
    CBITSTREAM;
    const int32_t expected = -42;
    bitStream.Write(expected);
    int32_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteInt32_MinValue_ReadBack) {
    CBITSTREAM;
    const int32_t expected = std::numeric_limits<int32_t>::min();
    bitStream.Write(expected);
    int32_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteFloat_ReadBack) {
    CBITSTREAM;
    const float expected = 3.14159f;
    bitStream.Write(expected);
    float val = 0.0f;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_FLOAT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteFloat_NegativeValue_ReadBack) {
    CBITSTREAM;
    const float expected = -1234.5678f;
    bitStream.Write(expected);
    float val = 0.0f;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_FLOAT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteDouble_ReadBack) {
    CBITSTREAM;
    const double expected = 2.718281828459045;
    bitStream.Write(expected);
    double val = 0.0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_DOUBLE_EQ(val, expected);
}

// ===========================================================================
// Sequence of multiple values — ordering preserved
// ===========================================================================

TEST(BitStreamExtended, WriteMultiplePrimitives_ReadBackInOrder) {
    CBITSTREAM;
    const bool    b   = true;
    const uint8_t u8  = 7u;
    const uint16_t u16 = 300u;
    const uint32_t u32 = 0xCAFEu;
    const float   f   = 1.5f;

    bitStream.Write(b);
    bitStream.Write(u8);
    bitStream.Write(u16);
    bitStream.Write(u32);
    bitStream.Write(f);

    bool    rb   = false;
    uint8_t ru8  = 0;
    uint16_t ru16 = 0;
    uint32_t ru32 = 0;
    float   rf   = 0.0f;

    ASSERT_TRUE(bitStream.Read(rb));
    ASSERT_TRUE(bitStream.Read(ru8));
    ASSERT_TRUE(bitStream.Read(ru16));
    ASSERT_TRUE(bitStream.Read(ru32));
    ASSERT_TRUE(bitStream.Read(rf));

    EXPECT_EQ(rb,   b);
    EXPECT_EQ(ru8,  u8);
    EXPECT_EQ(ru16, u16);
    EXPECT_EQ(ru32, u32);
    EXPECT_FLOAT_EQ(rf, f);
}

TEST(BitStreamExtended, WriteManyBools_ReadBackInOrder) {
    CBITSTREAM;
    // Deliberate pattern to exercise bit-packing.
    const bool pattern[] = {true, false, true, true, false, false, true, false};
    for (bool b : pattern) bitStream.Write(b);

    for (bool expected : pattern) {
        bool val = !expected; // start opposite to ensure we actually read
        ASSERT_TRUE(bitStream.Read(val));
        EXPECT_EQ(val, expected);
    }
}

TEST(BitStreamExtended, WriteSequenceOfUint32s_ReadBackInOrder) {
    CBITSTREAM;
    constexpr int N = 8;
    uint32_t written[N];
    for (int i = 0; i < N; ++i) {
        written[i] = static_cast<uint32_t>(i * 111111u + 7u);
        bitStream.Write(written[i]);
    }
    for (int i = 0; i < N; ++i) {
        uint32_t val = 0;
        ASSERT_TRUE(bitStream.Read(val));
        EXPECT_EQ(val, written[i]) << "Mismatch at index " << i;
    }
}

// ===========================================================================
// Bit accounting: GetNumberOfBitsUsed / GetNumberOfUnreadBits
// ===========================================================================

TEST(BitStreamExtended, GetNumberOfBitsUsed_StartsAtZero) {
    CBITSTREAM;
    EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0u);
}

TEST(BitStreamExtended, GetNumberOfBitsUsed_IncreasesByTypeSizeAfterWrite) {
    CBITSTREAM;
    bitStream.Write<uint8_t>(0);
    EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 8u);

    bitStream.Write<uint16_t>(0);
    EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 24u);

    bitStream.Write<uint32_t>(0);
    EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 56u);
}

TEST(BitStreamExtended, GetNumberOfBitsUsed_AfterWritingBool_IsOne) {
    CBITSTREAM;
    bitStream.Write<bool>(true);
    EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 1u);
}

TEST(BitStreamExtended, GetNumberOfUnreadBits_EqualsBitsUsed_BeforeAnyRead) {
    CBITSTREAM;
    bitStream.Write<uint32_t>(12345u);
    bitStream.Write<uint32_t>(67890u);
    EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), bitStream.GetNumberOfBitsUsed());
}

TEST(BitStreamExtended, GetNumberOfUnreadBits_DecreasesAfterRead) {
    CBITSTREAM;
    bitStream.Write<uint32_t>(0xAABBCCDDu);

    const auto totalBits = bitStream.GetNumberOfBitsUsed();
    EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), totalBits);

    uint32_t val = 0;
    bitStream.Read(val);
    EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0u);
}

TEST(BitStreamExtended, GetNumberOfUnreadBits_AfterPartialRead) {
    CBITSTREAM;
    bitStream.Write<uint32_t>(1u);
    bitStream.Write<uint32_t>(2u);
    bitStream.Write<uint32_t>(3u);

    uint32_t val = 0;
    bitStream.Read(val); // consumes 32 bits
    EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 64u);
}

// ===========================================================================
// Reset
// ===========================================================================

TEST(BitStreamExtended, Reset_ClearsBitsUsed) {
    CBITSTREAM;
    bitStream.Write<uint32_t>(42u);
    ASSERT_GT(bitStream.GetNumberOfBitsUsed(), 0u);

    bitStream.Reset();
    EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0u);
}

TEST(BitStreamExtended, Reset_CanWriteAgainAfterReset) {
    CBITSTREAM;
    bitStream.Write<uint32_t>(0xFFFFu);
    bitStream.Reset();

    const uint32_t expected = 99u;
    bitStream.Write(expected);
    uint32_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, Reset_NumberOfUnreadBitsIsZeroAfterReset) {
    CBITSTREAM;
    bitStream.Write<uint64_t>(0x1122334455667788ull);
    bitStream.Reset();
    EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0u);
}

// ===========================================================================
// SetReadOffset — seeking back to re-read
// ===========================================================================

TEST(BitStreamExtended, SetReadOffset_ZeroAllowsReRead) {
    CBITSTREAM;
    const uint32_t expected = 0xFACEFEEDu;
    bitStream.Write(expected);

    uint32_t first = 0, second = 0;
    ASSERT_TRUE(bitStream.Read(first));
    EXPECT_EQ(first, expected);

    // Seek back to the beginning and read again.
    bitStream.SetReadOffset(0);
    ASSERT_TRUE(bitStream.Read(second));
    EXPECT_EQ(second, expected);
}

TEST(BitStreamExtended, SetReadOffset_ToMiddleOfStream) {
    CBITSTREAM;
    bitStream.Write<uint32_t>(111u);
    bitStream.Write<uint32_t>(222u);
    bitStream.Write<uint32_t>(333u);

    // Skip the first uint32 by seeking to bit 32.
    bitStream.SetReadOffset(32);
    uint32_t val = 0;
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, 222u);
}

TEST(BitStreamExtended, SetReadOffset_RepeatedReadsFromOffset) {
    CBITSTREAM;
    bitStream.Write<uint16_t>(0xABCDu);
    bitStream.Write<uint16_t>(0x1234u);

    // Read twice from offset 0.
    for (int i = 0; i < 2; ++i) {
        bitStream.SetReadOffset(0);
        uint16_t val = 0;
        ASSERT_TRUE(bitStream.Read(val));
        EXPECT_EQ(val, 0xABCDu);
    }
}

// ===========================================================================
// ResetReadPointer
// ===========================================================================

TEST(BitStreamExtended, ResetReadPointer_AllowsReReadFromStart) {
    CBITSTREAM;
    bitStream.Write<uint32_t>(9876u);

    uint32_t v1 = 0;
    ASSERT_TRUE(bitStream.Read(v1));
    EXPECT_EQ(v1, 9876u);

    bitStream.ResetReadPointer();

    uint32_t v2 = 0;
    ASSERT_TRUE(bitStream.Read(v2));
    EXPECT_EQ(v2, 9876u);
}

// ===========================================================================
// Compressed writes
// ===========================================================================

TEST(BitStreamExtended, WriteCompressedUint32_ReadCompressedUint32) {
    CBITSTREAM;
    const uint32_t expected = 42u;
    bitStream.WriteCompressed(expected);
    uint32_t val = 0;
    ASSERT_TRUE(bitStream.ReadCompressed(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteCompressedUint32_Zero_RoundTrips) {
    CBITSTREAM;
    const uint32_t expected = 0u;
    bitStream.WriteCompressed(expected);
    uint32_t val = 0xFFFFFFFFu;
    ASSERT_TRUE(bitStream.ReadCompressed(val));
    EXPECT_EQ(val, 0u);
}

TEST(BitStreamExtended, WriteCompressedUint32_MaxValue_RoundTrips) {
    CBITSTREAM;
    const uint32_t expected = std::numeric_limits<uint32_t>::max();
    bitStream.WriteCompressed(expected);
    uint32_t val = 0;
    ASSERT_TRUE(bitStream.ReadCompressed(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteCompressedUint32_UsesFewerBitsForSmallValues) {
    // Compressed encoding of small values should use fewer bits than 32.
    CBITSTREAM;
    bitStream.WriteCompressed<uint32_t>(1u);
    EXPECT_LT(bitStream.GetNumberOfBitsUsed(), 32u);
}

TEST(BitStreamExtended, WriteCompressedUint64_RoundTrips) {
    CBITSTREAM;
    const uint64_t expected = 12345678ull;
    bitStream.WriteCompressed(expected);
    uint64_t val = 0;
    ASSERT_TRUE(bitStream.ReadCompressed(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteCompressedUint16_RoundTrips) {
    CBITSTREAM;
    const uint16_t expected = 500u;
    bitStream.WriteCompressed(expected);
    uint16_t val = 0;
    ASSERT_TRUE(bitStream.ReadCompressed(val));
    EXPECT_EQ(val, expected);
}

// ===========================================================================
// char16_t / u16string round-trips (as used in the codebase)
// ===========================================================================

TEST(BitStreamExtended, WriteChar16_ReadChar16_SingleChar) {
    CBITSTREAM;
    const char16_t expected = u'A';
    bitStream.Write(expected);
    char16_t val = u'\0';
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_EQ(val, expected);
}

TEST(BitStreamExtended, WriteU16String_ReadBack_CharByChar) {
    CBITSTREAM;
    const std::u16string expected = u"Hello";
    for (char16_t c : expected) bitStream.Write(c);

    std::u16string result;
    char16_t c = 0;
    while (bitStream.Read(c)) result += c;

    EXPECT_EQ(result, expected);
}

TEST(BitStreamExtended, WriteU16String_NullTerminated_ReadBack) {
    CBITSTREAM;
    const std::u16string str = u"Test\u00FCString"; // contains non-ASCII
    for (char16_t c : str) bitStream.Write(c);
    // Write a null terminator.
    bitStream.Write<char16_t>(0);

    std::u16string result;
    char16_t c = 0;
    while (bitStream.Read(c) && c != 0) result += c;

    EXPECT_EQ(result, str);
}

TEST(BitStreamExtended, LUWString_WriteAndRead) {
    CBITSTREAM;
    const std::u16string str = u"GameTest";
    bitStream.Write(LUWString(str, 33));

    LUWString readBack(33);
    ASSERT_TRUE(bitStream.Read(readBack));
    EXPECT_EQ(readBack.string, str);
}

TEST(BitStreamExtended, LUWString_EmptyString_WriteAndRead) {
    CBITSTREAM;
    const std::u16string str = u"";
    bitStream.Write(LUWString(str, 10));

    LUWString readBack(10);
    ASSERT_TRUE(bitStream.Read(readBack));
    EXPECT_EQ(readBack.string, str);
}

// ===========================================================================
// NiPoint3 round-trip
// ===========================================================================

TEST(BitStreamExtended, WriteNiPoint3_ReadBack_MatchesOriginal) {
    CBITSTREAM;
    const NiPoint3 expected(1.5f, -2.75f, 100.0f);
    bitStream.Write(expected.x);
    bitStream.Write(expected.y);
    bitStream.Write(expected.z);

    NiPoint3 result;
    ASSERT_TRUE(bitStream.Read(result.x));
    ASSERT_TRUE(bitStream.Read(result.y));
    ASSERT_TRUE(bitStream.Read(result.z));

    EXPECT_FLOAT_EQ(result.x, expected.x);
    EXPECT_FLOAT_EQ(result.y, expected.y);
    EXPECT_FLOAT_EQ(result.z, expected.z);
}

TEST(BitStreamExtended, WriteNiPoint3_Zero_ReadBack) {
    CBITSTREAM;
    const NiPoint3 expected = NiPoint3Constant::ZERO;
    bitStream.Write(expected.x);
    bitStream.Write(expected.y);
    bitStream.Write(expected.z);

    NiPoint3 result(1.0f, 1.0f, 1.0f);
    ASSERT_TRUE(bitStream.Read(result.x));
    ASSERT_TRUE(bitStream.Read(result.y));
    ASSERT_TRUE(bitStream.Read(result.z));

    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
    EXPECT_FLOAT_EQ(result.z, 0.0f);
}

TEST(BitStreamExtended, WriteNiPoint3_UnitX_ReadBack) {
    CBITSTREAM;
    const NiPoint3 expected = NiPoint3Constant::UNIT_X;
    bitStream.Write(expected.x);
    bitStream.Write(expected.y);
    bitStream.Write(expected.z);

    NiPoint3 result;
    ASSERT_TRUE(bitStream.Read(result.x));
    ASSERT_TRUE(bitStream.Read(result.y));
    ASSERT_TRUE(bitStream.Read(result.z));

    EXPECT_EQ(result, expected);
}

TEST(BitStreamExtended, WriteMultipleNiPoint3s_ReadBackInOrder) {
    CBITSTREAM;
    const NiPoint3 a(1.0f, 2.0f, 3.0f);
    const NiPoint3 b(4.0f, 5.0f, 6.0f);

    bitStream.Write(a.x); bitStream.Write(a.y); bitStream.Write(a.z);
    bitStream.Write(b.x); bitStream.Write(b.y); bitStream.Write(b.z);

    NiPoint3 ra, rb;
    ASSERT_TRUE(bitStream.Read(ra.x)); ASSERT_TRUE(bitStream.Read(ra.y)); ASSERT_TRUE(bitStream.Read(ra.z));
    ASSERT_TRUE(bitStream.Read(rb.x)); ASSERT_TRUE(bitStream.Read(rb.y)); ASSERT_TRUE(bitStream.Read(rb.z));

    EXPECT_EQ(ra, a);
    EXPECT_EQ(rb, b);
}

// ===========================================================================
// Over-reading past end of stream
// ===========================================================================

TEST(BitStreamExtended, OverRead_BoolAfterExhaustedStream_ReturnsFalse) {
    CBITSTREAM;
    bitStream.Write<bool>(true);
    bool val = false;
    // Consume the one bit.
    ASSERT_TRUE(bitStream.Read(val));
    EXPECT_TRUE(val);

    // Further read should fail (return false).
    bool overVal = true;
    EXPECT_FALSE(bitStream.Read(overVal));
}

TEST(BitStreamExtended, OverRead_Uint32AfterExhaustedStream_ReturnsFalse) {
    CBITSTREAM;
    bitStream.Write<uint32_t>(1u);
    uint32_t v = 0;
    ASSERT_TRUE(bitStream.Read(v));

    uint32_t overV = 0;
    EXPECT_FALSE(bitStream.Read(overV));
}

TEST(BitStreamExtended, OverRead_EmptyStream_ReturnsFalseForUint32) {
    CBITSTREAM;
    // Nothing has been written.
    uint32_t val = 0xDEADu;
    EXPECT_FALSE(bitStream.Read(val));
}

TEST(BitStreamExtended, OverRead_EmptyStream_ReturnsFalseForBool) {
    CBITSTREAM;
    bool val = true;
    EXPECT_FALSE(bitStream.Read(val));
}

// ===========================================================================
// Mixed write/read interleaving via SetReadOffset
// ===========================================================================

TEST(BitStreamExtended, InterleaveWriteThenRewind_ReadAllValues) {
    CBITSTREAM;
    bitStream.Write<uint8_t>(10u);
    bitStream.Write<uint8_t>(20u);
    bitStream.Write<uint8_t>(30u);

    bitStream.SetReadOffset(0);

    uint8_t a = 0, b = 0, c = 0;
    ASSERT_TRUE(bitStream.Read(a));
    ASSERT_TRUE(bitStream.Read(b));
    ASSERT_TRUE(bitStream.Read(c));

    EXPECT_EQ(a, 10u);
    EXPECT_EQ(b, 20u);
    EXPECT_EQ(c, 30u);
}

// ===========================================================================
// Byte count consistency
// ===========================================================================

TEST(BitStreamExtended, GetNumberOfBytesUsed_ConsistentWithBitsUsed) {
    CBITSTREAM;
    bitStream.Write<uint32_t>(1u);
    bitStream.Write<uint32_t>(2u);
    // 64 bits = 8 bytes.
    EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 64u);
    EXPECT_EQ(bitStream.GetNumberOfBytesUsed(), 8u);
}

TEST(BitStreamExtended, GetNumberOfBytesUsed_RoundsUpForPartialByte) {
    CBITSTREAM;
    // Write one boolean — that's 1 bit, which should round up to 1 byte.
    bitStream.Write<bool>(true);
    EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 1u);
    EXPECT_EQ(bitStream.GetNumberOfBytesUsed(), 1u);
}

// ===========================================================================
// Large data integrity
// ===========================================================================

TEST(BitStreamExtended, WriteManyUint32s_ReadAllBackCorrectly) {
    CBITSTREAM;
    constexpr int N = 64;
    for (int i = 0; i < N; ++i) {
        bitStream.Write<uint32_t>(static_cast<uint32_t>(i * 13 + 7));
    }
    for (int i = 0; i < N; ++i) {
        uint32_t val = 0;
        ASSERT_TRUE(bitStream.Read(val)) << "Read failed at i=" << i;
        EXPECT_EQ(val, static_cast<uint32_t>(i * 13 + 7)) << "Mismatch at i=" << i;
    }
    EXPECT_EQ(bitStream.GetNumberOfUnreadBits(), 0u);
}

TEST(BitStreamExtended, AlternatingBoolAndUint8_RoundTrip) {
    CBITSTREAM;
    constexpr int PAIRS = 10;
    for (int i = 0; i < PAIRS; ++i) {
        bitStream.Write<bool>(i % 2 == 0);
        bitStream.Write<uint8_t>(static_cast<uint8_t>(i));
    }
    for (int i = 0; i < PAIRS; ++i) {
        bool b = false;
        uint8_t u = 0;
        ASSERT_TRUE(bitStream.Read(b));
        ASSERT_TRUE(bitStream.Read(u));
        EXPECT_EQ(b, i % 2 == 0) << "bool mismatch at i=" << i;
        EXPECT_EQ(u, static_cast<uint8_t>(i)) << "uint8 mismatch at i=" << i;
    }
}
