// Read-only access to the game's PAK_ v4 archives, enough to regenerate kiaccess\data\strings_en.tsv on first run
// from PAK\DX11\GLOBAL.PAK (record `gameinfo\strings\strings`, type 26) instead of shipping the game's text.
// Port of 05_tools/pak_table.py (record = char name[240] + u8 flags[4] + u64 offset + u32 size + u16 ext + u16 a + u16 b)
// and 05_tools/strings_decode.py (u32 version=1, u32 count, u32 table_off=16, u32 blob_off, count x {u32 hash, u32 str_off}, strings).
#pragma once
#include <cstdint>
#include <string>

namespace ki {
namespace pak {

// Finds the record called `name` (exact, backslash separators, no extension) and reads its data.
bool readEntry(const std::wstring& pakPath, const std::string& name, std::string& out, std::string& err);
// Decodes a type-26 localization table into the TSV the narrator loads (columns hash, offset, text; LF line ends;
// tab / newline / carriage return in a text escaped as backslash t, n, r - the format 05_tools/strings_decode.py writes).
bool decodeStringTable(const std::string& blob, std::string& tsv, int& count, std::string& err);
// readEntry + decodeStringTable + write the file.
bool generateStringsTsv(const std::wstring& pakPath, const std::wstring& outTsv, int& count, std::string& err);

}  // namespace pak
}  // namespace ki
