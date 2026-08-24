#include "ZipCrcReader.h"

#include <iostream>
#include <fstream>
#include <vector>
//#include <iomanip>
#include <cstring>
#include <algorithm>

// Binary structure mapping to the End of Central Directory (EOCD) record
#pragma pack(push, 1)
struct EndOfCentralDirectory {
    uint32_t signature;          // Expected: 0x06054b50
    uint16_t diskNumber;
    uint16_t startDisk;
    uint16_t totalEntriesDisk;
    uint16_t totalEntries;
    uint32_t centralDirSize;
    uint32_t centralDirOffset;
    uint16_t commentLength;
};

// Binary structure mapping to a single Central Directory header entry
struct CentralDirHeader {
    uint32_t signature;          // Expected: 0x02014b50
    uint16_t versionMadeBy;
    uint16_t versionNeeded;
    uint16_t generalPurposeBit;
    uint16_t compressionMethod;
    uint16_t lastModTime;
    uint16_t lastModDate;
    uint32_t crc32;              // Guaranteed valid CRC32
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
    uint16_t fileCommentLength;
    uint16_t diskNumberStart;
    uint16_t internalAttr;
    uint32_t externalAttr;
    uint32_t localHeaderOffset;
};
#pragma pack(pop)

uint32_t getCRC32FromZipCentralDirectory(const std::string& zipPath, const std::string& targetFileName) {
    std::ifstream file(zipPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open ZIP file." << std::endl;
        return 0;
    }

    const std::streamsize fileSize = file.tellg();
    if (fileSize < static_cast<std::streamsize>(sizeof(EndOfCentralDirectory))) {
        std::cerr << "Error: File size too small for a valid ZIP archive." << std::endl;
        return 0;
    }

    // 1. Locate the End of Central Directory (EOCD) record by searching backward from the end.
    // The EOCD record may be followed by a variable-length archive comment (up to 65535 bytes).
    const size_t maxSearchSize = std::min(static_cast<size_t>(fileSize), sizeof(EndOfCentralDirectory) + 65535);
    std::vector<char> buffer(maxSearchSize);

    file.seekg(fileSize - maxSearchSize, std::ios::beg);
    file.read(buffer.data(), maxSearchSize);

    EndOfCentralDirectory eocd{};
    bool foundEocd = false;

    // Scan backwards byte-by-byte for the EOCD signature (0x06054b50)
    for (size_t i = maxSearchSize - sizeof(EndOfCentralDirectory); ; --i) {
        uint32_t sig;
        std::memcpy(&sig, &buffer[i], sizeof(uint32_t));
        if (sig == 0x06054b50) {
            std::memcpy(&eocd, &buffer[i], sizeof(EndOfCentralDirectory));
            foundEocd = true;
            break;
        }
        if (i == 0) break;
    }

    if (!foundEocd) {
        std::cerr << "Error: End of Central Directory signature not found (corrupted file or ZIP64)." << std::endl;
        return 0;
    }

    // 2. Jump to the start of the Central Directory
    file.seekg(eocd.centralDirOffset, std::ios::beg);

    // 3. Iterate through all entries in the Central Directory
    for (uint16_t i = 0; i < eocd.totalEntries; ++i) {
        CentralDirHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.signature != 0x02014b50) {
            std::cerr << "Error: Invalid Central Directory header signature." << std::endl;
            return 0;
        }

        // Read entry filename
        std::string fileName(header.fileNameLength, '\0');
        file.read(&fileName[0], header.fileNameLength);

        // Check for match
        if (fileName == targetFileName) {
            return header.crc32;
        }

        // Skip extra fields and file comments to position at the next entry header
        file.seekg(header.extraFieldLength + header.fileCommentLength, std::ios::cur);
    }

    std::cerr << "Error: Target file not found in ZIP archive." << std::endl;
    return 0;
}
