#ifndef ENV_ENVFS_H
#define ENV_ENVFS_H

namespace env
{

using DirStartF = void (void*, std::wstring_view);
using DirEndF = void (void*, std::wstring_view);
using FileF = void (void*, std::wstring_view, FILETIME);

void forEachEntry(
  const std::wstring& path, void* cx,
  DirStartF* dirStartF, DirEndF* dirEndF, FileF* fileF);

} // namespace

#endif // ENV_ENVFS_H
