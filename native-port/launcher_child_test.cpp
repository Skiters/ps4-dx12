// SPDX-License-Identifier: GPL-2.0-or-later
#include <filesystem>
#include <string_view>
int wmain(int argc,wchar_t** argv) {
    if(argc!=3||std::wstring_view(argv[1])!=L"--game")return 91;
    const std::filesystem::path game(argv[2]);
    if(!game.is_absolute()||game.filename()!=L"eboot with spaces.bin"||
       !std::filesystem::is_regular_file(game))return 92;
    return 37; // Launcher must wait for and propagate the child result.
}
