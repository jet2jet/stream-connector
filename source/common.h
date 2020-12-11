#pragma once

#define APP_NAME L"stream-connector"

// needs process id and any uint64-number
#define PIPE_ANONYMOUS_NAME_FORMAT L"\\\\.\\pipe\\" APP_NAME L"\\work-pipe-%08lX-%llu"
// needs id string
#define PIPE_PROXY_NAME_FORMAT L"\\\\.\\pipe\\" APP_NAME L"\\proxy-%s"
