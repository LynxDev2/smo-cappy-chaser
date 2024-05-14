#pragma once

#include <prim/seadSafeString.h>
#include <filedevice/seadArchiveFileDevice.h>

namespace al {
class IAudioResourceLoader;
class Resource;
bool isExistFile(const sead::SafeString& fileName);
bool isExistDirectory(const sead::SafeString& fileName);
bool isExistArchive(const sead::SafeString& fileName);
bool isExistArchive(const sead::SafeString& fileName, const char* ext);
u32 getFileSize(const sead::SafeString& fileName);
u32 calcFileAlignment(const sead::SafeString& fileName);
u32 calcBufferSizeAlignment(const sead::SafeString& fileName);
u8* loadFile(const sead::SafeString& fileName);
void tryLoadFileToBuffer(const sead::SafeString& fileName, u8*, u32, s32);
sead::ArchiveRes* loadArchive(const sead::SafeString& fileName);
void loadArchiveWithExt(const sead::SafeString& fileName, char const* ext);
void tryRequestLoadArchive(const sead::SafeString& fileName, sead::Heap* heap);
void loadSoundItem(u32, u32, al::IAudioResourceLoader* resLoader);
void tryRequestLoadSoundItem(u32);
void tryRequestPreLoadFile(const al::Resource* res, s32, sead::Heap* heap, al::IAudioResourceLoader* resLoader);
void tryRequestPreLoadFile(const al::Resource* res, const sead::SafeString& fileName, sead::Heap* heap,
                           al::IAudioResourceLoader* resLoader);
void waitLoadDoneAllFile();
void clearFileLoaderEntry();
void makeLocalizedArchivePath(sead::BufferedSafeString* outPath,
                              const sead::SafeString& fileName);
void makeLocalizedArchivePathByCountryCode(sead::BufferedSafeString* outPath,
                                           const sead::SafeString& fileName);
void setFileLoaderThreadPriority(s32 priority);
}  // namespace al
