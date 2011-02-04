#ifndef NODE_PLATFORM_WIN32_FS_H_
#define NODE_PLATFORM_WIN32_FS_H_

#include <platform_win32.h>

namespace node {

void win_fs_init();
int win_symlink(WCHAR *target, WCHAR *path, bool junction, bool target_is_dir);

} // namespace node

#endif  // NODE_PLATFORM_WIN32_FS_H_
