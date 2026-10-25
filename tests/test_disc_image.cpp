#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "DiscImage.h"

// ============================================================
// Phase 4.2b: DiscImage. Every image is built synthetically in a
// temporary folder; nothing is read from ROMs/.
// ============================================================

namespace
{

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

constexpr size_t SECTOR = 2048;

struct TempDir
{
    fs::path path;
    explicit TempDir(const std::string& name)
        : path(fs::temp_directory_path() / ("ultrascan_disc_" + name))
    {
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
    }
    ~TempDir() { std::error_code ec; fs::remove_all(path, ec); }
    std::string file(const std::string& name) const { return (path / name).string(); }
};

void writeFile(const std::string& path, const Bytes& data)
{
    std::ofstream f(path, std::ios::binary);
    f.write((const char*)data.data(), (std::streamsize)data.size());
}

void put32le(Bytes& b, size_t off, uint32_t v)
{
    for (int i = 0; i < 4; ++i) b[off + i] = (uint8_t)(v >> (8 * i));
}

Bytes pattern(size_t n, uint8_t seed)
{
    Bytes d(n);
    for (size_t i = 0; i < n; ++i) d[i] = (uint8_t)(seed + i * 7);
    return d;
}

struct IsoFile { std::string name; Bytes data; };

struct IsoImage
{
    Bytes bytes;            // 2048-byte sectors
    size_t dirSectors = 0;
};

// Directory record for `name` (already including any ";1").
Bytes makeRecord(const std::string& name, uint32_t lba, uint32_t size, bool isDir)
{
    size_t len = 33 + name.size();
    if (len & 1) ++len;                      // records have even length
    Bytes r(len, 0);
    r[0] = (uint8_t)len;
    put32le(r, 2, lba);
    put32le(r, 10, size);
    r[25] = isDir ? 2 : 0;
    r[32] = (uint8_t)name.size();
    for (size_t i = 0; i < name.size(); ++i) r[33 + i] = (uint8_t)name[i];
    return r;
}

// PVD at sector 16, root directory at sector 18, file data after it.
// `fillers` extra tiny entries are inserted before the real files so the
// directory can be made to spill over into a second sector.
IsoImage buildIso(const std::vector<IsoFile>& files, int fillers = 0)
{
    const uint32_t dirLba = 18;

    // Layout pass: the directory size depends on how many records there are.
    auto layout = [&](uint32_t dataLba, Bytes& dir) {
        auto add = [&](const Bytes& rec) {
            if (dir.size() % SECTOR + rec.size() > SECTOR)
                dir.resize((dir.size() / SECTOR + 1) * SECTOR, 0);   // zero pad to next sector
            dir.insert(dir.end(), rec.begin(), rec.end());
        };
        add(makeRecord(std::string(1, '\0'), dirLba, 0, true));
        add(makeRecord(std::string(1, '\1'), dirLba, 0, true));
        for (int i = 0; i < fillers; ++i)
            add(makeRecord("FILLER" + std::to_string(i) + ".DAT;1", dataLba, 1, false));
        uint32_t lba = dataLba;
        for (const auto& f : files)
        {
            add(makeRecord(f.name, lba, (uint32_t)f.data.size(), false));
            lba += (uint32_t)((f.data.size() + SECTOR - 1) / SECTOR);
        }
        dir.resize((dir.size() + SECTOR - 1) / SECTOR * SECTOR, 0);
    };

    Bytes dir;
    layout(dirLba + 1, dir);                          // first guess: 1 dir sector
    size_t dirSectors = dir.size() / SECTOR;
    dir.clear();
    layout(dirLba + (uint32_t)dirSectors, dir);       // real data start
    dirSectors = dir.size() / SECTOR;

    IsoImage img;
    img.dirSectors = dirSectors;
    img.bytes.assign((dirLba + dirSectors) * SECTOR, 0);

    // PVD
    uint8_t* pvd = &img.bytes[16 * SECTOR];
    pvd[0] = 1;
    std::memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 1;
    Bytes root = makeRecord(std::string(1, '\0'), dirLba, (uint32_t)dir.size(), true);
    root[0] = 34;
    std::memcpy(pvd + 156, root.data(), 34);

    std::memcpy(&img.bytes[dirLba * SECTOR], dir.data(), dir.size());

    for (const auto& f : files)
    {
        size_t start = img.bytes.size();
        size_t padded = (f.data.size() + SECTOR - 1) / SECTOR * SECTOR;
        img.bytes.resize(start + padded, 0);
        std::memcpy(&img.bytes[start], f.data.data(), f.data.size());
    }
    return img;
}

// Re-pack 2048-byte sectors as MODE1/2352 (16-byte header, 288-byte trailer).
Bytes toRaw2352(const Bytes& iso)
{
    Bytes out;
    for (size_t s = 0; s < iso.size() / SECTOR; ++s)
    {
        out.insert(out.end(), 16, 0xAA);
        out.insert(out.end(), iso.begin() + s * SECTOR, iso.begin() + (s + 1) * SECTOR);
        out.insert(out.end(), 288, 0xBB);
    }
    return out;
}

std::vector<IsoFile> standardFiles()
{
    return {
        { "HYPER.EXE;1",  pattern(5000, 3) },
        { "README.TXT;1", pattern(100, 40) },
    };
}

} // namespace

// ---- ISO -----------------------------------------------------------------
TEST(DiscImage, IsoReadsFileByName)
{
    TempDir d("iso");
    auto files = standardFiles();
    writeFile(d.file("disc.iso"), buildIso(files).bytes);

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("disc.iso"))) << img.error();
    EXPECT_FALSE(img.isBare());

    Bytes out;
    ASSERT_TRUE(img.readFile("HYPER.EXE", out)) << img.error();
    EXPECT_EQ(out, files[0].data);
    ASSERT_TRUE(img.readFile("README.TXT", out)) << img.error();
    EXPECT_EQ(out, files[1].data);
}

TEST(DiscImage, LookupIsCaseInsensitiveAndIgnoresVersionSuffix)
{
    TempDir d("case");
    auto files = standardFiles();
    writeFile(d.file("disc.iso"), buildIso(files).bytes);

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("disc.iso")));
    Bytes out;
    EXPECT_TRUE(img.readFile("hyper.exe", out));
    EXPECT_EQ(out, files[0].data);
    EXPECT_TRUE(img.readFile("Hyper.Exe;1", out));
    EXPECT_EQ(out, files[0].data);
}

TEST(DiscImage, MissingFileReturnsFalseWithError)
{
    TempDir d("missing");
    writeFile(d.file("disc.iso"), buildIso(standardFiles()).bytes);

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("disc.iso")));
    Bytes out;
    EXPECT_FALSE(img.readFile("NOPE.BIN", out));
    EXPECT_FALSE(img.error().empty());
    EXPECT_TRUE(out.empty());
}

TEST(DiscImage, ErrorIsClearedByASuccessfulCall)
{
    TempDir d("errclear");
    writeFile(d.file("disc.iso"), buildIso(standardFiles()).bytes);

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("disc.iso")));
    Bytes out;
    EXPECT_FALSE(img.readFile("NOPE.BIN", out));
    EXPECT_TRUE(img.readFile("HYPER.EXE", out));
    EXPECT_TRUE(img.error().empty());
}

TEST(DiscImage, DirectorySpanningTwoSectors)
{
    TempDir d("dir2");
    auto files = standardFiles();
    IsoImage iso = buildIso(files, /*fillers=*/60);
    ASSERT_EQ(iso.dirSectors, 2u);                    // the test setup really spills over
    writeFile(d.file("disc.iso"), iso.bytes);

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("disc.iso")));
    Bytes out;
    ASSERT_TRUE(img.readFile("README.TXT", out)) << img.error();   // lives in sector 2 of the dir
    EXPECT_EQ(out, files[1].data);
    ASSERT_TRUE(img.readFile("HYPER.EXE", out)) << img.error();
    EXPECT_EQ(out, files[0].data);
}

// ---- cue / bin -----------------------------------------------------------
TEST(DiscImage, CueWithMode1_2352)
{
    TempDir d("cue2352");
    auto files = standardFiles();
    writeFile(d.file("disc.bin"), toRaw2352(buildIso(files).bytes));
    std::string cue = "FILE \"disc.bin\" BINARY\r\n"
                      "  TRACK 01 MODE1/2352\r\n"
                      "    INDEX 01 00:00:00\r\n";
    writeFile(d.file("disc.cue"), Bytes(cue.begin(), cue.end()));

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("disc.cue"))) << img.error();
    Bytes out;
    ASSERT_TRUE(img.readFile("HYPER.EXE", out)) << img.error();
    EXPECT_EQ(out, files[0].data);
    ASSERT_TRUE(img.readFile("readme.txt", out)) << img.error();
    EXPECT_EQ(out, files[1].data);
}

TEST(DiscImage, CueWithMode1_2048AndUnquotedName)
{
    TempDir d("cue2048");
    auto files = standardFiles();
    writeFile(d.file("plain.bin"), buildIso(files).bytes);
    std::string cue = "FILE plain.bin BINARY\n  TRACK 01 MODE1/2048\n    INDEX 01 00:00:00\n";
    writeFile(d.file("plain.cue"), Bytes(cue.begin(), cue.end()));

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("plain.cue"))) << img.error();
    Bytes out;
    ASSERT_TRUE(img.readFile("HYPER.EXE", out)) << img.error();
    EXPECT_EQ(out, files[0].data);
}

TEST(DiscImage, CueDirectorySpanningTwoSectorsIn2352)
{
    TempDir d("cuedir2");
    auto files = standardFiles();
    writeFile(d.file("disc.bin"), toRaw2352(buildIso(files, 60).bytes));
    std::string cue = "FILE \"disc.bin\" BINARY\n TRACK 01 MODE1/2352\n";
    writeFile(d.file("disc.cue"), Bytes(cue.begin(), cue.end()));

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("disc.cue"))) << img.error();
    Bytes out;
    ASSERT_TRUE(img.readFile("README.TXT", out)) << img.error();
    EXPECT_EQ(out, files[1].data);
}

TEST(DiscImage, CueRejectsAudioUnknownModeAndMissingParts)
{
    TempDir d("cuebad");
    writeFile(d.file("disc.bin"), toRaw2352(buildIso(standardFiles()).bytes));

    auto tryCue = [&](const std::string& text, std::string& error) {
        writeFile(d.file("t.cue"), Bytes(text.begin(), text.end()));
        DiscImage img;
        bool ok = img.open(d.file("t.cue"));
        error = img.error();
        return ok;
    };

    std::string err;
    EXPECT_FALSE(tryCue("FILE \"disc.bin\" BINARY\n TRACK 01 AUDIO\n", err));
    EXPECT_NE(err.find("audio"), std::string::npos);

    EXPECT_FALSE(tryCue("FILE \"disc.bin\" BINARY\n TRACK 01 MODE2/2352\n", err));
    EXPECT_FALSE(err.empty());

    EXPECT_FALSE(tryCue("FILE \"disc.bin\" WAVE\n TRACK 01 MODE1/2352\n", err));
    EXPECT_FALSE(tryCue("TRACK 01 MODE1/2352\n", err));
    EXPECT_FALSE(tryCue("FILE \"disc.bin\" BINARY\n", err));
    EXPECT_FALSE(tryCue("", err));
    EXPECT_FALSE(tryCue("FILE \"no_such.bin\" BINARY\n TRACK 01 MODE1/2352\n", err));
    EXPECT_FALSE(err.empty());
}

TEST(DiscImage, MissingImageFailsToOpen)
{
    DiscImage img;
    EXPECT_FALSE(img.open("this/path/does/not/exist.iso"));
    EXPECT_FALSE(img.error().empty());
    Bytes out;
    EXPECT_FALSE(img.readFile("HYPER.EXE", out));
}

// ---- Malformed images ----------------------------------------------------
TEST(DiscImage, BadVolumeSignatureReturnsFalse)
{
    TempDir d("badsig");
    IsoImage iso = buildIso(standardFiles());
    iso.bytes[16 * SECTOR + 1] = 'X';
    writeFile(d.file("disc.iso"), iso.bytes);

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("disc.iso")));
    Bytes out;
    EXPECT_FALSE(img.readFile("HYPER.EXE", out));
    EXPECT_FALSE(img.error().empty());
}

TEST(DiscImage, TruncatedImagesReturnFalse)
{
    TempDir d("trunc");
    IsoImage iso = buildIso(standardFiles());

    // Cut in the middle of HYPER.EXE's data.
    Bytes cut(iso.bytes.begin(), iso.bytes.begin() + 20 * SECTOR + 100);
    writeFile(d.file("cut.iso"), cut);
    {
        DiscImage img;
        ASSERT_TRUE(img.open(d.file("cut.iso")));
        Bytes out;
        EXPECT_FALSE(img.readFile("HYPER.EXE", out));
        EXPECT_FALSE(img.error().empty());
        EXPECT_TRUE(out.empty());
    }

    // Too short to even hold the volume descriptor.
    Bytes tiny(iso.bytes.begin(), iso.bytes.begin() + 5 * SECTOR);
    writeFile(d.file("tiny.iso"), tiny);
    {
        DiscImage img;
        ASSERT_TRUE(img.open(d.file("tiny.iso")));
        Bytes out;
        EXPECT_FALSE(img.readFile("HYPER.EXE", out));
    }
}

TEST(DiscImage, CorruptRootDirectoryFieldsReturnFalse)
{
    TempDir d("corrupt");
    IsoImage good = buildIso(standardFiles());
    const size_t rootRec = 16 * SECTOR + 156;
    Bytes out;

    {   // absurd directory size
        IsoImage iso = good;
        put32le(iso.bytes, rootRec + 10, 0x7FFFFFFFu);
        writeFile(d.file("a.iso"), iso.bytes);
        DiscImage img;
        ASSERT_TRUE(img.open(d.file("a.iso")));
        EXPECT_FALSE(img.readFile("HYPER.EXE", out));
    }
    {   // directory extent past the end of the image
        IsoImage iso = good;
        put32le(iso.bytes, rootRec + 2, 99999u);
        writeFile(d.file("b.iso"), iso.bytes);
        DiscImage img;
        ASSERT_TRUE(img.open(d.file("b.iso")));
        EXPECT_FALSE(img.readFile("HYPER.EXE", out));
    }
    {   // root record is not a directory
        IsoImage iso = good;
        iso.bytes[rootRec + 25] = 0;
        writeFile(d.file("c.iso"), iso.bytes);
        DiscImage img;
        ASSERT_TRUE(img.open(d.file("c.iso")));
        EXPECT_FALSE(img.readFile("HYPER.EXE", out));
    }
    {   // a file record whose name length overruns its record
        IsoImage iso = good;
        const size_t dir = 18 * SECTOR;
        size_t pos = 0;
        pos += iso.bytes[dir + pos];                 // "."
        pos += iso.bytes[dir + pos];                 // ".."
        iso.bytes[dir + pos + 32] = 250;             // first real record
        writeFile(d.file("d.iso"), iso.bytes);
        DiscImage img;
        ASSERT_TRUE(img.open(d.file("d.iso")));
        EXPECT_FALSE(img.readFile("HYPER.EXE", out));
    }
    {   // a file whose extent is past the end of the image
        IsoImage iso = good;
        const size_t dir = 18 * SECTOR;
        size_t pos = 0;
        pos += iso.bytes[dir + pos];
        pos += iso.bytes[dir + pos];
        put32le(iso.bytes, dir + pos + 2, 50000u);
        writeFile(d.file("e.iso"), iso.bytes);
        DiscImage img;
        ASSERT_TRUE(img.open(d.file("e.iso")));
        EXPECT_FALSE(img.readFile("HYPER.EXE", out));
        EXPECT_TRUE(out.empty());
    }
}

// ---- Bare file -----------------------------------------------------------
TEST(DiscImage, BareFileReturnsItsContentsForAnyName)
{
    TempDir d("bare");
    Bytes data = pattern(3820, 9);
    writeFile(d.file("HYPER.EXE"), data);

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("HYPER.EXE"))) << img.error();
    EXPECT_TRUE(img.isBare());

    Bytes out;
    ASSERT_TRUE(img.readFile("HYPER.EXE", out)) << img.error();
    EXPECT_EQ(out, data);
    ASSERT_TRUE(img.readFile("anything-else", out)) << img.error();
    EXPECT_EQ(out, data);
}

TEST(DiscImage, EmptyBareFileReturnsFalse)
{
    TempDir d("bareempty");
    writeFile(d.file("empty.bin"), {});

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("empty.bin")));
    Bytes out;
    EXPECT_FALSE(img.readFile("HYPER.EXE", out));
    EXPECT_FALSE(img.error().empty());
}

TEST(DiscImage, ReopeningResetsState)
{
    TempDir d("reopen");
    Bytes data = pattern(64, 1);
    writeFile(d.file("a.bin"), data);
    writeFile(d.file("disc.iso"), buildIso(standardFiles()).bytes);

    DiscImage img;
    ASSERT_TRUE(img.open(d.file("a.bin")));
    EXPECT_TRUE(img.isBare());
    ASSERT_TRUE(img.open(d.file("disc.iso")));
    EXPECT_FALSE(img.isBare());
    Bytes out;
    EXPECT_TRUE(img.readFile("HYPER.EXE", out));
    EXPECT_FALSE(img.open(d.file("missing.iso")));
    EXPECT_FALSE(img.readFile("HYPER.EXE", out));
}
