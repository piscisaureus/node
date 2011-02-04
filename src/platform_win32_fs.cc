
#include <platform_win32.h>
#include <platform_win32_fs.h>

#include <errno.h>
#include <winioctl.h>

#ifndef SYMLINK_FLAG_RELATIVE
# define SYMLINK_FLAG_RELATIVE 1
#endif

#define IS_SLASH(c) ((c) == L'\\' || (c) == L'/')
#define IS_LETTER(c) (((c) >= L'a' && (c) <= L'z') || ((c) >= L'A' && (c) <= L'Z'))

namespace node {

// Retrieve dynamically so it will not break XP compatibility
BOOLEAN (WINAPI *pCreateSymbolicLinkW)(WCHAR* path, WCHAR* target,
    DWORD flags) = NULL;

const WCHAR JUNCTION_PREFIX[] = L"\\??\\";
const WCHAR JUNCTION_PREFIX_LEN = 4;

static inline int win_create_junction(WCHAR *target, WCHAR *path) {

  HANDLE handle = INVALID_HANDLE_VALUE;
  bool created = false;

  REPARSE_DATA_BUFFER *buffer = NULL;

  int target_len;
  bool target_is_supported;

  int needed_buf_size, used_buf_size, used_data_size, path_buf_len;

  int start, len, i;
  bool add_slash;

  DWORD bytes;
  WCHAR *path_buf;

  // Fetch the length of the symlink target path
  target_len = wcslen(target);

  // Check if the path is an absolute path
  target_is_supported = target_len >= 3 && IS_LETTER(target[0]) &&
      target[1] == L':' && IS_SLASH(target[2]);

  if (!target_is_supported) {
    SetLastError(EINVAL);
    goto error;
  }

  // Do a pessimistic calculation of the required buffer size
  needed_buf_size =
      FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer) +
      JUNCTION_PREFIX_LEN * sizeof(WCHAR) +
      2 * (target_len + 2) * sizeof(WCHAR);

  // Allocate the buffer
  buffer = (REPARSE_DATA_BUFFER*)malloc(needed_buf_size);
  if (!buffer) {
    SetLastError(ENOMEM);
    goto error;
  }

  // Grab a pointer to the part of the buffer where filenames go
  path_buf = (WCHAR*)&(buffer->MountPointReparseBuffer.PathBuffer);
  path_buf_len = 0;

  // Copy the substitute (internal) target path
  start = path_buf_len;
  wcsncpy((WCHAR*)&path_buf[path_buf_len], JUNCTION_PREFIX, JUNCTION_PREFIX_LEN);
  path_buf_len += JUNCTION_PREFIX_LEN;
  add_slash = false;
  for (i = 0; target[i]; i++) {
    if (IS_SLASH(target[i])) {
      add_slash = true;
      continue;
    }

    if (add_slash) {
      path_buf[path_buf_len++] = L'\\';
      add_slash = false;
    }

    path_buf[path_buf_len++] = target[i];
  }
  path_buf[path_buf_len++] = L'\\';
  len = path_buf_len - start;

  // Set the info about the substitute name
  buffer->MountPointReparseBuffer.SubstituteNameOffset = start * sizeof(WCHAR);
  buffer->MountPointReparseBuffer.SubstituteNameLength = len * sizeof(WCHAR);

  // Insert null terminator
  path_buf[path_buf_len++] = L'\0';

  // Copy the print name of the target path
  start = path_buf_len;
  add_slash = false;
  for (i = 0; target[i]; i++) {
    if (IS_SLASH(target[i])) {
      add_slash = true;
      continue;
    }

    if (add_slash) {
      path_buf[path_buf_len++] = L'\\';
      add_slash = false;
    }

    path_buf[path_buf_len++] = target[i];
  }
  len = path_buf_len - start;
  if (len == 2) {
    path_buf[path_buf_len++] = L'\\';
    len++;
  }

  // Set the info about the print name
  buffer->MountPointReparseBuffer.PrintNameOffset = start * sizeof(WCHAR);
  buffer->MountPointReparseBuffer.PrintNameLength = len * sizeof(WCHAR);

  // Insert another null terminator
  path_buf[path_buf_len++] = L'\0';

  // Calculate how much buffer space was actually used
  used_buf_size = FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer) +
        path_buf_len * sizeof(WCHAR);
  used_data_size = used_buf_size -
      FIELD_OFFSET(REPARSE_DATA_BUFFER, MountPointReparseBuffer);

  // Put general info in the data buffer
  buffer->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
  buffer->ReparseDataLength = used_data_size;
  buffer->Reserved = 0;

  // Create a new directory
  if (!CreateDirectoryW(path, NULL))
    goto error;
  created = true;

  // Open the directory
  handle = CreateFileW(path, GENERIC_ALL, 0, NULL, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  if (handle == INVALID_HANDLE_VALUE)
    goto error;

  // Create the actual reparse point
  if (!DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, buffer, used_buf_size,
      NULL, 0, &bytes, NULL))
    goto error;

  // Clean up
  CloseHandle(handle);
  free(buffer);

  return 0;

error:
  errno = GetLastError();

  if (buffer != NULL)
    free(buffer);

  if (handle != INVALID_HANDLE_VALUE)
    CloseHandle(handle);

  if (created)
    RemoveDirectoryW(path);

  return -1;
}

static inline int win_create_symlink(WCHAR *target, WCHAR *path,
    bool target_is_dir) {
  if (!pCreateSymbolicLinkW) {
    errno = ENOSYS;
    return -1;
  }

  DWORD flags = 0;
  if (target_is_dir)
    flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;

  int result = pCreateSymbolicLinkW(path, target, flags) ? 0 : -1;
  errno = GetLastError();

  return result;
}

int win_symlink(WCHAR *target, WCHAR *path, bool junction, bool target_is_dir) {
  if (junction) {
    return win_create_junction(target, path);
  } else {
    return win_create_symlink(target, path, target_is_dir);
  }
}

template <typename T>
static inline void win_get_function_pointer(T& ptr, const char *library, const char *name) {
  HINSTANCE library_handle = LoadLibraryA(library);
  if (library_handle == NULL) {
    ptr = NULL;
  } else {
    ptr = (T)GetProcAddress(library_handle, name);
  }
}

void win_fs_init() {
  win_get_function_pointer(pCreateSymbolicLinkW, "kernel32", "CreateSymbolicLinkW");
}

} // namespace node
