#pragma once

// Texture mods (SafeHouse's docs/MODDING.md, "Sly 1 / Textures"). Every texture a level loads has a name,
// "<W>x<H>_<hash>.png", where the hash is FNV-1a 64 of its decoded RGBA (level 0, as the level stores it). A PNG of that
// name in <mods>/Textures (or the texture pack, --texture-pack) replaces the texture; --dump-textures DIR writes each
// texture the game loads under that name.
// SafeHouse builds only (CANE_SAFEHOUSE, as mods.h); elsewhere no texture is replaced.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct BMP;

#if !defined(CANE_SAFEHOUSE)
inline bool FTextureReplacement(const BMP&, std::vector<uint8_t>&, uint32_t&, uint32_t&) { return false; }
inline void ReportTextureMods() {}
#else

// --dump-textures DIR: write every texture the game loads to DIR (files already there are kept).
void SetTextureDumpDirectory(const std::filesystem::path& directory);
// --dump-textures DIR LEVEL: the game goes straight to LEVEL (its folder name, e.g. "A Stealthy Approach", or its id in
// hex, e.g. 103), writes the textures it loaded and quits. Called with the level's name or id from the command line.
void SetTextureDumpLevel(const std::string& level);
// main.cpp, once a frame after the world's update: drives --dump-textures DIR LEVEL (the warp, then the quit).
void ShUpdateTextureDump();
// --texture-pack DIR: a texture pack (DIR/Textures.pak, else DIR/Textures; the same names) under the Mods folder's own
// textures, which win. The launcher passes the PS3 HD pack this way while it is turned on (ps3hd.cpp).
void SetTexturePackDirectory(const std::filesystem::path& directory);
bool FTextureModsActive();
#include <vector>
// Called by LoadTexturesFromBrx for each image once its bytes are read (image.diffuseTexture). Dumps the texture
// when dumping. Returns true with the replacement's RGBA (rows as the level stores them) and size when the mods
// folder replaces it. A replacement whose pixels equal the original's is not used: the original texture (with
// its own format and mip chain) stays, so unchanged dumps render exactly as before.
// Prints how many textures of the level just loaded were replaced (and kept as unchanged), when mods have any.
void ReportTextureMods();
bool FTextureReplacement(const BMP& image, std::vector<uint8_t>& rgba, uint32_t& width, uint32_t& height);
// An image's level 0 as RGBA8 (rows as the level stores them) and its mods name (ps3hd.cpp names its PNGs so).
bool DecodeTextureLevel0(const BMP& image, std::vector<uint8_t>& rgba);
#include <string>
std::string TextureModName(const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height);
// --ps3-hd-textures SOURCE OUT (ps3hd.cpp): The Sly Collection's (PS3) textures over this installation's, written as
// a texture pack (OUT/Textures.pak; png, --ps3-hd-png: OUT/Textures, the PNG files). SOURCE: the extracted disc, the
// .iso or a .zip of it.
int Ps3HdTexturesCommand(const std::filesystem::path& installation, const std::string& source, const std::string& out, bool png);

#endif
