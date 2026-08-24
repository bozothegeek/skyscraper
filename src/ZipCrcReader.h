#ifndef ZIP_CRC_READER_H
#define ZIP_CRC_READER_H

#include <string>
#include <cstdint>

// Reads the CRC32 checksum of a specific file inside a ZIP archive.
// Uses the Central Directory to handle archives generated with Data Descriptors.
//
// @param zipPath        Path to the target .zip file.
// @param targetFileName Internal path/filename within the ZIP archive (e.g., "folder/file.txt").
// @return uint32_t      The 32-bit CRC checksum value, or 0 if reading fails/file is not found.
uint32_t getCRC32FromZipCentralDirectory(const std::string& zipPath, const std::string& targetFileName);

#endif // ZIP_CRC_READER_H
